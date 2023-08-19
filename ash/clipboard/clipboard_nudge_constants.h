// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

enum ClipboardNudgeType {
  // Onboarding nudge. Shows when a user copies and pastes repeatedly within a
  // time frame.
  kOnboardingNudge = 0,

  // Shows when the keyboard shortcut for clipboard is pressed with no items
  // in the history.
  kZeroStateNudge = 1,

  // Shows the keyboard shortcut for clipboard history in the screenshot
  // notification nudge.
  kScreenshotNotificationNudge = 2,

  // Shows when a user copies data that is already in the clipboard history.
  kDuplicateCopyNudge = 3,

  // NOTE: Need to update when adding a new nudge type.
  kMax = kDuplicateCopyNudge,
};

ASH_EXPORT extern const char* const kClipboardHistoryOnboardingNudgeShowCount;
ASH_EXPORT extern const char* const kClipboardHistoryOnboardingNudgeOpenTime;
ASH_EXPORT extern const char* const kClipboardHistoryOnboardingNudgePasteTime;
ASH_EXPORT extern const char* const kClipboardHistoryZeroStateNudgeShowCount;
ASH_EXPORT extern const char* const kClipboardHistoryZeroStateNudgeOpenTime;
ASH_EXPORT extern const char* const kClipboardHistoryZeroStateNudgePasteTime;
ASH_EXPORT extern const char* const
    kClipboardHistoryScreenshotNotificationShowCount;
ASH_EXPORT extern const char* const
    kClipboardHistoryScreenshotNotificationOpenTime;
ASH_EXPORT extern const char* const
    kClipboardHistoryScreenshotNotificationPasteTime;
ASH_EXPORT extern const char* const
    kClipboardHistoryDuplicateCopyNudgeShowCount;
ASH_EXPORT extern const char* const kClipboardHistoryDuplicateCopyNudgeOpenTime;
ASH_EXPORT extern const char* const
    kClipboardHistoryDuplicateCopyNudgePasteTime;

// Returns the histogram that records the time delta between showing the nudge
// of `type` and pasting clipboard history data.
ASH_EXPORT const char* GetClipboardHistoryPasteTimeDeltaHistogram(
    ClipboardNudgeType type);

// Returns the histogram that records the time delta between showing the nudge
// of `type` and showing the clipboard history menu.
ASH_EXPORT const char* GetMenuOpenTimeDeltaHistogram(ClipboardNudgeType type);

constexpr base::TimeDelta kCappedNudgeMinInterval = base::Days(1);
constexpr int kCappedNudgeShownLimit = 3;
constexpr int kContextMenuBadgeShowLimit = 3;
constexpr base::TimeDelta kMaxTimeBetweenPaste = base::Minutes(10);

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
