// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/login_unlock_throughput_recorder.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/metrics/login_event_recorder.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/total_animation_throughput_reporter.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"

namespace ash {
namespace {

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

  base::raw_ptr<ShelfView> shelf_view_;
  base::OnceClosure on_animation_end_;
};

std::string GetDeviceModeSuffix() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode()
             ? "TabletMode"
             : "ClamshellMode";
}

void RecordMetrics(const base::TimeTicks& start,
                   const cc::FrameSequenceMetrics::CustomReportData& data,
                   const char* smoothness_name,
                   const char* jank_name,
                   const char* duration_name) {
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
  base::UmaHistogramPercentage(jank_name + suffix, jank);
  // TODO(crbug.com/1143898): Deprecate this metrics once the login/unlock
  // performance issue is resolved.
  base::UmaHistogramCustomTimes(duration_name + suffix,
                                base::Milliseconds(duration_ms),
                                base::Milliseconds(100), base::Seconds(5), 50);
}

void ReportLogin(base::TimeTicks start,
                 const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected) {
    LOG(WARNING) << "Zero frames expected in login animation throughput data";
    return;
  }

  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(
      "LoginAnimationEnd",
      /*send_to_uma=*/false,
      /*write_to_file=*/false);
  chromeos::LoginEventRecorder::Get()->RunScheduledWriteLoginTimes();
  RecordMetrics(start, data, "Ash.LoginAnimation.Smoothness.",
                "Ash.LoginAnimation.Jank.", "Ash.LoginAnimation.Duration.");
}

void ReportUnlock(base::TimeTicks start,
                  const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected) {
    LOG(WARNING) << "Zero frames expected in unlock animation throughput data";
    return;
  }
  RecordMetrics(start, data, "Ash.UnlockAnimation.Smoothness.",
                "Ash.UnlockAnimation.Jank.", "Ash.UnlockAnimation.Duration.");
}

void OnRestoredWindowPresentationTimeReceived(
    int restore_window_id,
    base::TimeTicks presentation_timestamp) {
  LoginUnlockThroughputRecorder* throughput_recorder =
      Shell::Get()->login_unlock_throughput_recorder();
  throughput_recorder->OnRestoredWindowPresented(restore_window_id);
}

}  // namespace

LoginUnlockThroughputRecorder::LoginUnlockThroughputRecorder() {
  Shell::Get()->session_controller()->AddObserver(this);
  chromeos::LoginState::Get()->AddObserver(this);
}

LoginUnlockThroughputRecorder::~LoginUnlockThroughputRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  chromeos::LoginState::Get()->RemoveObserver(this);
}

void LoginUnlockThroughputRecorder::OnLockStateChanged(bool locked) {
  auto logged_in_user = chromeos::LoginState::Get()->GetLoggedInUserType();

  if (!locked &&
      (logged_in_user == chromeos::LoginState::LOGGED_IN_USER_OWNER ||
       logged_in_user == chromeos::LoginState::LOGGED_IN_USER_REGULAR)) {
    auto* primary_root = Shell::GetPrimaryRootWindow();
    new ui::TotalAnimationThroughputReporter(
        primary_root->GetHost()->compositor(),
        base::BindOnce(&ReportUnlock, base::TimeTicks::Now()),
        /*should_delete=*/true);
  }
}

void LoginUnlockThroughputRecorder::LoggedInStateChanged() {
  auto* login_state = chromeos::LoginState::Get();
  auto logged_in_user = login_state->GetLoggedInUserType();

  if (user_logged_in_)
    return;

  if (!login_state->IsUserLoggedIn())
    return;

  if (logged_in_user != chromeos::LoginState::LOGGED_IN_USER_OWNER &&
      logged_in_user != chromeos::LoginState::LOGGED_IN_USER_REGULAR) {
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
      if (app_id.empty() || app_id == app_constants::kLacrosAppId)
        windows_to_restore_.insert(restore_window_id);

      break;
    default:
      NOTREACHED();
  }
}

void LoginUnlockThroughputRecorder::OnRestoredWindowCreated(
    int restore_window_id) {
  auto it = windows_to_restore_.find(restore_window_id);
  if (it == windows_to_restore_.end())
    return;
  windows_to_restore_.erase(it);
  if (windows_to_restore_.empty() && !primary_user_logged_in_.is_null()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - primary_user_logged_in_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Ash.LoginSessionRestore.AllBrowserWindowsCreated", duration_ms,
        base::Milliseconds(1), base::Seconds(100), 100);
  }
  restore_windows_not_shown_.insert(restore_window_id);
}

void LoginUnlockThroughputRecorder::OnBeforeRestoredWindowShown(
    int restore_window_id,
    ui::Compositor* compositor) {
  auto it = restore_windows_not_shown_.find(restore_window_id);
  if (it == restore_windows_not_shown_.end())
    return;

  restore_windows_not_shown_.erase(it);
  if (windows_to_restore_.empty() && restore_windows_not_shown_.empty() &&
      !primary_user_logged_in_.is_null()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - primary_user_logged_in_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.LoginSessionRestore.AllBrowserWindowsShown",
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
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
  if (it == restore_windows_presentation_time_requested_.end())
    return;

  restore_windows_presentation_time_requested_.erase(it);
  if (windows_to_restore_.empty() && restore_windows_not_shown_.empty() &&
      restore_windows_presentation_time_requested_.empty() &&
      !primary_user_logged_in_.is_null()) {
    const base::TimeDelta duration_ms =
        base::TimeTicks::Now() - primary_user_logged_in_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented", duration_ms,
        base::Milliseconds(1), base::Seconds(100), 100);
  }
  restore_windows_presented_.insert(restore_window_id);
}

void LoginUnlockThroughputRecorder::InitShelfIconList(const ShelfModel* model) {
  shelf_initialized_ = true;

  // Copy shelf icons to the expected list.
  for (int index = 0; index < model->item_count(); ++index) {
    const ShelfID& id = model->items()[index].id;
    const ShelfItem& item = model->items()[index];
    if (item.image.isNull())
      expected_shelf_icons_.insert(id);
  }

  if (expected_shelf_icons_.empty())
    OnAllExpectedShelfIconsLoaded();
}

void LoginUnlockThroughputRecorder::UpdateShelfIconList(
    const ShelfModel* model) {
  if (!shelf_initialized_)
    return;

  // Remove IDs that have icons loaded or were already deleted.
  base::flat_set<ShelfID> expected_ids_without_icons;

  for (int index = 0; index < model->item_count(); ++index) {
    const ShelfItem& item = model->items()[index];
    const ShelfID& id = item.id;
    if (!expected_shelf_icons_.contains(id))
      continue;

    if (item.image.isNull())
      expected_ids_without_icons.insert(id);
  }
  expected_shelf_icons_ = expected_ids_without_icons;

  if (expected_shelf_icons_.empty())
    OnAllExpectedShelfIconsLoaded();
}

void LoginUnlockThroughputRecorder::
    ResetScopedThroughputReporterBlockerForTesting() {
  scoped_throughput_reporter_blocker_.reset();
}

void LoginUnlockThroughputRecorder::OnLoginAnimationFinish(
    base::TimeTicks start,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  ui_recorder_.OnPostLoginAnimationFinish();
  ReportLogin(start, data);
}

void LoginUnlockThroughputRecorder::SetShelfViewIfNotSet(
    ShelfView* shelf_view) {
  if (!shelf_view_)
    shelf_view_ = shelf_view;
}

void LoginUnlockThroughputRecorder::ScheduleWaitForShelfAnimationEnd() {
  DCHECK(shelf_view_);
  if (!shelf_view_)
    return;

  base::OnceCallback on_animation_end = base::BindOnce(
      [](base::TimeTicks primary_user_logged_in) {
        const base::TimeDelta duration_ms =
            base::TimeTicks::Now() - primary_user_logged_in;
        UMA_HISTOGRAM_CUSTOM_TIMES(
            "Ash.LoginSessionRestore.ShelfLoginAnimationEnd", duration_ms,
            base::Milliseconds(1), base::Seconds(100), 100);
      },
      primary_user_logged_in_);

  (new AnimationObserver(shelf_view_, on_animation_end))->StartObserving();
}

void LoginUnlockThroughputRecorder::OnAllExpectedShelfIconsLoaded() {
  if (shelf_icons_loaded_)
    return;

  scoped_throughput_reporter_blocker_.reset();

  shelf_icons_loaded_ = true;
  const base::TimeDelta duration_ms =
      base::TimeTicks::Now() - primary_user_logged_in_;
  UMA_HISTOGRAM_CUSTOM_TIMES("Ash.LoginSessionRestore.AllShelfIconsLoaded",
                             duration_ms, base::Milliseconds(1),
                             base::Seconds(100), 100);
  ScheduleWaitForShelfAnimationEnd();
}

}  // namespace ash
