// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_constants.h"

namespace ash {

constexpr const char* const kClipboardHistoryOnboardingNudgeShowCount =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ShownCount";
constexpr const char* const kClipboardHistoryOnboardingNudgeOpenTime =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ToFeatureOpenTimeV2";
constexpr const char* const kClipboardHistoryOnboardingNudgePasteTime =
    "Ash.ClipboardHistory.Nudges.OnboardingNudge.ToFeaturePasteTimeV2";
constexpr const char* const kClipboardHistoryZeroStateNudgeShowCount =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ShownCount";
constexpr const char* const kClipboardHistoryZeroStateNudgeOpenTime =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ToFeatureOpenTimeV2";
constexpr const char* const kClipboardHistoryZeroStateNudgePasteTime =
    "Ash.ClipboardHistory.Nudges.ZeroStateNudge.ToFeaturePasteTimeV2";
constexpr const char* const kClipboardHistoryScreenshotNotificationShowCount =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge.ShownCount";
constexpr const char* const kClipboardHistoryScreenshotNotificationOpenTime =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge."
    "ToFeatureOpenTimeV2";
constexpr const char* const kClipboardHistoryScreenshotNotificationPasteTime =
    "Ash.ClipboardHistory.Nudges.ScreenshotNotificationNudge."
    "ToFeaturePasteTimeV2";
constexpr const char* const kClipboardHistoryDuplicateCopyNudgeShowCount =
    "Ash.ClipboardHistory.Nudges.DuplicateCopyNudge.ShownCount";
constexpr const char* const kClipboardHistoryDuplicateCopyNudgeOpenTime =
    "Ash.ClipboardHistory.Nudges.DuplicateCopyNudge.ToFeatureOpenTimeV2";
constexpr const char* const kClipboardHistoryDuplicateCopyNudgePasteTime =
    "Ash.ClipboardHistory.Nudges.DuplicateCopyNudge.ToFeaturePasteTimeV2";

const char* GetClipboardHistoryPasteTimeDeltaHistogram(
    ClipboardNudgeType type) {
  switch (type) {
    case ClipboardNudgeType::kOnboardingNudge:
      return kClipboardHistoryOnboardingNudgePasteTime;
    case ClipboardNudgeType::kZeroStateNudge:
      return kClipboardHistoryZeroStateNudgePasteTime;
    case ClipboardNudgeType::kScreenshotNotificationNudge:
      return kClipboardHistoryScreenshotNotificationPasteTime;
    case ClipboardNudgeType::kDuplicateCopyNudge:
      return kClipboardHistoryDuplicateCopyNudgePasteTime;
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
    case ClipboardNudgeType::kDuplicateCopyNudge:
      return kClipboardHistoryDuplicateCopyNudgeOpenTime;
  }
}

}  // namespace ash
