// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

enum ClipboardNudgeType {
  // Onboarding nudge. Shows when a user copy and pastes repeatedly within a
  // time frame.
  kOnboardingNudge = 0,
  // Shows when the keyboard shortcut for clipboard is pressed with no items
  // in the history.
  kZeroStateNudge = 1,
};

constexpr int kNotificationLimit = 3;
constexpr int kContextMenuBadgeShowLimit = 3;
constexpr base::TimeDelta kMinInterval = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kMaxTimeBetweenPaste =
    base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kNudgeShowTime = base::TimeDelta::FromSeconds(10);
constexpr float kNudgeFadeAnimationScale = 1.2f;
constexpr base::TimeDelta kNudgeFadeAnimationTime =
    base::TimeDelta::FromMilliseconds(250);
constexpr gfx::Tween::Type kNudgeFadeOpacityAnimationTweenType =
    gfx::Tween::LINEAR;
constexpr gfx::Tween::Type kNudgeFadeScalingAnimationTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
