// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_display_controller.h"

#include "base/numerics/safe_conversions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_model_utils.h"
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

// The amount of time for the toolbar icon to be visible after a download is
// completed.
constexpr base::TimeDelta kToolbarIconVisibilityTimeInterval = base::Hours(24);

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
  // to encourage users to interact with them. However, consider downloads
  // pending scanning as in progress, because we do want users to scan potential
  // dangerous downloads.
  int in_progress_count = 0;
  // Count of in-progress downloads (by the above definition) that are paused.
  int paused_count = 0;
  // Whether there are no more in-progress downloads (by the above definition)
  // that are not paused or pending deep scanning, i.e., whether all actively
  // downloading items are done.
  bool all_done = true;
};

AllDownloadUIModelsInfo GetAllModelsInfo(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
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
      } else if (!IsPendingDeepScanning(model.get())) {
        // An in-progress download (by the above definition) is exactly one of
        // actively downloading, paused, or pending deep scanning. If we got
        // here, it is actively downloading and hence we are not all done.
        info.all_done = false;
      }
    }
  }
  return info;
}

}  // namespace

DownloadDisplayController::DownloadDisplayController(
    DownloadDisplay* display,
    Browser* browser,
    DownloadBubbleUIController* bubble_controller)
    : display_(display),
      browser_(browser),
      download_manager_(browser_->profile()->GetDownloadManager()),
      download_notifier_(download_manager_, this),
      bubble_controller_(bubble_controller) {
  bubble_controller_->InitOfflineItems(
      this,
      base::BindOnce(&DownloadDisplayController::MaybeShowButtonWhenCreated,
                     weak_factory_.GetWeakPtr()));
  base::PowerMonitor::AddPowerSuspendObserver(this);
}

DownloadDisplayController::~DownloadDisplayController() {
  base::PowerMonitor::RemovePowerSuspendObserver(this);
}

void DownloadDisplayController::OnNewItem(bool show_animation) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }

  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  UpdateToolbarButtonState(all_models);
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
                                              bool is_deep_scanning,
                                              bool may_show_details) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  AllDownloadUIModelsInfo info = GetAllModelsInfo(all_models);
  bool will_show_details =
      may_show_details && ((is_done && info.all_done) || is_deep_scanning);
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
  UpdateToolbarButtonState(all_models);
}

void DownloadDisplayController::OnRemovedItem(const ContentId& id) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  // Hide the button if there is only one download item left and that item is
  // about to be removed.
  if (all_models.size() == 1 && all_models[0]->GetContentId() == id) {
    HideToolbarButton();
    return;
  }
  UpdateToolbarButtonState(all_models);
}

void DownloadDisplayController::OnManagerGoingDown(
    content::DownloadManager* manager) {
  if (download_manager_ == manager) {
    download_manager_ = nullptr;
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

  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  UpdateToolbarButtonState(all_models);
  if (download::ShouldShowDownloadBubble(browser_->profile()) &&
      details_shown_while_fullscreen_) {
    display_->ShowDetails();
    details_shown_while_fullscreen_ = false;
  }
}

void DownloadDisplayController::OnResume() {
  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  UpdateToolbarButtonState(all_models);
}

void DownloadDisplayController::UpdateToolbarButtonState(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  if (all_models.empty()) {
    HideToolbarButton();
    return;
  }
  AllDownloadUIModelsInfo info = GetAllModelsInfo(all_models);
  base::Time last_complete_time =
      GetLastCompleteTime(bubble_controller_->GetOfflineItems());

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
    const offline_items_collection::OfflineContentAggregator::OfflineItemList&
        offline_items) {
  base::Time last_time = DownloadPrefs::FromDownloadManager(download_manager_)
                             ->GetLastCompleteTime();
  for (const auto& offline_item : offline_items) {
    if (last_time < offline_item.completion_time)
      last_time = offline_item.completion_time;
  }
  return last_time;
}

void DownloadDisplayController::MaybeShowButtonWhenCreated() {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }

  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  UpdateToolbarButtonState(all_models);
  if (display_->IsShowing()) {
    ScheduleToolbarDisappearance(
        kToolbarIconVisibilityTimeInterval -
        (base::Time::Now() -
         GetLastCompleteTime(bubble_controller_->GetOfflineItems())));
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
  ProgressInfo progress_info;
  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  int64_t received_bytes = 0;
  int64_t total_bytes = 0;

  for (const auto& model : all_models) {
    if (IsModelInProgress(model.get())) {
      ++progress_info.download_count;
      if (model->GetTotalBytes() <= 0) {
        // There may or may not be more data coming down this pipe.
        progress_info.progress_certain = false;
      } else {
        received_bytes += model->GetCompletedBytes();
        total_bytes += model->GetTotalBytes();
      }
    }
  }

  if (total_bytes > 0) {
    progress_info.progress_percentage =
        base::ClampFloor(received_bytes * 100.0 / total_bytes);
  }
  return progress_info;
}
