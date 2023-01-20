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

// From the button UI's perspective, whether the download is considered in
// progress.
bool IsModelInProgress(const DownloadUIModel* model) {
  // Consider dangerous downloads as completed, because we don't want to
  // encourage users to interact with them. However, consider downloads pending
  // scanning as in progress, because we do want users to scan potential
  // dangerous downloads.
  if (model->IsDangerous() &&
      model->GetDangerType() !=
          download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    return false;
  }
  return model->GetState() == download::DownloadItem::IN_PROGRESS;
}

bool HasDeepScanningDownload(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  for (const auto& model : all_models) {
    if (model->GetDangerType() ==
            download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
        model->GetState() != download::DownloadItem::CANCELLED) {
      return true;
    }
  }
  return false;
}

int InProgressDownloadCount(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  int in_progress_count = 0;
  for (const auto& model : all_models) {
    if (IsModelInProgress(model.get())) {
      in_progress_count++;
    }
  }
  return in_progress_count;
}

int PausedDownloadCount(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  int paused_count = 0;
  for (const auto& model : all_models) {
    if (IsModelInProgress(model.get()) && model->IsPaused()) {
      paused_count++;
    }
  }
  return paused_count;
}

bool HasUnactionedDownload(
    std::vector<std::unique_ptr<DownloadUIModel>>& all_models) {
  for (const auto& model : all_models) {
    if (!model->WasActionedOn()) {
      return true;
    }
  }
  return false;
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

void DownloadDisplayController::OnNewItem(bool show_details) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }

  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  UpdateToolbarButtonState(all_models);
  if (!show_details) {
    return;
  }
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
    display_->ShowDetails();
  }
}

void DownloadDisplayController::OnUpdatedItem(bool is_done,
                                              bool show_details_if_done) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  if (is_done) {
    ScheduleToolbarDisappearance(kToolbarIconVisibilityTimeInterval);
    if (show_details_if_done && display_->IsFullscreenWithParentViewHidden()) {
      // Suppress the complete event for now because the parent view is
      // hidden.
      download_completed_while_fullscreen_ = true;
    }
  }
  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
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
  display_->UpdateDownloadIcon();
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
  if (!fullscreen_notification_shown_ ||
      display_->IsFullscreenWithParentViewHidden()) {
    return;
  }
  fullscreen_notification_shown_ = false;

  std::vector<std::unique_ptr<DownloadUIModel>> all_models =
      bubble_controller_->GetAllItemsToDisplay();
  UpdateToolbarButtonState(all_models);
  int in_progress_count = InProgressDownloadCount(all_models);
  if (in_progress_count > 0 &&
      download::ShouldShowDownloadBubble(browser_->profile())) {
    display_->ShowDetails();
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
  int in_progress_count = InProgressDownloadCount(all_models);
  int paused_count = PausedDownloadCount(all_models);
  bool has_deep_scanning_download = HasDeepScanningDownload(all_models);
  base::Time last_complete_time =
      GetLastCompleteTime(bubble_controller_->GetOfflineItems());

  if (in_progress_count > 0) {
    icon_info_.icon_state = DownloadIconState::kProgress;
    icon_info_.is_active = paused_count >= in_progress_count ? false : true;
  } else {
    icon_info_.icon_state = DownloadIconState::kComplete;
    if (HasRecentCompleteDownload(kToolbarIconActiveTimeInterval,
                                  last_complete_time) &&
        HasUnactionedDownload(all_models)) {
      icon_info_.is_active = true;
      ScheduleToolbarInactive(kToolbarIconActiveTimeInterval);
    } else if (!display_->IsFullscreenWithParentViewHidden() &&
               download_completed_while_fullscreen_) {
      icon_info_.is_active = true;
      ScheduleToolbarInactive(kToolbarIconActiveTimeInterval);
      download_completed_while_fullscreen_ = false;
    } else {
      icon_info_.is_active = false;
    }
  }

  if (has_deep_scanning_download) {
    icon_info_.icon_state = DownloadIconState::kDeepScanning;
  }

  if (icon_info_.icon_state != DownloadIconState::kComplete ||
      HasRecentCompleteDownload(kToolbarIconVisibilityTimeInterval,
                                last_complete_time)) {
    ShowToolbarButton();
  }
  display_->UpdateDownloadIcon();
}

void DownloadDisplayController::UpdateDownloadIconToInactive() {
  icon_info_.is_active = false;
  display_->UpdateDownloadIcon();
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
