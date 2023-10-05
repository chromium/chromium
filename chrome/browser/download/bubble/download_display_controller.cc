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
#include "chrome/browser/download/bubble/download_bubble_display_info.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
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
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

using DownloadIconActive = DownloadDisplay::IconActive;
using DownloadIconState = DownloadDisplay::IconState;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;

// The amount of time for the toolbar icon to be visible after a download is
// completed.
constexpr base::TimeDelta kToolbarIconVisibilityTimeInterval =
    base::Minutes(60);

// The amount of time for the toolbar icon to stay active after a download is
// completed. If the download completed while full screen, the timer is started
// after user comes out of the full screen.
constexpr base::TimeDelta kToolbarIconActiveTimeInterval = base::Minutes(1);

// Whether there are no more in-progress downloads that are not paused, i.e.,
// whether all actively downloading items are done.
bool IsAllDone(const DownloadBubbleDisplayInfo& info) {
  return info.in_progress_count == info.paused_count;
}

}  // namespace

DownloadDisplayController::DownloadDisplayController(
    DownloadDisplay* display,
    Browser* browser,
    DownloadBubbleUIController* bubble_controller)
    : display_(display),
      browser_(browser),
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

  UpdateButtonStateFromUpdateService();
  if (display_->ShouldShowExclusiveAccessBubble()) {
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
    DownloadDisplay::IconUpdateInfo updates;
    updates.show_animation = show_animation;
    display_->UpdateDownloadIcon(updates);
  }
}

void DownloadDisplayController::OnUpdatedItem(bool is_done,
                                              bool may_show_details) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  const DownloadBubbleDisplayInfo& info = UpdateButtonStateFromUpdateService();
  bool will_show_details = may_show_details && is_done && IsAllDone(info);
  if (is_done) {
    ScheduleToolbarDisappearance(kToolbarIconVisibilityTimeInterval);
  }
  if (will_show_details && display_->IsFullscreenWithParentViewHidden()) {
    // If we would show the details, but the user is in fullscreen (and is
    // capable of exiting), we should instead show the details once the user
    // exits fullscreen.
    should_show_details_on_exit_fullscreen_ =
        display_->ShouldShowExclusiveAccessBubble();
    // Show the details if we are in immersive fullscreen.
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    will_show_details = browser_view && browser_view->IsImmersiveModeEnabled();
  }

  // At this point, we are possibly in fullscreen. If we're in immersive
  // fullscreen on macOS, it's OK to show the details bubble because the
  // toolbar is either visible or it can be made visible. However, if we're
  // in content/HTML fullscreen, the toolbar is not visible and we should not
  // show the bubble. So, check our fullscreen state here and avoid showing
  // the bubble if we're in content fullscreen.
#if BUILDFLAG(IS_MAC)
  will_show_details =
      will_show_details && !fullscreen_utils::IsInContentFullscreen(browser_);
#endif

  if (will_show_details) {
    display_->ShowDetails();
  }
}

void DownloadDisplayController::OnRemovedItem(const ContentId& id) {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }
  UpdateButtonStateFromUpdateService();
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
  if (display_->GetIconState() == DownloadIconState::kComplete) {
    DownloadDisplay::IconUpdateInfo updates;
    updates.new_active = DownloadIconActive::kInactive;
    display_->UpdateDownloadIcon(updates);
  }
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

bool DownloadDisplayController::OpenMostSpecificDialog(
    const offline_items_collection::ContentId& content_id) {
  // This method is currently used only for Lacros download notifications.
  // This is called when a notification is clicked, and shows the download
  // bubble in the Lacros browser window. In Lacros browser fullscreen (always
  // immersive), the immersive fullscreen toolbar is shown (handled by display_)
  // so no special case is needed here. In Lacros tab fullscreen (not
  // immersive), the notification is not visible and can't be clicked, so we
  // don't need to check display_->IsFullscreenWithParentViewHidden() here.
  return display_->OpenMostSpecificDialog(content_id);
}

void DownloadDisplayController::ListenToFullScreenChanges() {
  observation_.Observe(
      browser_->exclusive_access_manager()->fullscreen_controller());
}

void DownloadDisplayController::OnFullscreenStateChanged() {
  if ((!fullscreen_notification_shown_ &&
       !should_show_details_on_exit_fullscreen_) ||
      display_->IsFullscreenWithParentViewHidden()) {
    return;
  }
  fullscreen_notification_shown_ = false;

  UpdateButtonStateFromUpdateService();
  if (download::ShouldShowDownloadBubble(browser_->profile()) &&
      should_show_details_on_exit_fullscreen_) {
    display_->ShowDetails();
    should_show_details_on_exit_fullscreen_ = false;
  }
}

void DownloadDisplayController::OnResume() {
  UpdateButtonStateFromUpdateService();
}

void DownloadDisplayController::OpenSecuritySubpage(
    const offline_items_collection::ContentId& id) {
  display_->OpenSecuritySubpage(id);
}

void DownloadDisplayController::UpdateToolbarButtonState(
    const DownloadBubbleDisplayInfo& info,
    const DownloadDisplay::ProgressInfo& progress_info) {
  if (info.all_models_size == 0) {
    HideToolbarButton();
    return;
  }
  base::Time last_complete_time = GetLastCompleteTime(info.last_completed_time);

  DownloadDisplay::IconUpdateInfo updates;

  if (info.in_progress_count > 0) {
    updates.new_state = DownloadIconState::kProgress;
    updates.new_active = info.paused_count < info.in_progress_count
                             ? DownloadIconActive::kActive
                             : DownloadIconActive::kInactive;
  } else {
    updates.new_state = DownloadIconState::kComplete;
    bool complete_unactioned =
        HasRecentCompleteDownload(kToolbarIconActiveTimeInterval,
                                  last_complete_time) &&
        info.has_unactioned;
    bool exited_fullscreen_owed_details =
        !display_->IsFullscreenWithParentViewHidden() &&
        should_show_details_on_exit_fullscreen_;
    if (complete_unactioned || exited_fullscreen_owed_details) {
      updates.new_active = DownloadIconActive::kActive;
      ScheduleToolbarInactive(kToolbarIconActiveTimeInterval);
    } else {
      updates.new_active = DownloadIconActive::kInactive;
    }
  }

  if (info.has_deep_scanning) {
    updates.new_state = DownloadIconState::kDeepScanning;
  }

  if (updates.new_state != DownloadIconState::kComplete ||
      HasRecentCompleteDownload(kToolbarIconVisibilityTimeInterval,
                                last_complete_time)) {
    ShowToolbarButton();
  }

  updates.new_progress = progress_info;

  display_->UpdateDownloadIcon(updates);
}

void DownloadDisplayController::UpdateDownloadIconToInactive() {
  DownloadDisplay::IconUpdateInfo updates;
  updates.new_active = DownloadIconActive::kInactive;
  display_->UpdateDownloadIcon(updates);
}

const DownloadBubbleDisplayInfo&
DownloadDisplayController::UpdateButtonStateFromUpdateService() {
  const DownloadBubbleDisplayInfo& info =
      bubble_controller_->update_service()->GetDisplayInfo(
          GetWebAppIdForBrowser(browser_));
  DownloadDisplay::ProgressInfo progress_info =
      bubble_controller_->update_service()->GetProgressInfo(
          GetWebAppIdForBrowser(browser_));

  UpdateToolbarButtonState(info, progress_info);

  return info;
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
    base::Time last_completed_time_from_current_models) const {
  base::Time last_time = DownloadPrefs::FromBrowserContext(browser_->profile())
                             ->GetLastCompleteTime();
  return std::max(last_time, last_completed_time_from_current_models);
}

void DownloadDisplayController::MaybeShowButtonWhenCreated() {
  if (!download::ShouldShowDownloadBubble(browser_->profile())) {
    return;
  }

  const DownloadBubbleDisplayInfo& info = UpdateButtonStateFromUpdateService();
  if (display_->IsShowing()) {
    ScheduleToolbarDisappearance(
        kToolbarIconVisibilityTimeInterval -
        (base::Time::Now() - GetLastCompleteTime(info.last_completed_time)));
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
