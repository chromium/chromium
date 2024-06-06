// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include <algorithm>
#include <map>
#include <utility>

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
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
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

// Tracing ID and trace events row name.
// This must be a constexpr.
constexpr char kLoginThroughput[] = "LoginThroughput";

// Unit tests often miss initialization and thus we use different label.
constexpr char kLoginThroughputUnordered[] = "LoginThroughput-unordered";

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

std::string GetDeviceModeSuffix() {
  return display::Screen::GetScreen()->InTabletMode() ? "TabletMode"
                                                      : "ClamshellMode";
}

void RecordDurationMetrics(
    const base::TimeTicks& start,
    const cc::FrameSequenceMetrics::CustomReportData& data,
    const char* smoothness_name,
    const char* jank_name,
    const char* duration_name_short,
    const char* duration_name_long) {
  DCHECK(data.frames_expected_v3);

  // Report could happen during Shell shutdown. Early out in that case.
  if (!Shell::HasInstance() || !Shell::Get()->tablet_mode_controller())
    return;

  int duration_ms = (base::TimeTicks::Now() - start).InMilliseconds();
  int smoothness, jank;
  smoothness = metrics_util::CalculateSmoothnessV3(data);
  jank = metrics_util::CalculateJankV3(data);

  std::string suffix = GetDeviceModeSuffix();
  base::UmaHistogramPercentage(smoothness_name + suffix, smoothness);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      smoothness_name + suffix);
  base::UmaHistogramPercentage(jank_name + suffix, jank);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      jank_name + suffix);
  // TODO(crbug.com/1143898): Deprecate this metrics once the login/unlock
  // performance issue is resolved.
  base::UmaHistogramCustomTimes(duration_name_short + suffix,
                                base::Milliseconds(duration_ms),
                                base::Milliseconds(100), base::Seconds(5), 50);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      duration_name_short + suffix);

  base::UmaHistogramCustomTimes(
      duration_name_long + suffix, base::Milliseconds(duration_ms),
      base::Milliseconds(100), base::Seconds(30), 100);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      duration_name_long + suffix);
}

void ReportLoginTotalAnimationThroughput(
    base::TimeTicks start,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected_v3) {
    LOG(WARNING) << "Zero frames expected in login animation throughput data";
    return;
  }

  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginAnimationEnd",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      "LoginAnimationEnd");
  // TODO(b/297957283): Deprecate Ash.LoginAnimation.Duration after M122.
  RecordDurationMetrics(
      start, data, "Ash.LoginAnimation.Smoothness.", "Ash.LoginAnimation.Jank.",
      "Ash.LoginAnimation.Duration.", "Ash.LoginAnimation.Duration2.");
}

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

void WindowRestoreTracker::Init(base::OnceClosure on_all_window_created,
                                base::OnceClosure on_all_window_shown,
                                base::OnceClosure on_all_window_presented) {
  on_created_ = std::move(on_all_window_created);
  on_shown_ = std::move(on_all_window_shown);
  on_presented_ = std::move(on_all_window_presented);
}

int WindowRestoreTracker::NumberOfWindows() const {
  return windows_.size();
}

void WindowRestoreTracker::AddWindow(int window_id, const std::string& app_id) {
  DCHECK(window_id);
  if (app_id.empty() || app_id == app_constants::kLacrosAppId) {
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
    std::move(on_created_).Run();
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

  if (compositor &&
      display::Screen::GetScreen()->GetPrimaryDisplay().detected()) {
    compositor->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(&WindowRestoreTracker::OnCompositorFramePresented,
                       weak_ptr_factory_.GetWeakPtr(), window_id));
  } else if (compositor) {
    // Primary display not detected. Assume it's a headless unit.
    OnPresented(window_id);
  }

  const bool all_shown = CountWindowsInState(State::kNotCreated) == 0 &&
                         CountWindowsInState(State::kCreated) == 0;
  if (all_shown && on_shown_) {
    std::move(on_shown_).Run();
  }
}

void WindowRestoreTracker::OnPresentedForTesting(int window_id) {
  OnPresented(window_id);
}

void WindowRestoreTracker::OnCompositorFramePresented(
    int window_id,
    const viz::FrameTimingDetails& details) {
  OnPresented(window_id);
}

void WindowRestoreTracker::OnPresented(int window_id) {
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
    std::move(on_presented_).Run();
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
              base::SequencedTaskRunner::GetCurrentDefault())) {
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

LoginUnlockThroughputRecorder::TimeMarker::TimeMarker(const std::string& name)
    : name_(name) {}

void LoginUnlockThroughputRecorder::EnsureTracingSliceNamed() {
  // EnsureTracingSliceNamed() should be called only on expected events.
  // If login ThroughputRecording did not start with either OnAuthSuccess
  // or LoggedInStateChanged the tracing slice will have the "-unordered"
  // suffix.
  //
  // Depending on the login flow this function may get called multiple times.
  if (login_time_markers_.empty()) {
    // The first event will name the tracing row.
    AddLoginTimeMarker(kLoginThroughput);
  }
}

void LoginUnlockThroughputRecorder::OnAuthSuccess() {
  EnsureTracingSliceNamed();
  timestamp_on_auth_success_ = base::TimeTicks::Now();
  AddLoginTimeMarker("OnAuthSuccess");
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

  EnsureTracingSliceNamed();
  timestamp_primary_user_logged_in_ = base::TimeTicks::Now();
  AddLoginTimeMarker("UserLoggedIn");

  if (logged_in_user != LoginState::LOGGED_IN_USER_OWNER &&
      logged_in_user != LoginState::LOGGED_IN_USER_REGULAR) {
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

  // Report UserLoggedIn histogram if we had OnAuthSuccess() event previously.
  if (timestamp_on_auth_success_.has_value()) {
    const base::TimeDelta duration =
        base::TimeTicks::Now() - timestamp_on_auth_success_.value();
    base::UmaHistogramTimes("Ash.Login.LoggedInStateChanged", duration);
  }

  ui_recorder_.OnUserLoggedIn();
  auto* primary_root = Shell::GetPrimaryRootWindow();

  auto* rec = new ui::TotalAnimationThroughputReporter(
      primary_root->GetHost()->compositor(),
      base::BindOnce(
          &LoginUnlockThroughputRecorder::OnCompositorAnimationFinished,
          weak_ptr_factory_.GetWeakPtr(),
          timestamp_primary_user_logged_in_.value()),
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
    base::TimeTicks start,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  ReportLoginTotalAnimationThroughput(start, data);

  login_animation_throughput_received_ = true;
  MaybeReportLoginFinished();
}

void LoginUnlockThroughputRecorder::OnArcOptedIn() {
  arc_opt_in_time_ = base::TimeTicks::Now();
}

void LoginUnlockThroughputRecorder::OnArcAppListReady() {
  if (arc_app_list_ready_reported_)
    return;

  // |Ash.ArcAppInitialAppsInstallDuration| histogram is only reported for
  // the first user session after they opted into the ARC++.
  // |arc_opt_in_time_| will only have value if user opted in into the ARC++
  // in this session (in this browser instance).
  if (arc_opt_in_time_.has_value()) {
    const auto duration = base::TimeTicks::Now() - arc_opt_in_time_.value();
    UmaHistogramCustomTimes("Ash.ArcAppInitialAppsInstallDuration", duration,
                            base::Seconds(1) /* min */,
                            base::Hours(1) /* max */, 100 /* buckets */);
  }

  arc_app_list_ready_reported_ = true;
}

bool LoginUnlockThroughputRecorder::NeedReportArcAppListReady() const {
  return arc_opt_in_time_.has_value() && !arc_app_list_ready_reported_;
}

void LoginUnlockThroughputRecorder::ScheduleWaitForShelfAnimationEndIfNeeded() {
  // If not ready yet, do nothing this time.
  if (!window_restore_done_ || !shelf_icons_loaded_) {
    return;
  }

  DCHECK(!dcheck_shelf_animation_end_scheduled_);
  dcheck_shelf_animation_end_scheduled_ = true;

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

        const base::TimeDelta duration_ms =
            base::TimeTicks::Now() -
            self->timestamp_primary_user_logged_in_.value();
        constexpr char kAshLoginSessionRestoreShelfLoginAnimationEnd[] =
            "Ash.LoginSessionRestore.ShelfLoginAnimationEnd";
        UMA_HISTOGRAM_CUSTOM_TIMES(
            kAshLoginSessionRestoreShelfLoginAnimationEnd, duration_ms,
            base::Milliseconds(1), base::Seconds(100), 100);
        ash::Shell::Get()
            ->login_unlock_throughput_recorder()
            ->AddLoginTimeMarker(kAshLoginSessionRestoreShelfLoginAnimationEnd);

        self->shelf_animation_finished_ = true;
        self->MaybeReportLoginFinished();
      },
      weak_ptr_factory_.GetWeakPtr());

  (new ShelfAnimationObserver(on_shelf_animation_end))->StartObserving();

  // Unblock deferred task now.
  // TODO(b/328339021, b/323098858): This is the mitigation against a bug
  // that animation observation has race condition.
  // Can be in a part of better architecture.
  AddLoginTimeMarker("BootTime.Login4");
  base::UmaHistogramCustomTimes(
      "BootTime.Login4",
      base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value(),
      base::Milliseconds(100), base::Seconds(100), 100);
  post_login_deferred_task_timer_.Stop();
  if (!post_login_deferred_task_runner_->Started()) {
    post_login_deferred_task_runner_->Start();
  }
}

void LoginUnlockThroughputRecorder::OnAllExpectedShelfIconsLoaded() {
  DCHECK(!shelf_icons_loaded_);
  shelf_icons_loaded_ = true;

  if (timestamp_primary_user_logged_in_.has_value()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value();
    constexpr char kAshLoginSessionRestoreAllShelfIconsLoaded[] =
        "Ash.LoginSessionRestore.AllShelfIconsLoaded";
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllShelfIconsLoaded,
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllShelfIconsLoaded);
  }

  ScheduleWaitForShelfAnimationEndIfNeeded();
}

void LoginUnlockThroughputRecorder::AddLoginTimeMarker(
    const std::string& marker_name) {
  // Unit tests often miss the full initialization flow so we use a
  // different label in this case.
  if (login_time_markers_.empty() && marker_name != kLoginThroughput) {
    login_time_markers_.emplace_back(kLoginThroughputUnordered);

    const base::TimeTicks begin = login_time_markers_.front().time();
    const base::TimeTicks end = begin;

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "startup", kLoginThroughputUnordered, TRACE_ID_LOCAL(kLoginThroughput),
        begin);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "startup", kLoginThroughputUnordered, TRACE_ID_LOCAL(kLoginThroughput),
        end);
  }

  login_time_markers_.emplace_back(marker_name);
  bool reported = false;

#define REPORT_LOGIN_THROUGHPUT_EVENT(metric)                        \
  if (marker_name == metric) {                                       \
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(                \
        "startup", metric, TRACE_ID_LOCAL(kLoginThroughput), begin); \
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(                  \
        "startup", metric, TRACE_ID_LOCAL(kLoginThroughput), end);   \
    reported = true;                                                 \
  }                                                                  \
  class __STUB__

  if (login_time_markers_.size() > 1) {
    const base::TimeTicks begin =
        login_time_markers_[login_time_markers_.size() - 2].time();
    const base::TimeTicks end =
        login_time_markers_[login_time_markers_.size() - 1].time();

    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllBrowserWindowsCreated");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllBrowserWindowsShown");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllShelfIconsLoaded");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.ShelfLoginAnimationEnd");
    REPORT_LOGIN_THROUGHPUT_EVENT("LoginAnimationEnd");
    REPORT_LOGIN_THROUGHPUT_EVENT("LoginFinished");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginAnimation.Smoothness.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Smoothness.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Jank.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Jank.TabletMode");
    // TODO(b/297957283): Deprecate
    // Ash.LoginAnimation.Duration.{TabletMode,ClamshellMode} after M122.
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Duration.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Duration.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Duration2.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Duration2.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("BootTime.Login2");
    REPORT_LOGIN_THROUGHPUT_EVENT("BootTime.Login3");
    REPORT_LOGIN_THROUGHPUT_EVENT("BootTime.Login4");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.UnlockAnimation.Smoothness.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.UnlockAnimation.Smoothness.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("ArcUiAvailable");
    REPORT_LOGIN_THROUGHPUT_EVENT("OnAuthSuccess");
    REPORT_LOGIN_THROUGHPUT_EVENT("UserLoggedIn");
    if (!reported) {
      constexpr char kFailedEvent[] = "FailedToReportEvent";
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "startup", kFailedEvent, TRACE_ID_LOCAL(kLoginThroughput), begin);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "startup", kFailedEvent, TRACE_ID_LOCAL(kLoginThroughput), end);
    }
  } else {
    // The first event will be used as a row name in the tracing UI.
    const base::TimeTicks begin = login_time_markers_.front().time();
    const base::TimeTicks end = begin;

    REPORT_LOGIN_THROUGHPUT_EVENT(kLoginThroughput);
  }
#undef REPORT_LOGIN_THROUGHPUT_EVENT
  DCHECK(reported) << "Failed to report " << marker_name
                   << ", login_time_markers_.size()="
                   << login_time_markers_.size();
}

void LoginUnlockThroughputRecorder::BrowserSessionRestoreDataLoaded(
    std::vector<RestoreWindowID> window_ids) {
  if (login_finished_reported_) {
    return;
  }

  if (browser_session_restore_data_loaded_) {
    // This may be called twice after login but before
    // `login_finished_reported_` for some reasons (e.g. errors.) Normally in
    // that case, the set of windows should be the same as the first one. So, we
    // only track the first set of windows.
    //
    // In some tests, session restore seems to be performed multiple times with
    // different sets of windows, but we also ignore such cases because those
    // tests are not very related to login performance.
    return;
  }

  for (const auto& w : window_ids) {
    window_restore_tracker_.AddWindow(w.session_window_id, w.app_name);
  }

  browser_session_restore_data_loaded_ = true;
  MaybeRestoreDataLoaded();
}

void LoginUnlockThroughputRecorder::FullSessionRestoreDataLoaded(
    std::vector<RestoreWindowID> window_ids) {
  if (login_finished_reported_) {
    return;
  }

  for (const auto& w : window_ids) {
    window_restore_tracker_.AddWindow(w.session_window_id, w.app_name);
  }

  DCHECK(!full_session_restore_data_loaded_);
  full_session_restore_data_loaded_ = true;
  MaybeRestoreDataLoaded();
}

void LoginUnlockThroughputRecorder::ArcUiAvailableAfterLogin() {
  AddLoginTimeMarker("ArcUiAvailable");

  // It seems that neither `OnAuthSuccess` nor `LoggedInStateChanged` is called
  // on some ARC tests.
  if (!timestamp_primary_user_logged_in_.has_value()) {
    return;
  }

  const base::TimeDelta duration =
      base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value();
  base::UmaHistogramCustomTimes("Ash.Login.ArcUiAvailableAfterLogin.Duration",
                                duration, base::Milliseconds(100),
                                base::Seconds(30), 100);
  LOCAL_HISTOGRAM_TIMES("Ash.Tast.ArcUiAvailableAfterLogin.Duration", duration);
}

void LoginUnlockThroughputRecorder::SetLoginFinishedReportedForTesting() {
  login_finished_reported_ = true;
}

void LoginUnlockThroughputRecorder::MaybeReportLoginFinished() {
  if (!login_animation_throughput_received_ || !shelf_animation_finished_) {
    return;
  }
  if (login_finished_reported_) {
    return;
  }
  login_finished_reported_ = true;

  ui_recorder_.OnPostLoginAnimationFinish();

  AddLoginTimeMarker("LoginFinished");
  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginFinished",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);

  AddLoginTimeMarker("BootTime.Login3");
  base::UmaHistogramCustomTimes(
      "BootTime.Login3",
      base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value(),
      base::Milliseconds(100), base::Seconds(100), 100);

  LoginEventRecorder::Get()->RunScheduledWriteLoginTimes();
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

void LoginUnlockThroughputRecorder::MaybeRestoreDataLoaded() {
  if (!browser_session_restore_data_loaded_ ||
      !full_session_restore_data_loaded_) {
    return;
  }

  // Now the set of the windows to be restored should be fixed. If no window is
  // added to the tracker so far, we consider window restore has been done.
  if (window_restore_tracker_.NumberOfWindows() == 0) {
    DCHECK(!window_restore_done_);
    window_restore_done_ = true;
    shelf_tracker_.IgnoreBrowserIcon();
    ScheduleWaitForShelfAnimationEndIfNeeded();
  }
}

void LoginUnlockThroughputRecorder::OnAllWindowsCreated() {
  if (timestamp_primary_user_logged_in_.has_value()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value();
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsCreated[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsCreated";
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllBrowserWindowsCreated,
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsCreated);
  }
}

void LoginUnlockThroughputRecorder::OnAllWindowsShown() {
  if (timestamp_primary_user_logged_in_.has_value()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value();
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsShown[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsShown";
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.LoginSessionRestore.AllBrowserWindowsShown",
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsShown);
  }
}

void LoginUnlockThroughputRecorder::OnAllWindowsPresented() {
  if (timestamp_primary_user_logged_in_.has_value()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - timestamp_primary_user_logged_in_.value();
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsPresented[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented";
    // Headless units do not report presentation time, so we only report
    // the histogram if primary display is functional.
    if (display::Screen::GetScreen()->GetPrimaryDisplay().detected()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          kAshLoginSessionRestoreAllBrowserWindowsPresented, duration_ms,
          base::Milliseconds(1), base::Seconds(100), 100);
    }
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsPresented);
  }

  DCHECK(!window_restore_done_);
  window_restore_done_ = true;
  ScheduleWaitForShelfAnimationEndIfNeeded();
}

}  // namespace ash
