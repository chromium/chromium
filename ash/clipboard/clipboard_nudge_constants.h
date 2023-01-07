// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_

#include "base/time/time.h"

namespace ash {

enum ClipboardNudgeType {
  // Onboarding nudge. Shows when a user copy and pastes repeatedly within a
  // time frame.
  kOnboardingNudge = 0,
  // Shows when the keyboard shortcut for clipboard is pressed with no items
  // in the history.
  kZeroStateNudge = 1,
  // Shows the keyboard shortcut for clipboard history in the screenshot
  // notification nudge.
  kScreenshotNotificationNudge = 2,
};

const char kOnboardingNudge_ShowCount[] =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ShownCount";
const char kOnboardingNudge_OpenTime[] =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ToFeatureOpenTime";
const char kOnboardingNudge_PasteTime[] =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ToFeaturePasteTime";
const char kZeroStateNudge_ShowCount[] =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ShownCount";
const char kZeroStateNudge_OpenTime[] =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ToFeatureOpenTime";
const char kZeroStateNudge_PasteTime[] =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ToFeaturePasteTime";
const char kScreenshotNotification_ShowCount[] =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge.ShownCount";
const char kScreenshotNotification_OpenTime[] =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge.ToFeatureOpenTime";
const char kScreenshotNotification_PasteTime[] =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge."
    "ToFeaturePasteTime";

constexpr int kNotificationLimit = 3;
constexpr int kContextMenuBadgeShowLimit = 3;
constexpr base::TimeDelta kMinInterval = base::Days(1);
constexpr base::TimeDelta kMaxTimeBetweenPaste = base::Minutes(10);

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
