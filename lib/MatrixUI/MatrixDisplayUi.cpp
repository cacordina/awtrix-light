/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 * Copyright (c) 2023 by Stephan Muehl (Blueforcer)
 * Note: This old lib for SSD1306 displays has been extremly
 * modified for AWTRIX Light and has nothing to do with the original purposes.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "MatrixDisplayUi.h"
#include "AwtrixFont.h"

MatrixDisplayUi::MatrixDisplayUi(FastLED_NeoMatrix *matrix)
{
  this->matrix = matrix;
}

void MatrixDisplayUi::init()
{
  this->matrix->begin();
  this->matrix->setTextWrap(false);
  this->matrix->setBrightness(70);
  this->matrix->setFont(&AwtrixFont);
}

void MatrixDisplayUi::setTargetFPS(uint8_t fps)
{
  float oldInterval = this->updateInterval;
  this->updateInterval = ((float)1.0 / (float)fps) * 1000;

  // Calculate new ticksPerFrame
  float changeRatio = oldInterval / (float)this->updateInterval;
  this->ticksPerFrame *= changeRatio;
  this->ticksPerTransition *= changeRatio;
}

// -/------ Automatic controll ------\-

void MatrixDisplayUi::enablesetAutoTransition()
{
  this->setAutoTransition = true;
}
void MatrixDisplayUi::disablesetAutoTransition()
{
  this->setAutoTransition = false;
}
void MatrixDisplayUi::setsetAutoTransitionForwards()
{
  this->state.frameTransitionDirection = 1;
  this->lastTransitionDirection = 1;
}
void MatrixDisplayUi::setsetAutoTransitionBackwards()
{
  this->state.frameTransitionDirection = -1;
  this->lastTransitionDirection = -1;
}
void MatrixDisplayUi::setTimePerApp(uint16_t time)
{
  this->ticksPerFrame = (int)((float)time / (float)updateInterval);
}
void MatrixDisplayUi::setTimePerTransition(uint16_t time)
{
  this->ticksPerTransition = (int)((float)time / (float)updateInterval);
}

// -/----- Frame settings -----\-
void MatrixDisplayUi::setAppAnimation(AnimationDirection dir)
{
  this->frameAnimationDirection = dir;
}

void MatrixDisplayUi::setApps(const std::vector<std::pair<String, AppCallback>> &appPairs)
{
  delete[] AppFunctions;
  AppCount = appPairs.size();
  AppFunctions = new AppCallback[AppCount];

  for (size_t i = 0; i < AppCount; ++i)
  {
    AppFunctions[i] = appPairs[i].second;
  }

  this->resetState();
}

// -/----- Overlays ------\-
void MatrixDisplayUi::setOverlays(OverlayCallback *overlayFunctions, uint8_t overlayCount)
{
  this->overlayFunctions = overlayFunctions;
  this->overlayCount = overlayCount;
}

// -/----- Manuel control -----\-
void MatrixDisplayUi::nextApp()
{
  if (this->state.frameState != IN_TRANSITION)
  {
    this->state.manuelControll = true;
    this->state.frameState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.frameTransitionDirection;
    this->state.frameTransitionDirection = 1;
  }
}
void MatrixDisplayUi::previousApp()
{
  if (this->state.frameState != IN_TRANSITION)
  {
    this->state.manuelControll = true;
    this->state.frameState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.frameTransitionDirection;
    this->state.frameTransitionDirection = -1;
  }
}

void MatrixDisplayUi::switchToApp(uint8_t frame)
{
  if (frame >= this->AppCount)
    return;
  this->state.ticksSinceLastStateSwitch = 0;
  if (frame == this->state.currentFrame)
    return;
  this->state.frameState = FIXED;
  this->state.currentFrame = frame;
}

void MatrixDisplayUi::transitionToApp(uint8_t frame)
{
  if (frame >= this->AppCount)
    return;
  this->state.ticksSinceLastStateSwitch = 0;
  if (frame == this->state.currentFrame)
    return;
  this->nextAppNumber = frame;
  this->lastTransitionDirection = this->state.frameTransitionDirection;
  this->state.manuelControll = true;
  this->state.frameState = IN_TRANSITION;

  this->state.frameTransitionDirection = frame < this->state.currentFrame ? -1 : 1;
}

// -/----- State information -----\-
MatrixDisplayUiState *MatrixDisplayUi::getUiState()
{
  return &this->state;
}

int8_t MatrixDisplayUi::update()
{
  long frameStart = millis();
  int8_t timeBudget = this->updateInterval - (frameStart - this->state.lastUpdate);
  if (timeBudget <= 0)
  {
    // Implement frame skipping to ensure time budget is keept
    if (this->setAutoTransition && this->state.lastUpdate != 0)
      this->state.ticksSinceLastStateSwitch += ceil(-timeBudget / this->updateInterval);

    this->state.lastUpdate = frameStart;
    this->tick();
  }
  return this->updateInterval - (millis() - frameStart);
}

void MatrixDisplayUi::tick()
{
  this->state.ticksSinceLastStateSwitch++;

  if (this->AppCount > 0)
  {
    switch (this->state.frameState)
    {
    case IN_TRANSITION:
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerTransition)
      {
        this->state.frameState = FIXED;
        this->state.currentFrame = getnextAppNumber();
        this->state.ticksSinceLastStateSwitch = 0;
        this->nextAppNumber = -1;
      }
      break;
    case FIXED:
      // Revert manuelControll
      if (this->state.manuelControll)
      {
        this->state.frameTransitionDirection = this->lastTransitionDirection;
        this->state.manuelControll = false;
      }
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerFrame)
      {
        if (this->setAutoTransition)
        {
          this->state.frameState = IN_TRANSITION;
        }
        this->state.ticksSinceLastStateSwitch = 0;
      }
      break;
    }
  }

  this->matrix->clear();
  if (this->AppCount > 0)
    this->drawApp();
  this->drawOverlays();
  this->matrix->show();
}

void MatrixDisplayUi::drawApp()
{
  switch (this->state.frameState)
  {
  case IN_TRANSITION:
  {
    float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
    int16_t x, y, x1, y1;
    switch (this->frameAnimationDirection)
    {
    case SLIDE_LEFT:
      x = -32 * progress;
      y = 0;
      x1 = x + 32;
      y1 = 0;
      break;
    case SLIDE_RIGHT:
      x = 32 * progress;
      y = 0;
      x1 = x - 32;
      y1 = 0;
      break;
    case SLIDE_UP:
      x = 0;
      y = -8 * progress;
      x1 = 0;
      y1 = y + 8;
      break;
    case SLIDE_DOWN:
      x = 0;
      y = 8 * progress;
      x1 = 0;
      y1 = y - 8;
      break;
    }
    // Invert animation if direction is reversed.
    int8_t dir = this->state.frameTransitionDirection >= 0 ? 1 : -1;
    x *= dir;
    y *= dir;
    x1 *= dir;
    y1 *= dir;
    bool FirstFrame = progress < 0.2;
    bool LastFrame = progress > 0.8;
    this->matrix->drawRect(x, y, x1, y1, matrix->Color(0, 0, 0));
    (this->AppFunctions[this->state.currentFrame])(this->matrix, &this->state, x, y, FirstFrame, LastFrame);
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, x1, y1, FirstFrame, LastFrame);
    break;
  }
  case FIXED:
    (this->AppFunctions[this->state.currentFrame])(this->matrix, &this->state, 0, 0, false, false);
    break;
  }
}

void MatrixDisplayUi::resetState()
{
  this->state.lastUpdate = 0;
  this->state.ticksSinceLastStateSwitch = 0;
  this->state.frameState = FIXED;
  this->state.currentFrame = 0;
}

void MatrixDisplayUi::drawOverlays()
{
  for (uint8_t i = 0; i < this->overlayCount; i++)
  {
    (this->overlayFunctions[i])(this->matrix, &this->state);
  }
}

uint8_t MatrixDisplayUi::getnextAppNumber()
{
  if (this->nextAppNumber != -1)
    return this->nextAppNumber;
  return (this->state.currentFrame + this->AppCount + this->state.frameTransitionDirection) % this->AppCount;
}
