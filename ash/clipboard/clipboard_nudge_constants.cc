// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_constants.h"

namespace ash {

constexpr const char* const kClipboardHistoryOnboardingNudgeShowCount =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ShownCount";
constexpr const char* const kClipboardHistoryOnboardingNudgeOpenTime =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ToFeatureOpenTime";
constexpr const char* const kClipboardHistoryOnboardingNudgePasteTime =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ToFeaturePasteTime";
constexpr const char* const kClipboardHistoryZeroStateNudgeShowCount =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ShownCount";
constexpr const char* const kClipboardHistoryZeroStateNudgeOpenTime =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ToFeatureOpenTime";
constexpr const char* const kClipboardHistoryZeroStateNudgePasteTime =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ToFeaturePasteTime";
constexpr const char* const kClipboardHistoryScreenshotNotificationShowCount =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge.ShownCount";
constexpr const char* const kClipboardHistoryScreenshotNotificationOpenTime =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge.ToFeatureOpenTime";
constexpr const char* const kClipboardHistoryScreenshotNotificationPasteTime =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge."
    "ToFeaturePasteTime";

const char* GetClipboardHistoryPasteTimeDeltaHistogram(
    ClipboardNudgeType type) {
  switch (type) {
    case ClipboardNudgeType::kOnboardingNudge:
      return kClipboardHistoryOnboardingNudgePasteTime;
    case ClipboardNudgeType::kZeroStateNudge:
      return kClipboardHistoryZeroStateNudgePasteTime;
    case ClipboardNudgeType::kScreenshotNotificationNudge:
      return kClipboardHistoryScreenshotNotificationPasteTime;
  }
}

const char* GetMenuOpenTimeDeltaHistogram(ClipboardNudgeType type) {
  switch (type) {
    case ClipboardNudgeType::kOnboardingNudge:
      return kClipboardHistoryOnboardingNudgeOpenTime;
    case ClipboardNudgeType::kZeroStateNudge:
      return kClipboardHistoryZeroStateNudgeOpenTime;
    case ClipboardNudgeType::kScreenshotNotificationNudge:
      return kClipboardHistoryScreenshotNotificationOpenTime;
  }
}

}  // namespace ash
