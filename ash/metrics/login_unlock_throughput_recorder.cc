// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
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
class AnimationObserver : public views::BoundsAnimatorObserver {
 public:
  AnimationObserver(ShelfView* shelf_view, base::OnceClosure& on_animation_end)
      : shelf_view_(shelf_view),
        on_animation_end_(std::move(on_animation_end)) {}

  AnimationObserver(const AnimationObserver&) = delete;
  AnimationObserver& operator=(const AnimationObserver&) = delete;

  ~AnimationObserver() override = default;

  // ShelfViewObserver overrides:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override {}
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override {
    shelf_view_->RemoveAnimationObserver(this);
    RunCallbackAndDestroy();
  }

  void StartObserving() {
    if (shelf_view_->IsAnimating()) {
      shelf_view_->AddAnimationObserver(this);
      return;
    }
    RunCallbackAndDestroy();
  }

 private:
  void RunCallbackAndDestroy() {
    std::move(on_animation_end_).Run();
    delete this;
  }

  raw_ptr<ShelfView, LeakedDanglingUntriaged> shelf_view_;
  base::OnceClosure on_animation_end_;
};

std::string GetDeviceModeSuffix() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode()
             ? "TabletMode"
             : "ClamshellMode";
}

void RecordDurationMetrics(
    const base::TimeTicks& start,
    const cc::FrameSequenceMetrics::CustomReportData& data,
    const char* smoothness_name,
    const char* jank_name,
    const char* duration_name_short,
    const char* duration_name_long) {
  DCHECK(data.frames_expected);

  // Report could happen during Shell shutdown. Early out in that case.
  if (!Shell::HasInstance() || !Shell::Get()->tablet_mode_controller())
    return;

  int duration_ms = (base::TimeTicks::Now() - start).InMilliseconds();
  int smoothness, jank;
  smoothness = metrics_util::CalculateSmoothness(data);
  jank = metrics_util::CalculateJank(data);

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
  if (!data.frames_expected) {
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

void ReportLoginFinished() {
  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginFinished",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      "LoginFinished");
  LoginEventRecorder::Get()->RunScheduledWriteLoginTimes();
}

void RecordSmoothnessMetrics(
    const cc::FrameSequenceMetrics::CustomReportData& data,
    const char* smoothness_name) {
  DCHECK(data.frames_expected);

  // Report could happen during Shell shutdown. Early out in that case.
  if (!Shell::HasInstance() || !Shell::Get()->tablet_mode_controller()) {
    return;
  }

  const int smoothness = metrics_util::CalculateSmoothness(data);

  const std::string suffix = GetDeviceModeSuffix();
  base::UmaHistogramPercentage(smoothness_name + suffix, smoothness);
  ash::Shell::Get()->login_unlock_throughput_recorder()->AddLoginTimeMarker(
      smoothness_name + suffix);
}

void ReportUnlock(const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected) {
    LOG(WARNING) << "Zero frames expected in unlock animation throughput data";
    return;
  }
  RecordSmoothnessMetrics(data, "Ash.UnlockAnimation.Smoothness.");
}

void OnRestoredWindowPresentationTimeReceived(
    int restore_window_id,
    base::TimeTicks presentation_timestamp) {
  LoginUnlockThroughputRecorder* throughput_recorder =
      Shell::Get()->login_unlock_throughput_recorder();
  throughput_recorder->OnRestoredWindowPresented(restore_window_id);
}

bool HasPendingIcon(const ShelfModel* model) {
  return base::ranges::any_of(model->items(), [](const ShelfItem& item) {
    return item.image.isNull();
  });
}

}  // namespace

LoginUnlockThroughputRecorder::LoginUnlockThroughputRecorder() {
  Shell::Get()->session_controller()->AddObserver(this);
  LoginState::Get()->AddObserver(this);
}

LoginUnlockThroughputRecorder::~LoginUnlockThroughputRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  LoginState::Get()->RemoveObserver(this);
}

void LoginUnlockThroughputRecorder::OnLockStateChanged(bool locked) {
  auto logged_in_user = LoginState::Get()->GetLoggedInUserType();

  if (!locked && (logged_in_user == LoginState::LOGGED_IN_USER_OWNER ||
                  logged_in_user == LoginState::LOGGED_IN_USER_REGULAR)) {
    auto* primary_root = Shell::GetPrimaryRootWindow();
    new ui::TotalAnimationThroughputReporter(
        primary_root->GetHost()->compositor(), base::BindOnce(&ReportUnlock),
        /*should_delete=*/true);
  }
}

LoginUnlockThroughputRecorder::TimeMarker::TimeMarker(const std::string& name)
    : name_(name) {}

void LoginUnlockThroughputRecorder::LoggedInStateChanged() {
  auto* login_state = LoginState::Get();
  auto logged_in_user = login_state->GetLoggedInUserType();

  if (user_logged_in_)
    return;

  if (!login_state->IsUserLoggedIn())
    return;

  // The first event will name the tracing row.
  if (login_time_markers_.empty())
    AddLoginTimeMarker(kLoginThroughput);

  if (logged_in_user != LoginState::LOGGED_IN_USER_OWNER &&
      logged_in_user != LoginState::LOGGED_IN_USER_REGULAR) {
    // Kiosk users fall here.
    return;
  }

  user_logged_in_ = true;
  ui_recorder_.OnUserLoggedIn();
  auto* primary_root = Shell::GetPrimaryRootWindow();
  primary_user_logged_in_ = base::TimeTicks::Now();

  auto* rec = new ui::TotalAnimationThroughputReporter(
      primary_root->GetHost()->compositor(),
      base::BindOnce(&LoginUnlockThroughputRecorder::OnLoginAnimationFinish,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      /*should_delete=*/true);
  login_animation_throughput_reporter_ = rec->GetWeakPtr();
  DCHECK(!scoped_throughput_reporter_blocker_);
  // Login animation metrics should not be reported until all shelf icons
  // were loaded.
  scoped_throughput_reporter_blocker_ =
      login_animation_throughput_reporter_->NewScopedBlocker();
}

void LoginUnlockThroughputRecorder::AddScheduledRestoreWindow(
    int restore_window_id,
    const std::string& app_id,
    RestoreWindowType window_type) {
  switch (window_type) {
    case LoginUnlockThroughputRecorder::kBrowser:
      DCHECK(restore_window_id);
      if (app_id.empty() || app_id == app_constants::kLacrosAppId) {
        windows_to_restore_.insert(restore_window_id);
      }
      break;
    default:
      NOTREACHED();
  }
}

void LoginUnlockThroughputRecorder::OnRestoredWindowCreated(
    int restore_window_id) {
  first_restored_window_created_ = true;

  auto it = windows_to_restore_.find(restore_window_id);
  if (it == windows_to_restore_.end()) {
    return;
  }
  windows_to_restore_.erase(it);
  if (windows_to_restore_.empty() && !primary_user_logged_in_.is_null()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - primary_user_logged_in_;
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsCreated[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsCreated";
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllBrowserWindowsCreated,
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsCreated);
  }
  restore_windows_not_shown_.insert(restore_window_id);
}

void LoginUnlockThroughputRecorder::OnBeforeRestoredWindowShown(
    int restore_window_id,
    ui::Compositor* compositor) {
  auto it = restore_windows_not_shown_.find(restore_window_id);
  if (it == restore_windows_not_shown_.end()) {
    return;
  }

  restore_windows_not_shown_.erase(it);
  if (windows_to_restore_.empty() && restore_windows_not_shown_.empty() &&
      !primary_user_logged_in_.is_null()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - primary_user_logged_in_;
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsShown[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsShown";
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.LoginSessionRestore.AllBrowserWindowsShown",
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsShown);
  }

  if (!compositor)
    return;

  restore_windows_presentation_time_requested_.insert(restore_window_id);
  compositor->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
      &OnRestoredWindowPresentationTimeReceived, restore_window_id));
}

void LoginUnlockThroughputRecorder::OnRestoredWindowPresented(
    int restore_window_id) {
  auto it =
      restore_windows_presentation_time_requested_.find(restore_window_id);
  if (it == restore_windows_presentation_time_requested_.end()) {
    return;
  }

  restore_windows_presentation_time_requested_.erase(it);
  if (windows_to_restore_.empty() && restore_windows_not_shown_.empty() &&
      restore_windows_presentation_time_requested_.empty() &&
      !primary_user_logged_in_.is_null()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - primary_user_logged_in_;
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsPresented[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented";
    UMA_HISTOGRAM_CUSTOM_TIMES(
        kAshLoginSessionRestoreAllBrowserWindowsPresented, duration_ms,
        base::Milliseconds(1), base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsPresented);
    all_restored_windows_presented_ = true;
    ScheduleWaitForShelfAnimationEndIfNeeded();
  }
  restore_windows_presented_.insert(restore_window_id);
}

void LoginUnlockThroughputRecorder::InitShelfIconList(const ShelfModel* model) {
  UpdateShelfIconList(model);
}

void LoginUnlockThroughputRecorder::UpdateShelfIconList(
    const ShelfModel* model) {
  shelf_initialized_ = true;

  has_pending_icon_ = HasPendingIcon(model);

  if (has_pending_icon_) {
    return;
  }

  // Internally it will be called again after browser has listed all the
  // windows/apps and added new shelf icons.
  if (!browser_windows_will_not_be_restored_ &&
      !first_restored_window_created_) {
    return;
  }

  // We do not collect this histogram from real users because it's not
  // really universal. It is tied to the flow from the ui.LoginPerf tast test
  // and therefore not listed in the histograms metadata.
  base::UmaHistogramSparse(
      "Ash.LoginSessionRestore.ExpectedShelfIconsInitialNumber",
      model->item_count());

  OnAllExpectedShelfIconsLoaded();
}

void LoginUnlockThroughputRecorder::
    ResetScopedThroughputReporterBlockerForTesting() {
  scoped_throughput_reporter_blocker_.reset();
}

void LoginUnlockThroughputRecorder::OnLoginAnimationFinish(
    base::TimeTicks start,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  login_animation_throughput_received_ = true;
  ReportLoginTotalAnimationThroughput(start, data);
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
  // If shelf icons were just waiting for browser window to be presented,
  // trigger "no more icons are going to be loaded for the session restore".
  if (!shelf_icons_loaded_ &&
      (browser_windows_will_not_be_restored_ ||
       all_restored_windows_presented_) &&
      shelf_initialized_ && !has_pending_icon_) {
    OnAllExpectedShelfIconsLoaded();
  }

  // If not ready yet or report was already scheduled, ignore.
  if (!shelf_icons_loaded_ ||
      (!browser_windows_will_not_be_restored_ &&
       !all_restored_windows_presented_) ||
      shelf_animation_end_scheduled_) {
    return;
  }

  shelf_animation_end_scheduled_ = true;

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

  ShelfView* shelf_view =
      RootWindowController::ForWindow(
          Shell::Get()->window_tree_host_manager()->GetPrimaryRootWindow())
          ->shelf()
          ->hotseat_widget()
          ->scrollable_shelf_view()
          ->shelf_view();
  base::OnceCallback on_animation_end = base::BindOnce(
      [](base::WeakPtr<LoginUnlockThroughputRecorder> self) {
        self->shelf_animation_finished_ = true;
        const base::TimeDelta duration_ms =
            base::TimeTicks::Now() - self->primary_user_logged_in_;
        constexpr char kAshLoginSessionRestoreShelfLoginAnimationEnd[] =
            "Ash.LoginSessionRestore.ShelfLoginAnimationEnd";
        UMA_HISTOGRAM_CUSTOM_TIMES(
            kAshLoginSessionRestoreShelfLoginAnimationEnd, duration_ms,
            base::Milliseconds(1), base::Seconds(100), 100);
        ash::Shell::Get()
            ->login_unlock_throughput_recorder()
            ->AddLoginTimeMarker(kAshLoginSessionRestoreShelfLoginAnimationEnd);
        self->MaybeReportLoginFinished();
      },
      weak_ptr_factory_.GetWeakPtr());

  (new AnimationObserver(shelf_view, on_animation_end))->StartObserving();
}

void LoginUnlockThroughputRecorder::OnAllExpectedShelfIconsLoaded() {
  if (shelf_icons_loaded_ || (!browser_windows_will_not_be_restored_ &&
                              !first_restored_window_created_)) {
    return;
  }

  shelf_icons_loaded_ = true;
  const base::TimeDelta duration_ms =
      base::TimeTicks::Now() - primary_user_logged_in_;
  constexpr char kAshLoginSessionRestoreAllShelfIconsLoaded[] =
      "Ash.LoginSessionRestore.AllShelfIconsLoaded";
  UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllShelfIconsLoaded,
                             duration_ms, base::Milliseconds(1),
                             base::Seconds(100), 100);
  AddLoginTimeMarker(kAshLoginSessionRestoreAllShelfIconsLoaded);
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
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.UnlockAnimation.Smoothness.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.UnlockAnimation.Smoothness.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("ArcUiAvailable");
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

void LoginUnlockThroughputRecorder::RestoreDataLoaded() {
  if (windows_to_restore_.empty()) {
    browser_windows_will_not_be_restored_ = true;
    ScheduleWaitForShelfAnimationEndIfNeeded();
  }
}

void LoginUnlockThroughputRecorder::ArcUiAvailableAfterLogin() {
  AddLoginTimeMarker("ArcUiAvailable");
  const base::TimeDelta duration =
      base::TimeTicks::Now() - primary_user_logged_in_;
  LOCAL_HISTOGRAM_TIMES("Ash.Tast.ArcUiAvailableAfterLogin.Duration", duration);
}

void LoginUnlockThroughputRecorder::MaybeReportLoginFinished() {
  if (login_finished_reported_) {
    return;
  }
  if (!login_animation_throughput_received_ || !shelf_animation_finished_ ||
      (!browser_windows_will_not_be_restored_ &&
       !all_restored_windows_presented_)) {
    return;
  }
  login_finished_reported_ = true;

  ui_recorder_.OnPostLoginAnimationFinish();
  ReportLoginFinished();
}

}  // namespace ash
