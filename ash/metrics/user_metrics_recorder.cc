// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/user_metrics_recorder.h"

#include <memory>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/metrics/desktop_task_switch_metric_recorder.h"
#include "ash/metrics/pointer_metrics_recorder.h"
#include "ash/metrics/stylus_metrics_recorder.h"
#include "ash/metrics/touch_usage_metrics_recorder.h"
#include "ash/metrics/wm_feature_metrics_recorder.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Time between calls to "RecordPeriodicMetrics".
constexpr base::TimeDelta kRecordPeriodicMetricsInterval = base::Minutes(30);

// Returns true if kiosk mode is active.
bool IsKioskModeActive() {
  return Shell::Get()->session_controller()->login_status() ==
         LoginStatus::KIOSK_APP;
}

// Returns true if there is an active user and their session isn't currently
// locked.
bool IsUserActive() {
  SessionControllerImpl* session = Shell::Get()->session_controller();
  return session->IsActiveUserSessionStarted() && !session->IsScreenLocked();
}

// Records the number of items in the shelf as an UMA statistic.
void RecordShelfItemCounts() {
  int pinned_item_count = 0;
  int unpinned_item_count = 0;
  for (const ShelfItem& item : ShelfModel::Get()->items()) {
    if (item.type == TYPE_PINNED_APP || item.type == TYPE_BROWSER_SHORTCUT)
      ++pinned_item_count;
    else
      ++unpinned_item_count;
  }

  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfItems",
                           pinned_item_count + unpinned_item_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfPinnedItems", pinned_item_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfUnpinnedItems",
                           unpinned_item_count);
}

}  // namespace

UserMetricsRecorder::UserMetricsRecorder()
    : wm_feature_metrics_recorder_(
          std::make_unique<WMFeatureMetricsRecorder>()) {
  StartTimer();
  login_metrics_recorder_ = std::make_unique<LoginMetricsRecorder>();
}

UserMetricsRecorder::UserMetricsRecorder(bool record_periodic_metrics) {
  if (record_periodic_metrics)
    StartTimer();
}

UserMetricsRecorder::~UserMetricsRecorder() {
  timer_.Stop();
}

// static
void UserMetricsRecorder::RecordUserClickOnTray(
    LoginMetricsRecorder::TrayClickTarget target) {
  LoginMetricsRecorder* recorder =
      Shell::Get()->metrics()->login_metrics_recorder();
  recorder->RecordUserTrayClick(target);
}

// static
void UserMetricsRecorder::RecordUserClickOnShelfButton(
    LoginMetricsRecorder::ShelfButtonClickTarget target) {
  LoginMetricsRecorder* recorder =
      Shell::Get()->metrics()->login_metrics_recorder();
  recorder->RecordUserShelfButtonClick(target);
}

void UserMetricsRecorder::StartDemoSessionMetricsRecording() {
  demo_session_metrics_recorder_ =
      std::make_unique<DemoSessionMetricsRecorder>();
  Shell::Get()->AddPreTargetHandler(demo_session_metrics_recorder_.get());
}

void UserMetricsRecorder::OnShellInitialized() {
  // Lazy creation of the DesktopTaskSwitchMetricRecorder because it accesses
  // Shell::Get() which is not available when |this| is instantiated.
  if (!desktop_task_switch_metric_recorder_) {
    desktop_task_switch_metric_recorder_ =
        std::make_unique<DesktopTaskSwitchMetricRecorder>();
  }
  pointer_metrics_recorder_ = std::make_unique<PointerMetricsRecorder>();
  touch_usage_metrics_recorder_ = std::make_unique<TouchUsageMetricsRecorder>();
  stylus_metrics_recorder_ = std::make_unique<StylusMetricsRecorder>();
}

void UserMetricsRecorder::OnShellShuttingDown() {
  // Doing the nullptr check as the recorder is not initialized outside demo
  // session. It was initialized during StartDemoSessionMetricsRecording().
  if (demo_session_metrics_recorder_ != nullptr) {
    Shell::Get()->RemovePreTargetHandler(demo_session_metrics_recorder_.get());
    demo_session_metrics_recorder_.reset();
  }
  desktop_task_switch_metric_recorder_.reset();

  // To clean up `pointer_metrics_recorder_`, `touch_usage_metrics_recorder_`
  // and `stylus_metrics_recorder_`, `wm_feature_metrics_recorder_` properly, a
  // valid shell instance is required, so explicitly delete them before the
  // shell instance becomes invalid.
  pointer_metrics_recorder_.reset();
  touch_usage_metrics_recorder_.reset();
  stylus_metrics_recorder_.reset();
  wm_feature_metrics_recorder_.reset();
}

void UserMetricsRecorder::RecordPeriodicMetrics() {
  if (IsUserInActiveDesktopEnvironment()) {
    RecordShelfItemCounts();
    RecordPeriodicAppListMetrics();
    wm_feature_metrics_recorder_->RecordPeriodicalWMMetrics();
  }
}

bool UserMetricsRecorder::IsUserInActiveDesktopEnvironment() const {
  return IsUserActive() && !IsKioskModeActive();
}

void UserMetricsRecorder::StartTimer() {
  timer_.Start(FROM_HERE, kRecordPeriodicMetricsInterval, this,
               &UserMetricsRecorder::RecordPeriodicMetrics);
}

}  // namespace ash
