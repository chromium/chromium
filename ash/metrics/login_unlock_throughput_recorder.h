// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
#define ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_

#include <map>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/metrics/ui_metrics_recorder.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/compositor/total_animation_throughput_reporter.h"

namespace ui {
class Compositor;
}

namespace ash {

class ShelfModel;

// WindowRestoreTracker tracks window states while windows are being restored
// during login time and triggers callbacks on some events.
class ASH_EXPORT WindowRestoreTracker {
 public:
  WindowRestoreTracker();
  ~WindowRestoreTracker();
  WindowRestoreTracker(const WindowRestoreTracker&) = delete;
  WindowRestoreTracker& operator=(const WindowRestoreTracker&) = delete;

  void Init(base::OnceClosure on_all_window_created,
            base::OnceClosure on_all_window_shown,
            base::OnceClosure on_all_window_presented);

  int NumberOfWindows() const;
  void AddWindow(int window_id, const std::string& app_id);
  void OnCreated(int window_id);
  void OnShown(int window_id, ui::Compositor* compositor);
  void OnPresentedForTesting(int window_id);

 private:
  enum class State {
    kNotCreated,  // This window is about to restore, but has not been created.
    kCreated,     // This window has been created.
    kShown,       // Show() is about to be called for this window.
    kPresented,   // This window is presented on the screen.
  };

  void OnCompositorFramePresented(int window_id,
                                  const viz::FrameTimingDetails& details);
  void OnPresented(int window_id);
  int CountWindowsInState(State state) const;

  // Map from window id to window state.
  std::map<int, State> windows_;
  base::OnceClosure on_created_;
  base::OnceClosure on_shown_;
  base::OnceClosure on_presented_;

  base::WeakPtrFactory<WindowRestoreTracker> weak_ptr_factory_{this};
};

// ShelfTracker waits until all pinned shelf icons are loaded and then triggers
// a callback.
class ASH_EXPORT ShelfTracker {
 public:
  ShelfTracker();
  ~ShelfTracker();
  ShelfTracker(const ShelfTracker&) = delete;
  ShelfTracker& operator=(const ShelfTracker&) = delete;

  void Init(base::OnceClosure on_all_expected_icons_loaded);

  void OnListInitialized(const ShelfModel* model);
  void OnUpdated(const ShelfModel* model);
  void IgnoreBrowserIcon();

 private:
  void MaybeRunClosure();

  bool shelf_item_list_initialized_ = false;
  bool has_pending_icon_ = false;
  bool has_browser_icon_ = false;
  bool should_check_browser_icon_ = true;

  base::OnceClosure on_ready_;
};

class ASH_EXPORT LoginUnlockThroughputRecorder : public LoginState::Observer {
 public:
  struct RestoreWindowID {
    int session_window_id;
    std::string app_name;
  };

  LoginUnlockThroughputRecorder();
  LoginUnlockThroughputRecorder(const LoginUnlockThroughputRecorder&) = delete;
  LoginUnlockThroughputRecorder& operator=(
      const LoginUnlockThroughputRecorder&) = delete;
  ~LoginUnlockThroughputRecorder() override;

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // This is called when restored window was created.
  void OnRestoredWindowCreated(int restore_window_id);

  // This is called before window is shown to request presentation feedback.
  void OnBeforeRestoredWindowShown(int restore_window_id,
                                   ui::Compositor* compositor);

  // This is called when the list of shelf icons is initialized.
  void InitShelfIconList(const ShelfModel* model);

  // This is called when the list of shelf icons is updated.
  void UpdateShelfIconList(const ShelfModel* model);

  // This is called when ARC++ becomes enabled.
  void OnArcOptedIn();

  // This is called when list of ARC++ apps is updated.
  void OnArcAppListReady();

  // This is called when cryptohome was successfully created/unlocked.
  void OnAuthSuccess();

  // This is called when ash-chrome is restarted (i.e. on start up procedure
  // of restoring).
  void OnAshRestart();

  // This is true if we need to report Ash.ArcAppInitialAppsInstallDuration
  // histogram in this session but it has not been reported yet.
  bool NeedReportArcAppListReady() const;

  void ResetScopedThroughputReporterBlockerForTesting();

  const ui::TotalAnimationThroughputReporter*
  GetLoginAnimationThroughputReporterForTesting() const {
    return login_animation_throughput_reporter_.get();
  }

  // Add a time marker for login animations events. A timeline will be sent to
  // tracing after login is done.
  void AddLoginTimeMarker(const std::string& marker_name);

  // This is called when the list of session windows gets completed. Note that
  // this can be called multiple times when session restore is attempted
  // multiple times, e.g. due to errors.
  void BrowserSessionRestoreDataLoaded(std::vector<RestoreWindowID> window_ids);

  // This is called when the list of full restore windows, e.g. Lacros windows.
  void FullSessionRestoreDataLoaded(std::vector<RestoreWindowID> window_ids);

  // Records that ARC has finished booting.
  void ArcUiAvailableAfterLogin();

  base::SequencedTaskRunner* post_login_deferred_task_runner() {
    return post_login_deferred_task_runner_.get();
  }

  WindowRestoreTracker* window_restore_tracker() {
    return &window_restore_tracker_;
  }

  void SetLoginFinishedReportedForTesting();

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

  void OnCompositorAnimationFinished(
      base::TimeTicks start,
      const cc::FrameSequenceMetrics::CustomReportData& data);

  void ScheduleWaitForShelfAnimationEndIfNeeded();

  void OnAllExpectedShelfIconsLoaded();

  void MaybeReportLoginFinished();

  void OnPostLoginDeferredTaskTimerFired();

  void MaybeRestoreDataLoaded();

  // We only want to initialize the slice name on certain expected events.
  // If we miss these, it will ne names "Unordered" and we will know that
  // we missed the expected event.
  void EnsureTracingSliceNamed();

  void OnAllWindowsCreated();
  void OnAllWindowsShown();
  void OnAllWindowsPresented();

  UiMetricsRecorder ui_recorder_;

  WindowRestoreTracker window_restore_tracker_;
  ShelfTracker shelf_tracker_;

  std::optional<base::TimeTicks> timestamp_on_auth_success_;
  std::optional<base::TimeTicks> timestamp_primary_user_logged_in_;

  // Whether ash is restarted (due to crash, or applying flags etc).
  bool is_ash_restart_ = false;

  bool user_logged_in_ = false;

  // Session restore data comes from chrome::SessionRestore and ash::FullRestore
  // independently.

  // This flag is true after SessionRestore has finished loading its data.
  bool browser_session_restore_data_loaded_ = false;

  // This flag is true after FullRestore has finished loading its data.
  bool full_session_restore_data_loaded_ = false;

  bool window_restore_done_ = false;

  // |shelf_icons_loaded_| is true when all shelf icons are considered loaded,
  // i.e. there is no pending icon on shelf after shelf is initialized.
  bool shelf_icons_loaded_ = false;

  bool dcheck_shelf_animation_end_scheduled_ = false;

  bool shelf_animation_finished_ = false;

  bool arc_app_list_ready_reported_ = false;

  bool login_animation_throughput_received_ = false;

  bool login_finished_reported_ = false;

  std::optional<base::TimeTicks> arc_opt_in_time_;

  base::WeakPtr<ui::TotalAnimationThroughputReporter>
      login_animation_throughput_reporter_;

  std::unique_ptr<
      ui::TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker>
      scoped_throughput_reporter_blocker_;

  std::vector<TimeMarker> login_time_markers_;

  // Timer that triggers post-login tasks in case the login animation is taking
  // longer time than expected.
  base::OneShotTimer post_login_deferred_task_timer_;

  // Deferred task runner for the post-login tasks.
  scoped_refptr<base::DeferredSequencedTaskRunner>
      post_login_deferred_task_runner_;

  base::WeakPtrFactory<LoginUnlockThroughputRecorder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
