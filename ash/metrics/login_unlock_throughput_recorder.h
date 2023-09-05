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
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/total_animation_throughput_reporter.h"

namespace ui {
class Compositor;
}

namespace ash {

class ShelfModel;

class ASH_EXPORT LoginUnlockThroughputRecorder : public SessionObserver,
                                                 public LoginState::Observer {
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

  // LoginState::Observer:
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

  // This is called when ARC++ becomes enabled.
  void OnArcOptedIn();

  // This is called when list of ARC++ apps is updated.
  void OnArcAppListReady();

  // This is true if we need to report Ash.ArcAppInitialAppsInstallDuration
  // histogram in this session but it has not been reported yet.
  bool NeedReportArcAppListReady() const;

  void ResetScopedThroughputReporterBlockerForTesting();

  const ui::TotalAnimationThroughputReporter*
  login_animation_throughput_reporter() const {
    return login_animation_throughput_reporter_.get();
  }

  // Add a time marker for login animations events. A timeline will be sent to
  // tracing after login is done.
  void AddLoginTimeMarker(const std::string& marker_name);

  // This flag signals that all expected browser windows are already scheduled.
  void RestoreDataLoaded();

  // Records that ARC has finished booting.
  void ArcUiAvailableAfterLogin();

  base::SequencedTaskRunner* post_login_deferred_task_runner() const {
    return post_login_deferred_task_runner_.get();
  }

 private:
  class TimeMarker {
   public:
    explicit TimeMarker(const std::string& name);
    TimeMarker(const TimeMarker& other) = default;
    ~TimeMarker() = default;

    const std::string& name() const { return name_; }
    base::TimeTicks time() const { return time_; }

    // Comparator for sorting.
    bool operator<(const TimeMarker& other) const {
      return time_ < other.time_;
    }

   private:
    friend class std::vector<TimeMarker>;

    const std::string name_;
    const base::TimeTicks time_ = base::TimeTicks::Now();
  };

  void OnLoginAnimationFinish(
      base::TimeTicks start,
      const cc::FrameSequenceMetrics::CustomReportData& data);

  void ScheduleWaitForShelfAnimationEndIfNeeded();

  void OnAllExpectedShelfIconsLoaded();

  void MaybeReportLoginFinished();

  void OnLoginAnimationFinishedTimerFired();

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

  bool shelf_initialized_ = false;

  // |has_pending_icon_| is true when last shelf icons update had an item
  // pending icon load.
  bool has_pending_icon_ = false;

  // |shelf_icons_loaded_| is true when shelf icons are considered loaded,
  // i.e. |has_pending_icon_| is true and first resored browser window was
  // created.
  bool shelf_icons_loaded_ = false;

  bool user_logged_in_ = false;

  bool arc_app_list_ready_reported_ = false;

  bool all_restored_windows_presented_ = false;

  bool shelf_animation_end_scheduled_ = false;

  bool shelf_animation_finished_ = false;

  bool login_animation_throughput_received_ = false;

  bool login_finished_reported_ = false;

  // |browser_windows_will_not_be_restored_| is true when session restore
  // window list is empty.
  bool browser_windows_will_not_be_restored_ = false;

  // This is a signal that current list of expected windows to be restored is
  // final.
  bool first_restored_window_created_ = false;

  absl::optional<base::TimeTicks> arc_opt_in_time_;

  base::WeakPtr<ui::TotalAnimationThroughputReporter>
      login_animation_throughput_reporter_;

  std::unique_ptr<
      ui::TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker>
      scoped_throughput_reporter_blocker_;

  std::vector<TimeMarker> login_time_markers_;

  // Timer that sets the limit to wait for the login animation to finish
  // before scheduling post-login tasks.
  base::OneShotTimer login_animation_finished_timer_;

  // Deferred task runner for the post-login tasks.
  scoped_refptr<base::DeferredSequencedTaskRunner>
      post_login_deferred_task_runner_;

  base::WeakPtrFactory<LoginUnlockThroughputRecorder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
