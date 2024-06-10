// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_METRICS_H_
#define ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_METRICS_H_

#include "base/time/time.h"

class PrefService;

namespace ash {

inline constexpr char kDialogClosedHistogram[] = "Ash.Pine.DialogClosed";
inline constexpr char kScreenshotOnShutdownStatus[] =
    "Ash.Pine.ScreenshotOnShutdownStatus";
inline constexpr char kDialogScreenshotVisibility[] =
    "Ash.Pine.DialogScreenshotVisibility";
inline constexpr char kInformedRestoreOnboardingHistogram[] =
    "Ash.Pine.OnboardingDialog.TurnRestoreOn";

// Enumeration of the status for taking the screenshot on shutdown.
// Note that these values are persisted to histograms so existing values should
// remain unchanged and new values should be added to the end.
enum class ScreenshotOnShutdownStatus {
  kSucceeded,
  kFailedInOverview,
  kFailedInLockScreen,
  kFailedInHomeLauncher,
  kFailedInPinnedMode,
  kFailedWithIncognito,
  kFailedWithNoWindows,
  kFailedOnTakingScreenshotTimeout,
  kFailedOnDifferentOrientations,
  kFailedOnDLP,
  kMaxValue = kFailedOnDLP,
};

// Enumeration of the ways the pine dialog could be closed. Used for histograms.
enum class ClosePineDialogType {
  kListviewRestoreButton,
  kListviewCancelButton,
  kListviewOther,
  kScreenshotRestoreButton,
  kScreenshotCancelButton,
  kScreenshotOther,
  kMaxValue = kScreenshotOther,
};

void RecordPineDialogClosing(ClosePineDialogType type);

// Records `status` on taking the screenshot on shutdown.
void RecordScreenshotOnShutdownStatus(ScreenshotOnShutdownStatus status);

// Records the durations of taking the screenshot, decoding and saving the pine
// screenshot taken on the last shutdown. Resets the prefs used to store the
// metrics across shutdowns.
void RecordPineScreenshotDurations(PrefService* local_state);

// Records whether the pine dialog is shown with screenshot or listview.
void RecordDialogScreenshotVisibility(bool visible);

// Records the time duration of fetching the pine screenshot from the disk and
// decoding it.
void RecordScreenshotDecodeDuration(base::TimeDelta duration);

// Records the duration from the pine dialog is shown to the user take an action
// on it.
void RecordTimeToAction(base::TimeDelta duration, bool showing_listview);

// Records the user's action at the onboarding page, `restore` is true if the
// user turned on restore on this page.
void RecordOnboardingAction(bool restore);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_METRICS_H_
