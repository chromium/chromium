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

inline constexpr char kFullRestoreNotificationHistogram[] =
    "Ash.FullRestore.ShowFullRestoreNotification";
inline constexpr char kFullRestoreDialogHistogram[] =
    "Ash.FullRestore.ShowInformedRestoreDialog";

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
  kFailedInGuestOrPublicUserSession,
  kMaxValue = kFailedInGuestOrPublicUserSession,
};

// Enumeration of the ways the informed restore dialog could be closed. Used for
// histograms.
enum class CloseDialogType {
  kListviewRestoreButton,
  kListviewCancelButton,
  kListviewOther,
  kScreenshotRestoreButton,
  kScreenshotCancelButton,
  kScreenshotOther,
  kMaxValue = kScreenshotOther,
};

void RecordDialogClosing(CloseDialogType type);

// Records `status` on taking the screenshot on shutdown.
void RecordScreenshotOnShutdownStatus(ScreenshotOnShutdownStatus status);

// Records the durations of taking the screenshot, decoding and saving the
// screenshot taken on the last shutdown. Resets the prefs used to store the
// metrics across shutdowns.
void RecordScreenshotDurations(PrefService* local_state);

// Records whether the informed restore dialog is shown with screenshot or
// listview.
void RecordDialogScreenshotVisibility(bool visible);

// Records the time duration of fetching the screenshot from the disk and
// decoding it.
void RecordScreenshotDecodeDuration(base::TimeDelta duration);

// Records the duration from the time the informed restore dialog is shown to
// the user take an action on it.
void RecordTimeToAction(base::TimeDelta duration, bool showing_listview);

// Records the user's action at the onboarding page, `restore` is true if the
// user turned on restore on this page.
void RecordOnboardingAction(bool restore);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_WINDOW_RESTORE_METRICS_H_
