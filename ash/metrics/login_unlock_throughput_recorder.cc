// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include <algorithm>
#include <map>
#include <utility>

#include "ash/metrics/post_login_event_observer.h"
#include "ash/metrics/post_login_metrics_recorder.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/app_constants/constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/total_animation_throughput_reporter.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"

namespace ash {
namespace {

// A class used to wait for animations.
class ShelfAnimationObserver : public views::BoundsAnimatorObserver {
 public:
  ShelfAnimationObserver(base::OnceClosure& on_shelf_animation_end)
      : on_shelf_animation_end_(std::move(on_shelf_animation_end)) {}

  ShelfAnimationObserver(const ShelfAnimationObserver&) = delete;
  ShelfAnimationObserver& operator=(const ShelfAnimationObserver&) = delete;
  ~ShelfAnimationObserver() override = default;

  // views::BoundsAnimatorObserver overrides:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    GetShelfView()->RemoveAnimationObserver(this);
    RunCallbackAndDestroy();
  }

  void StartObserving() {
    ShelfView* shelf_view = GetShelfView();

    if (!shelf_view->IsAnimating()) {
      RunCallbackAndDestroy();
      return;
    }

    shelf_view->AddAnimationObserver(this);
  }

 private:
  void RunCallbackAndDestroy() {
    std::move(on_shelf_animation_end_).Run();
    delete this;
  }

  ShelfView* GetShelfView() {
    return RootWindowController::ForWindow(
               Shell::Get()->window_tree_host_manager()->GetPrimaryRootWindow())
        ->shelf()
        ->hotseat_widget()
        ->scrollable_shelf_view()
        ->shelf_view();
  }

  base::OnceClosure on_shelf_animation_end_;
};

bool HasBrowserIcon(const ShelfModel* model) {
  return model->ItemByID(ShelfID(app_constants::kLacrosAppId)) ||
         model->ItemByID(ShelfID(app_constants::kChromeAppId));
}

bool HasPendingIcon(const ShelfModel* model) {
  return base::ranges::any_of(model->items(), [](const ShelfItem& item) {
    return item.image.isNull();
  });
}

}  // namespace

WindowRestoreTracker::WindowRestoreTracker() = default;
WindowRestoreTracker::~WindowRestoreTracker() = default;

void WindowRestoreTracker::Init(
    WindowRestoreTracker::NotifyCallback on_all_window_created,
    WindowRestoreTracker::NotifyCallback on_all_window_shown,
    WindowRestoreTracker::NotifyCallback on_all_window_presented) {
  on_created_ = std::move(on_all_window_created);
  on_shown_ = std::move(on_all_window_shown);
  on_presented_ = std::move(on_all_window_presented);
}

void WindowRestoreTracker::AddWindow(int window_id, const std::string& app_id) {
  DCHECK(window_id);
  DCHECK(!app_id.empty());
  if (app_id == app_constants::kChromeAppId ||
      app_id == app_constants::kLacrosAppId) {
    windows_.emplace(window_id, State::kNotCreated);
  }
}

void WindowRestoreTracker::OnCreated(int window_id) {
  auto iter = windows_.find(window_id);
  if (iter == windows_.end()) {
    return;
  }
  if (iter->second != State::kNotCreated) {
    return;
  }
  iter->second = State::kCreated;

  const bool all_created = CountWindowsInState(State::kNotCreated) == 0;
  if (all_created && on_created_) {
    std::move(on_created_).Run(base::TimeTicks::Now());
  }
}

void WindowRestoreTracker::OnShown(int window_id, ui::Compositor* compositor) {
  auto iter = windows_.find(window_id);
  if (iter == windows_.end()) {
    return;
  }
  if (iter->second != State::kCreated) {
    return;
  }
  iter->second = State::kShown;

  const bool all_shown = CountWindowsInState(State::kNotCreated) == 0 &&
                         CountWindowsInState(State::kCreated) == 0;
  if (all_shown && on_shown_) {
    std::move(on_shown_).Run(base::TimeTicks::Now());
  }

  if (compositor &&
      display::Screen::GetScreen()->GetPrimaryDisplay().detected()) {
    compositor->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(&WindowRestoreTracker::OnCompositorFramePresented,
                       weak_ptr_factory_.GetWeakPtr(), window_id));
  } else if (compositor) {
    // Primary display not detected. Assume it's a headless unit.
    OnPresented(window_id, base::TimeTicks::Now());
  }
}

void WindowRestoreTracker::OnPresentedForTesting(int window_id) {
  OnPresented(window_id, base::TimeTicks::Now());
}

void WindowRestoreTracker::OnCompositorFramePresented(
    int window_id,
    const viz::FrameTimingDetails& details) {
  OnPresented(window_id, details.presentation_feedback.timestamp);
}

void WindowRestoreTracker::OnPresented(int window_id,
                                       base::TimeTicks presentation_time) {
  auto iter = windows_.find(window_id);
  if (iter == windows_.end()) {
    return;
  }
  if (iter->second != State::kShown) {
    return;
  }
  iter->second = State::kPresented;

  const bool all_presented = CountWindowsInState(State::kNotCreated) == 0 &&
                             CountWindowsInState(State::kCreated) == 0 &&
                             CountWindowsInState(State::kShown) == 0;
  if (all_presented && on_presented_) {
    std::move(on_presented_).Run(presentation_time);
  }
}

int WindowRestoreTracker::CountWindowsInState(State state) const {
  return std::count_if(
      windows_.begin(), windows_.end(),
      [state](const std::pair<int, State>& kv) { return kv.second == state; });
}

ShelfTracker::ShelfTracker() = default;
ShelfTracker::~ShelfTracker() = default;

void ShelfTracker::Init(base::OnceClosure on_all_expected_icons_loaded) {
  on_ready_ = std::move(on_all_expected_icons_loaded);
}

void ShelfTracker::OnListInitialized(const ShelfModel* model) {
  shelf_item_list_initialized_ = true;
  OnUpdated(model);
}

void ShelfTracker::OnUpdated(const ShelfModel* model) {
  has_browser_icon_ = HasBrowserIcon(model);
  has_pending_icon_ = HasPendingIcon(model);
  MaybeRunClosure();
}

void ShelfTracker::IgnoreBrowserIcon() {
  should_check_browser_icon_ = false;
  MaybeRunClosure();
}

void ShelfTracker::MaybeRunClosure() {
  const bool browser_icon_ready =
      !should_check_browser_icon_ || has_browser_icon_;
  const bool all_icons_are_ready =
      shelf_item_list_initialized_ && browser_icon_ready && !has_pending_icon_;
  if (!all_icons_are_ready) {
    return;
  }

  if (on_ready_) {
    std::move(on_ready_).Run();
  }
}

LoginUnlockThroughputRecorder::LoginUnlockThroughputRecorder()
    : post_login_deferred_task_runner_(
          base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
              base::SequencedTaskRunner::GetCurrentDefault())),
      post_login_metrics_recorder_(this) {
  LoginState::Get()->AddObserver(this);

  window_restore_tracker_.Init(
      base::BindOnce(&LoginUnlockThroughputRecorder::OnAllWindowsCreated,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&LoginUnlockThroughputRecorder::OnAllWindowsShown,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&LoginUnlockThroughputRecorder::OnAllWindowsPresented,
                     weak_ptr_factory_.GetWeakPtr()));

  shelf_tracker_.Init(base::BindOnce(
      &LoginUnlockThroughputRecorder::OnAllExpectedShelfIconsLoaded,
      weak_ptr_factory_.GetWeakPtr()));
}

LoginUnlockThroughputRecorder::~LoginUnlockThroughputRecorder() {
  LoginState::Get()->RemoveObserver(this);
}

void LoginUnlockThroughputRecorder::AddObserver(PostLoginEventObserver* obs) {
  observers_.AddObserver(obs);
}

void LoginUnlockThroughputRecorder::RemoveObserver(
    PostLoginEventObserver* obs) {
  observers_.RemoveObserver(obs);
}

void LoginUnlockThroughputRecorder::OnAuthSuccess() {
  auto now = base::TimeTicks::Now();
  for (auto& obs : observers_) {
    obs.OnAuthSuccess(now);
  }
}

void LoginUnlockThroughputRecorder::OnAshRestart() {
  is_ash_restart_ = true;
  post_login_deferred_task_timer_.Stop();
  if (!post_login_deferred_task_runner_->Started()) {
    post_login_deferred_task_runner_->Start();
  }
}

void LoginUnlockThroughputRecorder::LoggedInStateChanged() {
  auto* login_state = LoginState::Get();
  auto logged_in_user = login_state->GetLoggedInUserType();

  if (user_logged_in_)
    return;

  if (!login_state->IsUserLoggedIn())
    return;

  const bool is_regular_user_or_owner =
      logged_in_user == LoginState::LOGGED_IN_USER_REGULAR;

  auto now = base::TimeTicks::Now();
  for (auto& obs : observers_) {
    obs.OnUserLoggedIn(now, is_ash_restart_, is_regular_user_or_owner);
  }

  if (!is_regular_user_or_owner) {
    // Kiosk users fall here.
    return;
  }

  // On ash restart, `SessionManager::CreateSessionForRestart` should happen
  // and trigger `LoggedInStateChanged` here to set `user_logged_in_` flag
  // before `OnAshRestart` is called. So `is_ash_restart_` should never be true
  // here. Otherwise, we have unexpected sequence of events and login metrics
  // would not be correctly reported.
  //
  // It seems somehow happening in b/333262357. Adding a DumpWithoutCrashing
  // to capture the offending stack.
  // TODO(b/333262357): Remove `DumpWithoutCrashing`.
  if (is_ash_restart_) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  user_logged_in_ = true;

  ui_recorder_.OnUserLoggedIn();

  auto* primary_root = Shell::GetPrimaryRootWindow();

  auto* rec = new ui::TotalAnimationThroughputReporter(
      primary_root->GetHost()->compositor(),
      base::BindOnce(
          &LoginUnlockThroughputRecorder::OnCompositorAnimationFinished,
          weak_ptr_factory_.GetWeakPtr()),
      /*should_delete=*/true);
  login_animation_throughput_reporter_ = rec->GetWeakPtr();
  DCHECK(!scoped_throughput_reporter_blocker_);
  // Login animation metrics should not be reported until all shelf icons
  // were loaded.
  scoped_throughput_reporter_blocker_ =
      login_animation_throughput_reporter_->NewScopedBlocker();

  constexpr base::TimeDelta kLoginAnimationDelayTimer = base::Seconds(20);
  // post_login_deferred_task_timer_ is owned by this class so it's safe to
  // use unretained pointer here.
  post_login_deferred_task_timer_.Start(
      FROM_HERE, kLoginAnimationDelayTimer, this,
      &LoginUnlockThroughputRecorder::OnPostLoginDeferredTaskTimerFired);
}

void LoginUnlockThroughputRecorder::OnRestoredWindowCreated(int id) {
  window_restore_tracker_.OnCreated(id);
}

void LoginUnlockThroughputRecorder::OnBeforeRestoredWindowShown(
    int id,
    ui::Compositor* compositor) {
  window_restore_tracker_.OnShown(id, compositor);
}

void LoginUnlockThroughputRecorder::InitShelfIconList(const ShelfModel* model) {
  shelf_tracker_.OnListInitialized(model);
}

void LoginUnlockThroughputRecorder::UpdateShelfIconList(
    const ShelfModel* model) {
  shelf_tracker_.OnUpdated(model);
}

void LoginUnlockThroughputRecorder::
    ResetScopedThroughputReporterBlockerForTesting() {
  scoped_throughput_reporter_blocker_.reset();
}

void LoginUnlockThroughputRecorder::OnCompositorAnimationFinished(
    const cc::FrameSequenceMetrics::CustomReportData& data,
    base::TimeTicks first_animation_started_at,
    base::TimeTicks last_animation_finished_at) {
  for (auto& obs : observers_) {
    obs.OnCompositorAnimationFinished(last_animation_finished_at, data);
  }

  time_compositor_animation_finished_ = last_animation_finished_at;
  MaybeReportLoginFinished();
}

void LoginUnlockThroughputRecorder::ScheduleWaitForShelfAnimationEndIfNeeded() {
  // If not ready yet, do nothing this time.
  if (!time_window_restore_done_.has_value() ||
      !time_shelf_icons_loaded_.has_value()) {
    return;
  }

  DCHECK(!shelf_animation_end_scheduled_);
  shelf_animation_end_scheduled_ = true;

  auto timestamp =
      std::max(*time_window_restore_done_, *time_shelf_icons_loaded_);

  for (auto& obs : observers_) {
    obs.OnShelfIconsLoadedAndSessionRestoreDone(timestamp);
  }

  scoped_throughput_reporter_blocker_.reset();

  // TotalAnimationThroughputReporter (login_animation_throughput_reporter_)
  // reports only on next non-animated frame. Ensure there is one.
  aura::Window* shelf_container =
      Shell::Get()->GetPrimaryRootWindowController()->GetContainer(
          kShellWindowId_ShelfContainer);
  if (shelf_container) {
    gfx::Rect bounds = shelf_container->GetTargetBounds();
    // Minimize affected area.
    bounds.set_width(1);
    bounds.set_height(1);
    shelf_container->SchedulePaintInRect(bounds);
  }

  base::OnceCallback on_shelf_animation_end = base::BindOnce(
      [](base::WeakPtr<LoginUnlockThroughputRecorder> self) {
        if (!self) {
          return;
        }

        auto now = base::TimeTicks::Now();
        for (auto& obs : self->observers_) {
          obs.OnShelfAnimationFinished(now);
        }

        self->time_shelf_animation_finished_ = now;
        self->MaybeReportLoginFinished();
      },
      weak_ptr_factory_.GetWeakPtr());

  (new ShelfAnimationObserver(on_shelf_animation_end))->StartObserving();

  post_login_deferred_task_timer_.Stop();
  if (!post_login_deferred_task_runner_->Started()) {
    post_login_deferred_task_runner_->Start();
  }
}

void LoginUnlockThroughputRecorder::OnAllExpectedShelfIconsLoaded() {
  auto now = base::TimeTicks::Now();

  DCHECK(!time_shelf_icons_loaded_.has_value());
  time_shelf_icons_loaded_ = now;

  for (auto& obs : observers_) {
    obs.OnAllExpectedShelfIconLoaded(now);
  }

  ScheduleWaitForShelfAnimationEndIfNeeded();
}

void LoginUnlockThroughputRecorder::FullSessionRestoreDataLoaded(
    std::vector<RestoreWindowID> window_ids,
    bool restore_automatically) {
  if (login_finished_reported_) {
    return;
  }

  DCHECK(!full_session_restore_data_loaded_);
  full_session_restore_data_loaded_ = true;

  auto now = base::TimeTicks::Now();
  for (auto& obs : observers_) {
    obs.OnSessionRestoreDataLoaded(now, restore_automatically);
  }

  // TODO(b/343001594): If `restore_automatically` is false, we should report
  // the metrics with different names rather than ignoring session restore.
  // For now we ignore session restore to keep consistency with old behavior.
  if (window_ids.empty() || !restore_automatically) {
    shelf_tracker_.IgnoreBrowserIcon();

    DCHECK(!time_window_restore_done_.has_value());
    time_window_restore_done_ = now;
    ScheduleWaitForShelfAnimationEndIfNeeded();
  } else {
    for (const auto& w : window_ids) {
      window_restore_tracker_.AddWindow(w.session_window_id, w.app_name);
    }
  }
}

void LoginUnlockThroughputRecorder::ArcUiAvailableAfterLogin() {
  auto now = base::TimeTicks::Now();
  for (auto& obs : observers_) {
    obs.OnArcUiReady(now);
  }
}

void LoginUnlockThroughputRecorder::SetLoginFinishedReportedForTesting() {
  login_finished_reported_ = true;
}

void LoginUnlockThroughputRecorder::MaybeReportLoginFinished() {
  if (!time_compositor_animation_finished_.has_value() ||
      !time_shelf_animation_finished_.has_value()) {
    return;
  }
  if (login_finished_reported_) {
    return;
  }
  login_finished_reported_ = true;

  base::TimeTicks timestamp = std::max(*time_shelf_animation_finished_,
                                       *time_compositor_animation_finished_);

  for (auto& obs : observers_) {
    obs.OnShelfAnimationAndCompositorAnimationDone(timestamp);
  }

  ui_recorder_.OnPostLoginAnimationFinish();
}

void LoginUnlockThroughputRecorder::OnPostLoginDeferredTaskTimerFired() {
  TRACE_EVENT0(
      "startup",
      "LoginUnlockThroughputRecorder::OnPostLoginDeferredTaskTimerFired");

  // `post_login_deferred_task_runner_` could be started in tests in
  // `ScheduleWaitForShelfAnimationEndIfNeeded` where shelf is created
  // before tests fake logins.
  // No `CHECK_IS_TEST()` because there could be longer than 20s animations
  // in production. See http://b/331236941
  if (post_login_deferred_task_runner_->Started()) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  post_login_deferred_task_runner_->Start();
}

void LoginUnlockThroughputRecorder::OnAllWindowsCreated(base::TimeTicks time) {
  for (auto& obs : observers_) {
    obs.OnAllBrowserWindowsCreated(time);
  }
}

void LoginUnlockThroughputRecorder::OnAllWindowsShown(base::TimeTicks time) {
  for (auto& obs : observers_) {
    obs.OnAllBrowserWindowsShown(time);
  }
}

void LoginUnlockThroughputRecorder::OnAllWindowsPresented(
    base::TimeTicks time) {
  for (auto& obs : observers_) {
    obs.OnAllBrowserWindowsPresented(time);
  }

  DCHECK(!time_window_restore_done_.has_value());
  time_window_restore_done_ = time;
  ScheduleWaitForShelfAnimationEndIfNeeded();
}

}  // namespace ash
