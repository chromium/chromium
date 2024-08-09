// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
#define ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_

#include <map>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/metrics/post_login_event_observer.h"
#include "ash/metrics/post_login_metrics_recorder.h"
#include "ash/metrics/ui_metrics_recorder.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
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
  using NotifyCallback = base::OnceCallback<void(base::TimeTicks)>;

  WindowRestoreTracker();
  ~WindowRestoreTracker();
  WindowRestoreTracker(const WindowRestoreTracker&) = delete;
  WindowRestoreTracker& operator=(const WindowRestoreTracker&) = delete;

  void Init(NotifyCallback on_all_window_created,
            NotifyCallback on_all_window_shown,
            NotifyCallback on_all_window_presented);

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
  void OnPresented(int window_id, base::TimeTicks presentation_time);
  int CountWindowsInState(State state) const;

  // Map from window id to window state.
  std::map<int, State> windows_;
  NotifyCallback on_created_;
  NotifyCallback on_shown_;
  NotifyCallback on_presented_;

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

  void AddObserver(PostLoginEventObserver* obs);
  void RemoveObserver(PostLoginEventObserver* obs);

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

  // This is called when cryptohome was successfully created/unlocked.
  void OnAuthSuccess();

  // This is called when ash-chrome is restarted (i.e. on start up procedure
  // of restoring).
  void OnAshRestart();

  void ResetScopedThroughputReporterBlockerForTesting();

  const ui::TotalAnimationThroughputReporter*
  GetLoginAnimationThroughputReporterForTesting() const {
    return login_animation_throughput_reporter_.get();
  }

  // This is called when the list of restore windows is ready. `window_ids`
  // includes session window ids for browser windows that were created in the
  // previous session. `restore_automatically` is true if session restore will
  // start immediately after this call. More specifically, it will be true if
  // the apps restore settings is set to "Always restore" in the OS settings.
  void FullSessionRestoreDataLoaded(std::vector<RestoreWindowID> window_ids,
                                    bool restore_automatically);

  // Records that ARC has finished booting.
  void ArcUiAvailableAfterLogin();

  base::SequencedTaskRunner* post_login_deferred_task_runner() {
    return post_login_deferred_task_runner_.get();
  }

  WindowRestoreTracker* window_restore_tracker() {
    return &window_restore_tracker_;
  }

  PostLoginMetricsRecorder* post_login_metrics_recorder() {
    return &post_login_metrics_recorder_;
  }

  void SetLoginFinishedReportedForTesting();

 private:
  void OnCompositorAnimationFinished(
      const cc::FrameSequenceMetrics::CustomReportData& data,
      base::TimeTicks first_animation_started_at,
      base::TimeTicks last_animation_finished_at);

  void ScheduleWaitForShelfAnimationEndIfNeeded();

  void OnAllExpectedShelfIconsLoaded();

  void MaybeReportLoginFinished();

  void OnPostLoginDeferredTaskTimerFired();

  void OnAllWindowsCreated(base::TimeTicks time);
  void OnAllWindowsShown(base::TimeTicks time);
  void OnAllWindowsPresented(base::TimeTicks time);

  UiMetricsRecorder ui_recorder_;

  WindowRestoreTracker window_restore_tracker_;
  ShelfTracker shelf_tracker_;

  // Whether ash is restarted (due to crash, or applying flags etc).
  bool is_ash_restart_ = false;

  bool user_logged_in_ = false;

  // Flags to DCHECK conditions.
  bool full_session_restore_data_loaded_ = false;
  bool shelf_animation_end_scheduled_ = false;

  // Flags to track state transition.
  std::optional<base::TimeTicks> time_window_restore_done_;
  std::optional<base::TimeTicks> time_shelf_icons_loaded_;
  std::optional<base::TimeTicks> time_shelf_animation_finished_;
  std::optional<base::TimeTicks> time_compositor_animation_finished_;
  bool login_finished_reported_ = false;

  base::WeakPtr<ui::TotalAnimationThroughputReporter>
      login_animation_throughput_reporter_;

  std::unique_ptr<
      ui::TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker>
      scoped_throughput_reporter_blocker_;

  // Timer that triggers post-login tasks in case the login animation is taking
  // longer time than expected.
  base::OneShotTimer post_login_deferred_task_timer_;
  // Deferred task runner for the post-login tasks.
  scoped_refptr<base::DeferredSequencedTaskRunner>
      post_login_deferred_task_runner_;

  base::ObserverList<PostLoginEventObserver> observers_;

  PostLoginMetricsRecorder post_login_metrics_recorder_;

  base::WeakPtrFactory<LoginUnlockThroughputRecorder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_METRICS_LOGIN_UNLOCK_THROUGHPUT_RECORDER_H_
