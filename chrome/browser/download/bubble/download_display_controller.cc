// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_display_controller.h"

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"

namespace {

using DownloadIconState = download::DownloadIconState;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;

// The amount of time for the toolbar icon to be visible after a download is
// completed.
constexpr base::TimeDelta kToolbarIconVisibilityTimeInterval =
    base::Minutes(60);

// The amount of time for the toolbar icon to stay active after a download is
// completed. If the download completed while full screen, the timer is started
// after user comes out of the full screen.
constexpr base::TimeDelta kToolbarIconActiveTimeInterval = base::Minutes(1);

// Information extracted from iterating over all models, to avoid having to do
// so multiple times.
struct AllDownloadUIModelsInfo {
  // Whether there are any downloads actively doing deep scanning.
  bool has_deep_scanning = false;
  // Whether any downloads are unactioned.
  bool has_unactioned = false;
  // From the button UI's perspective, whether the download is considered in
  // progress. Consider dangerous downloads as completed, because we don't want
  // to encourage users to interact with them.
  int in_progress_count = 0;
  // Count of in-progress downloads (by the above definition) that are paused.
  int paused_count = 0;
};

AllDownloadUIModelsInfo GetAllModelsInfo(
    const std::vector<DownloadUIModelPtr>& all_models) {
  AllDownloadUIModelsInfo info;
  for (const auto& model : all_models) {
    if (model->GetDangerType() ==
            download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
        model->GetState() != download::DownloadItem::CANCELLED) {
      info.has_deep_scanning = true;
    }
    if (!model->WasActionedOn()) {
      info.has_unactioned = true;
    }
    if (IsModelInProgress(model.get())) {
      ++info.in_progress_count;
      if (model->IsPaused()) {
        ++info.paused_count;
      }
    }
  }
  return info;
}

// Whether there are no more in-progress downloads (by the above definition)
// that are not paused, i.e., whether all actively downloading items are done.
bool IsAllDone(const AllDownloadUIModelsInfo& info) {
  return info.in_progress_count == info.paused_count;
}

}  // namespace

DownloadDisplayController::DownloadDisplayController(
    DownloadDisplay* display,
    Browser* browser,
    DownloadBubbleUIController* bubble_controller)
    : display_(display),
      browser_(browser),
      download_manager_(browser_->profile()->GetDownloadManager()),
      bubble_controller_(bubble_controller) {
  bubble_controller_->SetDownloadDisplayController(this);
  // |display| can be null in tests.
  if (display) {
    MaybeShowButtonWhenCreated();
  }
  base::PowerMonitor::AddPowerSuspendObserver(this);
}

DownloadDisplayController::~DownloadDisplayController() {
  base::PowerMonitor::RemovePowerSuspendObserver(this);
}

void DownloadDisplayController::OnNewItem(bool show_animation) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }

  UpdateButtonStateFromAllModels(true);
  if (display_->IsFullscreenWithParentViewHidden()) {
    fullscreen_notification_shown_ = true;
    ExclusiveAccessContext* exclusive_access_context =
        browser_->exclusive_access_manager()->context();
    // exclusive_access_context can be null in tests.
    if (exclusive_access_context) {
      exclusive_access_context->UpdateExclusiveAccessExitBubbleContent(
          GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
          ExclusiveAccessBubbleHideCallback(),
          /*notify_download=*/true,
          /*force_update=*/true);
    }
  } else {
    display_->UpdateDownloadIcon(show_animation);
  }
}

void DownloadDisplayController::OnUpdatedItem(bool is_done,
                                              bool may_show_details) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  std::vector<DownloadUIModelPtr> all_models =
      UpdateButtonStateFromAllModels(true);
  AllDownloadUIModelsInfo info = GetAllModelsInfo(all_models);
  bool will_show_details = may_show_details && is_done && IsAllDone(info);
  if (is_done) {
    ScheduleToolbarDisappearance(kToolbarIconVisibilityTimeInterval);
  }
  if (will_show_details && display_->IsFullscreenWithParentViewHidden()) {
    // Suppress the complete event for now because the parent view is
    // hidden.
    details_shown_while_fullscreen_ = true;
    will_show_details = false;
  }
  if (will_show_details) {
    display_->ShowDetails();
  }
}

void DownloadDisplayController::OnRemovedItem(const ContentId& id) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  std::vector<DownloadUIModelPtr> all_models =
      UpdateButtonStateFromAllModels(true);
  // Hide the button if there is only one download item left and that item is
  // about to be removed.
  if (all_models.size() == 1 && all_models[0]->GetContentId() == id) {
    HideToolbarButton();
    return;
  }
}

void DownloadDisplayController::OnButtonPressed() {
  DownloadUIController* download_ui_controller =
      DownloadCoreServiceFactory::GetForBrowserContext(
          browser_->profile()->GetOriginalProfile())
          ->GetDownloadUIController();
  if (download_ui_controller) {
    download_ui_controller->OnButtonClicked();
  }
}

void DownloadDisplayController::HandleButtonPressed() {
  // If the current state is kComplete, set the icon to inactive because of the
  // user action.
  if (icon_info_.icon_state == DownloadIconState::kComplete) {
    icon_info_.is_active = false;
  }
  display_->UpdateDownloadIcon(/*show_animation=*/false);
}

void DownloadDisplayController::ShowToolbarButton() {
  if (!display_->IsShowing()) {
    display_->Enable();
    display_->Show();
  }
}

void DownloadDisplayController::HideToolbarButton() {
  if (display_->IsShowing()) {
    display_->Hide();
  }
}

void DownloadDisplayController::HideBubble() {
  if (display_->IsShowingDetails()) {
    display_->HideDetails();
  }
}

void DownloadDisplayController::ListenToFullScreenChanges() {
  observation_.Observe(
      browser_->exclusive_access_manager()->fullscreen_controller());
}

void DownloadDisplayController::OnFullscreenStateChanged() {
  if ((!fullscreen_notification_shown_ && !details_shown_while_fullscreen_) ||
      display_->IsFullscreenWithParentViewHidden()) {
    return;
  }
  fullscreen_notification_shown_ = false;

  UpdateButtonStateFromAllModels(true);
  if (download::ShouldShowDownloadBubble(browser_->profile()) &&
      details_shown_while_fullscreen_) {
    display_->ShowDetails();
    details_shown_while_fullscreen_ = false;
  }
}

void DownloadDisplayController::OnResume() {
  UpdateButtonStateFromAllModels(true);
}

void DownloadDisplayController::UpdateToolbarButtonState(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  if (all_models.empty()) {
    HideToolbarButton();
    return;
  }
  AllDownloadUIModelsInfo info = GetAllModelsInfo(all_models);
  base::Time last_complete_time = GetLastCompleteTime(all_models);

  if (info.in_progress_count > 0) {
    icon_info_.icon_state = DownloadIconState::kProgress;
    icon_info_.is_active = info.paused_count < info.in_progress_count;
  } else {
    icon_info_.icon_state = DownloadIconState::kComplete;
    bool complete_unactioned =
        HasRecentCompleteDownload(kToolbarIconActiveTimeInterval,
                                  last_complete_time) &&
        info.has_unactioned;
    bool exited_fullscreen_owed_details =
        !display_->IsFullscreenWithParentViewHidden() &&
        details_shown_while_fullscreen_;
    if (complete_unactioned || exited_fullscreen_owed_details) {
      icon_info_.is_active = true;
      ScheduleToolbarInactive(kToolbarIconActiveTimeInterval);
    } else {
      icon_info_.is_active = false;
    }
  }

  if (info.has_deep_scanning) {
    icon_info_.icon_state = DownloadIconState::kDeepScanning;
  }

  if (icon_info_.icon_state != DownloadIconState::kComplete ||
      HasRecentCompleteDownload(kToolbarIconVisibilityTimeInterval,
                                last_complete_time)) {
    ShowToolbarButton();
  }
  display_->UpdateDownloadIcon(/*show_animation=*/false);
}

void DownloadDisplayController::UpdateDownloadIconToInactive() {
  icon_info_.is_active = false;
  display_->UpdateDownloadIcon(/*show_animation=*/false);
}

std::vector<DownloadUIModelPtr>
DownloadDisplayController::UpdateButtonStateFromAllModels(bool may_retry) {
  std::vector<std::unique_ptr<DownloadUIModel>> all_models;
  bool results_complete =
      bubble_controller_->update_service()->GetAllModelsToDisplay(all_models);
  UpdateToolbarButtonState(all_models);
  if (!results_complete && may_retry) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(
                &DownloadDisplayController::UpdateButtonStateFromAllModels),
            weak_factory_.GetWeakPtr(), /*may_retry=*/false));
  }
  return all_models;
}

void DownloadDisplayController::ScheduleToolbarDisappearance(
    base::TimeDelta interval) {
  icon_disappearance_timer_.Stop();
  icon_disappearance_timer_.Start(
      FROM_HERE, interval, this, &DownloadDisplayController::HideToolbarButton);
}

void DownloadDisplayController::ScheduleToolbarInactive(
    base::TimeDelta interval) {
  icon_inactive_timer_.Stop();
  icon_inactive_timer_.Start(
      FROM_HERE, interval, this,
      &DownloadDisplayController::UpdateDownloadIconToInactive);
}

base::Time DownloadDisplayController::GetLastCompleteTime(
    const std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  base::Time last_time = DownloadPrefs::FromDownloadManager(download_manager_)
                             ->GetLastCompleteTime();
  for (const auto& model : all_models) {
    if (last_time < model->GetEndTime()) {
      last_time = model->GetEndTime();
    }
  }
  return last_time;
}

void DownloadDisplayController::MaybeShowButtonWhenCreated() {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }

  std::vector<DownloadUIModelPtr> all_models =
      UpdateButtonStateFromAllModels(true);
  if (display_->IsShowing()) {
    ScheduleToolbarDisappearance(
        kToolbarIconVisibilityTimeInterval -
        (base::Time::Now() - GetLastCompleteTime(all_models)));
  }
}

bool DownloadDisplayController::HasRecentCompleteDownload(
    base::TimeDelta interval,
    base::Time last_complete_time) {
  base::Time current_time = base::Time::Now();
  base::TimeDelta time_since_last_completion =
      current_time - last_complete_time;
  // Also check that the current time is not smaller than the last complete
  // time, this can happen if the system clock has moved backward.
  return time_since_last_completion < interval &&
         current_time >= last_complete_time;
}

DownloadDisplayController::IconInfo DownloadDisplayController::GetIconInfo() {
  return icon_info_;
}

bool DownloadDisplayController::IsDisplayShowingDetails() {
  return display_->IsShowingDetails();
}

DownloadDisplayController::ProgressInfo
DownloadDisplayController::GetProgress() {
  return bubble_controller_->update_service()->GetProgressInfo();
}
