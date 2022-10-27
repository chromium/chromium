// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
#define ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/metrics/ui_metrics_recorder.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "chromeos/login/login_state/login_state.h"
#include "ui/compositor/total_animation_throughput_reporter.h"

namespace ui {
class Compositor;
}

namespace ash {

class ShelfModel;
class ShelfView;

class ASH_EXPORT LoginUnlockThroughputRecorder
    : public SessionObserver,
      public chromeos::LoginState::Observer {
 public:
  enum RestoreWindowType {
    kBrowser,
    kArc,
  };

  LoginUnlockThroughputRecorder();
  LoginUnlockThroughputRecorder(const LoginUnlockThroughputRecorder&) = delete;
  LoginUnlockThroughputRecorder& operator=(
      const LoginUnlockThroughputRecorder&) = delete;
  ~LoginUnlockThroughputRecorder() override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // chromeos::LoginState::Observer:
  void LoggedInStateChanged() override;

  // Adds "restore_window_id" to the list of potentially restored windows.
  // See
  // https://source.chromium.org/chromium/chromium/src/+/main:ui/views/widget/widget.h;l=404-415.
  void AddScheduledRestoreWindow(int restore_window_id,
                                 const std::string& app_id,
                                 RestoreWindowType window_type);

  // This is called when restored window was created.
  void OnRestoredWindowCreated(int restore_window_id);

  // This is called before window is shown to request presentation feedback.
  void OnBeforeRestoredWindowShown(int restore_window_id,
                                   ui::Compositor* compositor);

  // This is called when restored window was presented.
  void OnRestoredWindowPresented(int restore_window_id);

  // This is called when the list of shelf icons is initialized.
  void InitShelfIconList(const ShelfModel* model);

  // This is called when the list of shelf icons is updated.
  void UpdateShelfIconList(const ShelfModel* model);

  // Remembers ShelfView pointer to watch for shelf animation finish.
  void SetShelfViewIfNotSet(ShelfView* shelf_view);

  void ResetScopedThroughputReporterBlockerForTesting();

  const ui::TotalAnimationThroughputReporter*
  login_animation_throughput_reporter() const {
    return login_animation_throughput_reporter_.get();
  }

 private:
  void OnLoginAnimationFinish(
      base::TimeTicks start,
      const cc::FrameSequenceMetrics::CustomReportData& data);

  void ScheduleWaitForShelfAnimationEnd();

  void OnAllExpectedShelfIconsLoaded();

  UiMetricsRecorder ui_recorder_;

  // Set of window IDs ("restore_window_id") that could be restored but
  // for which windows have not been created yet.
  base::flat_set<int> windows_to_restore_;

  // Set of window IDs ("restore_window_id") that were created as a part of the
  // session restore but not yet shown.
  base::flat_set<int> restore_windows_not_shown_;

  // Set of window IDs ("restore_window_id") that were shown and presentation
  // time was requested.
  base::flat_set<int> restore_windows_presentation_time_requested_;

  // Set of window IDs ("restore_window_id") for which presentation time
  // was received.
  base::flat_set<int> restore_windows_presented_;

  base::TimeTicks primary_user_logged_in_;

  base::raw_ptr<ShelfView> shelf_view_ = nullptr;

  bool shelf_initialized_ = false;

  bool shelf_icons_loaded_ = false;

  bool user_logged_in_ = false;

  base::WeakPtr<ui::TotalAnimationThroughputReporter>
      login_animation_throughput_reporter_;

  std::unique_ptr<
      ui::TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker>
      scoped_throughput_reporter_blocker_;

  base::flat_set<ShelfID> expected_shelf_icons_;

  base::WeakPtrFactory<LoginUnlockThroughputRecorder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
