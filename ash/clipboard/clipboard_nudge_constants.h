// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_

#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

constexpr int kNotificationLimit = 3;
constexpr base::TimeDelta kMinInterval = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kMaxTimeBetweenPaste =
    base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kNudgeShowTime = base::TimeDelta::FromSeconds(6);
constexpr float kNudgeFadeAnimationScale = 1.2f;
constexpr base::TimeDelta kNudgeFadeAnimationTime =
    base::TimeDelta::FromMilliseconds(250);
constexpr gfx::Tween::Type kNudgeFadeOpacityAnimationTweenType =
    gfx::Tween::LINEAR;
constexpr gfx::Tween::Type kNudgeFadeScalingAnimationTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
