// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/gurl.h"

namespace policy {

namespace {
// Delay to wait to turn off privacy screen enforcement after confidential data
// becomes not visible. This is done to not blink the privacy screen in case of
// a quick switch from one confidential data to another.
const base::TimeDelta kPrivacyScreenOffDelay = base::Milliseconds(500);

// Helper method to check whether the restriction level is kBlock.
bool IsBlocked(RestrictionLevelAndUrl restriction_info) {
  return restriction_info.level == DlpRulesManager::Level::kBlock;
}

// Helper method to check whether the restriction level is kWarn.
bool IsWarn(RestrictionLevelAndUrl restriction_info) {
  return restriction_info.level == DlpRulesManager::Level::kWarn;
}

// Helper method to check if event should be reported.
// Does not apply to warning mode reporting.
bool IsReported(RestrictionLevelAndUrl restriction_info) {
  return restriction_info.level == DlpRulesManager::Level::kReport ||
         IsBlocked(restriction_info);
}

// If there is an on going video recording, interrupts it and notifies the user.
void InterruptVideoRecording() {
  if (ChromeCaptureModeDelegate::Get()->InterruptVideoRecordingIfAny())
    ShowDlpVideoCaptureStoppedNotification();
}

}  // namespace

static DlpContentManagerAsh* g_dlp_content_manager = nullptr;

// static
DlpContentManagerAsh* DlpContentManagerAsh::Get() {
  if (!g_dlp_content_manager) {
    g_dlp_content_manager = new DlpContentManagerAsh();
    g_dlp_content_manager->Init();
  }
  return g_dlp_content_manager;
}

void DlpContentManagerAsh::OnWindowOcclusionChanged(aura::Window* window) {
  // Stop video captures that now might include restricted content.
  CheckRunningVideoCapture();
}

void DlpContentManagerAsh::OnWindowDestroying(aura::Window* window) {
  window_observers_.erase(window);
  MaybeChangeOnScreenRestrictions();
}

DlpContentRestrictionSet DlpContentManagerAsh::GetConfidentialRestrictions(
    content::WebContents* web_contents) const {
  if (!base::Contains(confidential_web_contents_, web_contents))
    return DlpContentRestrictionSet();
  return confidential_web_contents_.at(web_contents);
}

DlpContentRestrictionSet DlpContentManagerAsh::GetOnScreenPresentRestrictions()
    const {
  return on_screen_restrictions_;
}

bool DlpContentManagerAsh::IsScreenshotApiRestricted(
    const ScreenshotArea& area) {
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  if (IsWarn(info.restriction_info))
    ReportWarningEvent(info.restriction_info.url,
                       DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kScreenshotBlockedUMA,
                      IsBlocked(info.restriction_info));
  // TODO(crbug.com/1252736): Properly handle WARN for screenshots API.
  return IsBlocked(info.restriction_info) || IsWarn(info.restriction_info);
}

void DlpContentManagerAsh::CheckScreenshotRestriction(
    const ScreenshotArea& area,
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kScreenshotBlockedUMA,
                      IsBlocked(info.restriction_info));
  CheckScreenCaptureRestriction(info, std::move(callback));
}

bool DlpContentManagerAsh::IsScreenCaptureRestricted(
    const content::DesktopMediaID& media_id) {
  const ConfidentialContentsInfo info =
      GetScreenShareConfidentialContentsInfo(media_id);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenShare);
  DlpBooleanHistogram(dlp::kScreenShareBlockedUMA,
                      IsBlocked(info.restriction_info));
  return IsBlocked(info.restriction_info);
}

void DlpContentManagerAsh::CheckScreenShareRestriction(
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    OnDlpRestrictionCheckedCallback callback) {
  ConfidentialContentsInfo info =
      GetScreenShareConfidentialContentsInfo(media_id);
  ProcessScreenShareRestriction(application_title, info, std::move(callback));
}

void DlpContentManagerAsh::OnVideoCaptureStarted(const ScreenshotArea& area) {
  DCHECK(!running_video_capture_info_.has_value());
  running_video_capture_info_.emplace(area);
}

void DlpContentManagerAsh::CheckStoppedVideoCapture(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  // If some confidential content was shown during the recording, but not
  // before, warn the user before saving the file.
  if (running_video_capture_info_.has_value() &&
      !running_video_capture_info_->confidential_contents.IsEmpty()) {
    const GURL& url =
        running_video_capture_info_->confidential_contents.GetContents()
            .begin()
            ->url;

    ReportWarningEvent(url, DlpRulesManager::Restriction::kScreenshot);

    auto reporting_callback = base::BindOnce(
        &MaybeReportWarningProceededEvent, url,
        DlpRulesManager::Restriction::kScreenshot, reporting_manager_);
    warn_notifier_->ShowDlpVideoCaptureWarningDialog(
        std::move(reporting_callback).Then(std::move(callback)),
        running_video_capture_info_->confidential_contents);
  } else {
    std::move(callback).Run(/*proceed=*/true);
  }

  running_video_capture_info_.reset();
}

void DlpContentManagerAsh::CheckCaptureModeInitRestriction(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  const ConfidentialContentsInfo info =
      GetConfidentialContentsOnScreen(DlpContentRestriction::kScreenshot);

  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kCaptureModeInitBlockedUMA,
                      IsBlocked(info.restriction_info));
  CheckScreenCaptureRestriction(info, std::move(callback));
}

void DlpContentManagerAsh::OnScreenCaptureStarted(
    const std::string& label,
    std::vector<content::DesktopMediaID> screen_capture_ids,
    const std::u16string& application_title,
    base::OnceClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const content::DesktopMediaID& id : screen_capture_ids) {
    ScreenShareInfo* screen_share_info =
        new ScreenShareInfo(label, id, application_title,
                            std::move(stop_callback), state_change_callback);
    DCHECK(std::find_if(
               running_screen_shares_.begin(), running_screen_shares_.end(),
               [=](base::WeakPtr<ScreenShareInfo> screen_share_info) -> bool {
                 return screen_share_info->GetLabel() == label &&
                        screen_share_info->GetMediaId() == id;
               }) == running_screen_shares_.end());

    running_screen_shares_.push_back(screen_share_info->GetWeakPtr());
  }
  CheckRunningScreenShares();
}

void DlpContentManagerAsh::OnScreenCaptureStopped(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  RemoveScreenShare(label, media_id);
}

void DlpContentManagerAsh::OnWindowRestrictionChanged(
    aura::Window* window,
    const DlpContentRestrictionSet& restrictions) {
  confidential_windows_[window] = restrictions;
  window_observers_[window] = std::make_unique<DlpWindowObserver>(window, this);
  MaybeChangeOnScreenRestrictions();
}

/* static */
void DlpContentManagerAsh::SetDlpContentManagerAshForTesting(
    DlpContentManagerAsh* dlp_content_manager) {
  if (g_dlp_content_manager)
    delete g_dlp_content_manager;
  g_dlp_content_manager = dlp_content_manager;
}

/* static */
void DlpContentManagerAsh::ResetDlpContentManagerAshForTesting() {
  g_dlp_content_manager = nullptr;
}

DlpContentManagerAsh::ScreenShareInfo::ScreenShareInfo(
    const std::string& label,
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    base::OnceClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback)
    : label_(label),
      media_id_(media_id),
      application_title_(application_title),
      stop_callback_(std::move(stop_callback)),
      state_change_callback_(state_change_callback) {}
DlpContentManagerAsh::ScreenShareInfo::~ScreenShareInfo() = default;

bool DlpContentManagerAsh::ScreenShareInfo::operator==(
    const DlpContentManagerAsh::ScreenShareInfo& other) const {
  return label_ == other.label_ && media_id_ == other.media_id_;
}

bool DlpContentManagerAsh::ScreenShareInfo::operator!=(
    const DlpContentManagerAsh::ScreenShareInfo& other) const {
  return !(*this == other);
}

const content::DesktopMediaID&
DlpContentManagerAsh::ScreenShareInfo::GetMediaId() const {
  return media_id_;
}

const std::string& DlpContentManagerAsh::ScreenShareInfo::GetLabel() const {
  return label_;
}

const std::u16string&
DlpContentManagerAsh::ScreenShareInfo::GetApplicationTitle() const {
  // TODO(crbug.com/1264793): Don't cache the application name, but compute it
  // here.
  return application_title_;
}

bool DlpContentManagerAsh::ScreenShareInfo::IsRunning() const {
  return state_ == State::kRunning;
}

void DlpContentManagerAsh::ScreenShareInfo::Pause() {
  DCHECK(state_ == State::kRunning);
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PAUSE);
  state_ = State::kPaused;
}

void DlpContentManagerAsh::ScreenShareInfo::Resume() {
  DCHECK(state_ == State::kPaused);
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PLAY);
  state_ = State::kRunning;
}

void DlpContentManagerAsh::ScreenShareInfo::Stop() {
  DCHECK(state_ != State::kStopped);
  if (stop_callback_) {
    std::move(stop_callback_).Run();
    state_ = State::kStopped;
  } else {
    NOTREACHED();
  }
}

void DlpContentManagerAsh::ScreenShareInfo::MaybeUpdateNotifications() {
  UpdatePausedNotification(/*show=*/state_ == State::kPaused);
  UpdateResumedNotification(/*show=*/state_ == State::kRunning);
}

void DlpContentManagerAsh::ScreenShareInfo::HideNotifications() {
  UpdatePausedNotification(/*show=*/false);
  UpdateResumedNotification(/*show=*/false);
}

base::WeakPtr<DlpContentManagerAsh::ScreenShareInfo>
DlpContentManagerAsh::ScreenShareInfo::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DlpContentManagerAsh::ScreenShareInfo::UpdatePausedNotification(
    bool show) {
  if ((notification_state_ == NotificationState::kShowingPausedNotification) ==
      show)
    return;
  if (show) {
    DCHECK(state_ == State::kPaused);
    ShowDlpScreenSharePausedNotification(label_, application_title_);
    notification_state_ = NotificationState::kShowingPausedNotification;
  } else {
    HideDlpScreenSharePausedNotification(label_);
    notification_state_ = NotificationState::kNotShowingNotification;
  }
}

void DlpContentManagerAsh::ScreenShareInfo::UpdateResumedNotification(
    bool show) {
  if ((notification_state_ == NotificationState::kShowingResumedNotification) ==
      show)
    return;
  if (show) {
    DCHECK(state_ == State::kRunning);
    ShowDlpScreenShareResumedNotification(label_, application_title_);
    notification_state_ = NotificationState::kShowingResumedNotification;
  } else {
    HideDlpScreenShareResumedNotification(label_);
    notification_state_ = NotificationState::kNotShowingNotification;
  }
}

DlpContentManagerAsh::VideoCaptureInfo::VideoCaptureInfo(
    const ScreenshotArea& area)
    : area(area) {}

DlpContentManagerAsh::DlpContentManagerAsh() = default;

DlpContentManagerAsh::~DlpContentManagerAsh() = default;

void DlpContentManagerAsh::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  DlpContentManager::OnConfidentialityChanged(web_contents, restriction_set);
  if (!restriction_set.IsEmpty()) {
    web_contents_window_observers_[web_contents] =
        std::make_unique<DlpWindowObserver>(web_contents->GetNativeView(),
                                            this);
    if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
      MaybeChangeOnScreenRestrictions();
    } else {
      CheckRunningScreenShares();
    }
  } else {
    CheckRunningScreenShares();
  }
}

void DlpContentManagerAsh::OnVisibilityChanged(
    content::WebContents* web_contents) {
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManagerAsh::RemoveFromConfidential(
    content::WebContents* web_contents) {
  DlpContentManager::RemoveFromConfidential(web_contents);
  web_contents_window_observers_.erase(web_contents);
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManagerAsh::MaybeChangeOnScreenRestrictions() {
  DlpContentRestrictionSet new_restriction_set;
  // Check each visible WebContents.
  for (const auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() == content::Visibility::VISIBLE) {
      new_restriction_set.UnionWith(entry.second);
    }
  }
  // Check each visible Lacros window.
  for (const auto& entry : confidential_windows_) {
    if (entry.first->IsVisible()) {
      new_restriction_set.UnionWith(entry.second);
    }
  }
  if (on_screen_restrictions_ != new_restriction_set) {
    DlpContentRestrictionSet added_restrictions =
        new_restriction_set.DifferenceWith(on_screen_restrictions_);
    DlpContentRestrictionSet removed_restrictions =
        on_screen_restrictions_.DifferenceWith(new_restriction_set);
    on_screen_restrictions_ = new_restriction_set;
    OnScreenRestrictionsChanged(added_restrictions, removed_restrictions);
  }
  CheckRunningVideoCapture();
  CheckRunningScreenShares();
}

void DlpContentManagerAsh::OnScreenRestrictionsChanged(
    const DlpContentRestrictionSet& added_restrictions,
    const DlpContentRestrictionSet& removed_restrictions) {
  DCHECK(!(added_restrictions.GetRestrictionLevel(
               DlpContentRestriction::kPrivacyScreen) ==
               DlpRulesManager::Level::kBlock &&
           removed_restrictions.GetRestrictionLevel(
               DlpContentRestriction::kPrivacyScreen) ==
               DlpRulesManager::Level::kBlock));
  ash::PrivacyScreenDlpHelper* privacy_screen_helper =
      ash::PrivacyScreenDlpHelper::Get();

  if (!privacy_screen_helper->IsSupported())
    return;

  const RestrictionLevelAndUrl added_restriction_info =
      added_restrictions.GetRestrictionLevelAndUrl(
          DlpContentRestriction::kPrivacyScreen);

  if (added_restriction_info.level == DlpRulesManager::Level::kBlock) {
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, true);
    privacy_screen_helper->SetEnforced(true);
  }

  MaybeReportEvent(added_restriction_info,
                   DlpRulesManager::Restriction::kPrivacyScreen);

  if (removed_restrictions.GetRestrictionLevel(
          DlpContentRestriction::kPrivacyScreen) ==
      DlpRulesManager::Level::kBlock) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &DlpContentManagerAsh::MaybeRemovePrivacyScreenEnforcement,
            base::Unretained(this)),
        kPrivacyScreenOffDelay);
  }
}

void DlpContentManagerAsh::MaybeRemovePrivacyScreenEnforcement() const {
  if (GetOnScreenPresentRestrictions().GetRestrictionLevel(
          DlpContentRestriction::kPrivacyScreen) !=
      DlpRulesManager::Level::kBlock) {
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, false);
    ash::PrivacyScreenDlpHelper::Get()->SetEnforced(false);
  }
}

DlpContentManagerAsh::ConfidentialContentsInfo
DlpContentManagerAsh::GetConfidentialContentsOnScreen(
    DlpContentRestriction restriction) const {
  DlpContentManagerAsh::ConfidentialContentsInfo info;
  info.restriction_info =
      GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(restriction);
  for (auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() != content::Visibility::VISIBLE)
      continue;
    if (entry.first->IsBeingDestroyed()) {
      // The contents can be in the process of being destroyed during this
      // check, although they have not yet been removed from
      // confidential_web_contents_. For example, this happens when we trigger
      // the check from OnWindowDestroying().
      continue;
    }
    if (entry.second.GetRestrictionLevel(restriction) ==
        info.restriction_info.level) {
      info.confidential_contents.Add(entry.first);
    }
  }
  return info;
}

DlpContentManagerAsh::ConfidentialContentsInfo
DlpContentManagerAsh::GetAreaConfidentialContentsInfo(
    const ScreenshotArea& area,
    DlpContentRestriction restriction) const {
  DlpContentManagerAsh::ConfidentialContentsInfo info;
  // Fullscreen - restricted if any confidential data is visible.
  if (area.type == ScreenshotType::kAllRootWindows) {
    return GetConfidentialContentsOnScreen(restriction);
  }

  // Window - restricted if the window contains confidential data.
  if (area.type == ScreenshotType::kWindow) {
    DCHECK(area.window);
    // Check whether the captured window contains any confidential WebContents.
    for (auto& entry : confidential_web_contents_) {
      aura::Window* web_contents_window = entry.first->GetNativeView();
      if (area.window->Contains(web_contents_window)) {
        if (entry.second.GetRestrictionLevel(restriction) ==
            info.restriction_info.level) {
          info.confidential_contents.Add(entry.first);
        } else if (entry.second.GetRestrictionLevel(restriction) >
                   info.restriction_info.level) {
          info.restriction_info =
              entry.second.GetRestrictionLevelAndUrl(restriction);
          info.confidential_contents.ClearAndAdd(entry.first);
        }
      }
    }
    // Check whether the captured window is a confidential Lacros window.
    auto window_entry = confidential_windows_.find(area.window);
    if (window_entry != confidential_windows_.end()) {
      if (window_entry->second.GetRestrictionLevel(restriction) ==
          info.restriction_info.level) {
        info.confidential_contents.Add(
            window_entry->first,
            window_entry->second.GetRestrictionUrl(restriction));
      } else if (window_entry->second.GetRestrictionLevel(restriction) >
                 info.restriction_info.level) {
        info.restriction_info =
            window_entry->second.GetRestrictionLevelAndUrl(restriction);
        info.confidential_contents.ClearAndAdd(
            window_entry->first,
            window_entry->second.GetRestrictionUrl(restriction));
      }
    }
    return info;
  }

  DCHECK_EQ(area.type, ScreenshotType::kPartialWindow);
  DCHECK(area.rect);
  DCHECK(area.window);
  // Partial - restricted if any visible confidential content intersects
  // with the area.

  // Intersect the captured area with all confidential WebContents.
  for (auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() != content::Visibility::VISIBLE ||
        entry.second.GetRestrictionLevel(restriction) ==
            DlpRulesManager::Level::kNotSet) {
      continue;
    }
    aura::Window* web_contents_window = entry.first->GetNativeView();
    if (web_contents_window->GetOcclusionState() ==
        aura::Window::OcclusionState::OCCLUDED) {
      continue;
    }
    aura::Window* root_window = web_contents_window->GetRootWindow();
    // If no root window, then the WebContent shouldn't be visible.
    if (!root_window)
      continue;
    // Not allowing if the area intersects with confidential WebContents,
    // but the intersection doesn't belong to occluded area.
    gfx::Rect intersection(*area.rect);
    aura::Window::ConvertRectToTarget(area.window, root_window, &intersection);
    intersection.Intersect(web_contents_window->GetBoundsInRootWindow());

    if (intersection.IsEmpty() ||
        web_contents_window->occluded_region_in_root().contains(
            gfx::RectToSkIRect(intersection)))
      continue;

    if (entry.second.GetRestrictionLevel(restriction) ==
        info.restriction_info.level) {
      info.confidential_contents.Add(entry.first);
    } else if (entry.second.GetRestrictionLevel(restriction) >
               info.restriction_info.level) {
      info.restriction_info =
          entry.second.GetRestrictionLevelAndUrl(restriction);
      info.confidential_contents.ClearAndAdd(entry.first);
    }
  }

  // Intersect the captured area with all confidential Lacros windows.
  for (auto& entry : confidential_windows_) {
    if (!entry.first->IsVisible() ||
        entry.first->GetOcclusionState() ==
            aura::Window::OcclusionState::OCCLUDED ||
        entry.second.GetRestrictionLevel(restriction) ==
            DlpRulesManager::Level::kNotSet) {
      continue;
    }
    aura::Window* root_window = entry.first->GetRootWindow();
    // If no root window, then the Window shouldn't be visible.
    if (!root_window)
      continue;
    // Not allowing if the area intersects with confidential Window,
    // but the intersection doesn't belong to occluded area.
    gfx::Rect intersection(*area.rect);
    aura::Window::ConvertRectToTarget(area.window, root_window, &intersection);
    intersection.Intersect(entry.first->GetBoundsInRootWindow());

    if (intersection.IsEmpty() ||
        entry.first->occluded_region_in_root().contains(
            gfx::RectToSkIRect(intersection)))
      continue;

    if (entry.second.GetRestrictionLevel(restriction) ==
        info.restriction_info.level) {
      info.confidential_contents.Add(
          entry.first, entry.second.GetRestrictionUrl(restriction));
    } else if (entry.second.GetRestrictionLevel(restriction) >
               info.restriction_info.level) {
      info.restriction_info =
          entry.second.GetRestrictionLevelAndUrl(restriction);
      info.confidential_contents.ClearAndAdd(
          entry.first, entry.second.GetRestrictionUrl(restriction));
    }
  }

  return info;
}

DlpContentManagerAsh::ConfidentialContentsInfo
DlpContentManagerAsh::GetScreenShareConfidentialContentsInfo(
    const content::DesktopMediaID& media_id) const {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    return GetConfidentialContentsOnScreen(DlpContentRestriction::kScreenShare);
  }
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    return GetScreenShareConfidentialContentsInfoForWebContents(
        media_id.web_contents_id);
  }
  DCHECK_EQ(media_id.type, content::DesktopMediaID::Type::TYPE_WINDOW);
  ConfidentialContentsInfo info;
  aura::Window* window = content::DesktopMediaID::GetNativeWindowById(media_id);
  if (window) {
    // Check whether the captured window contains any confidential WebContents.
    for (auto& entry : confidential_web_contents_) {
      aura::Window* web_contents_window = entry.first->GetNativeView();
      if (!window->Contains(web_contents_window))
        continue;
      if (entry.second.GetRestrictionLevel(
              DlpContentRestriction::kScreenShare) ==
          info.restriction_info.level) {
        info.confidential_contents.Add(entry.first);
      } else if (entry.second.GetRestrictionLevel(
                     DlpContentRestriction::kScreenShare) >
                 info.restriction_info.level) {
        info.restriction_info = entry.second.GetRestrictionLevelAndUrl(
            DlpContentRestriction::kScreenShare);
        info.confidential_contents.ClearAndAdd(entry.first);
      }
    }
    // Check whether the captured window is a confidential Lacros window.
    auto window_entry = confidential_windows_.find(window);
    if (window_entry != confidential_windows_.end()) {
      if (window_entry->second.GetRestrictionLevel(
              DlpContentRestriction::kScreenShare) ==
          info.restriction_info.level) {
        info.confidential_contents.Add(
            window_entry->first, window_entry->second.GetRestrictionUrl(
                                     DlpContentRestriction::kScreenShare));
      } else if (window_entry->second.GetRestrictionLevel(
                     DlpContentRestriction::kScreenShare) >
                 info.restriction_info.level) {
        info.restriction_info = window_entry->second.GetRestrictionLevelAndUrl(
            DlpContentRestriction::kScreenShare);
        info.confidential_contents.ClearAndAdd(
            window_entry->first, window_entry->second.GetRestrictionUrl(
                                     DlpContentRestriction::kScreenShare));
      }
    }
  }
  return info;
}

void DlpContentManagerAsh::CheckRunningVideoCapture() {
  if (!running_video_capture_info_.has_value())
    return;
  ConfidentialContentsInfo info = GetAreaConfidentialContentsInfo(
      running_video_capture_info_->area, DlpContentRestriction::kScreenshot);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  if (IsBlocked(info.restriction_info)) {
    DlpBooleanHistogram(dlp::kVideoCaptureInterruptedUMA, true);
    InterruptVideoRecording();
    running_video_capture_info_.reset();
    return;
  }
  if (IsWarn(info.restriction_info)) {
    // Remember any confidential content captured during the recording, so we
    // can inform the user about it after the recording is finished. We drop
    // those that the user was already warned about and has allowed the screen
    // capture to proceed.
    RemoveAllowedContents(info.confidential_contents,
                          DlpRulesManager::Restriction::kScreenshot);
    running_video_capture_info_->confidential_contents.UnionWith(
        info.confidential_contents);
    return;
  }
}

void DlpContentManagerAsh::RemoveScreenShare(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  base::EraseIf(running_screen_shares_,
                [=](base::WeakPtr<ScreenShareInfo> screen_share_info) -> bool {
                  const bool erased =
                      screen_share_info->GetLabel() == label &&
                      screen_share_info->GetMediaId() == media_id;
                  // Hide notifications if necessary.
                  screen_share_info->HideNotifications();
                  return erased;
                });
}

void DlpContentManagerAsh::CheckRunningScreenShares() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (base::WeakPtr<ScreenShareInfo> screen_share : running_screen_shares_) {
    ConfidentialContentsInfo info =
        GetScreenShareConfidentialContentsInfo(screen_share->GetMediaId());
    if (IsBlocked(info.restriction_info)) {
      if (screen_share->IsRunning()) {
        screen_share->Pause();
        MaybeReportEvent(info.restriction_info,
                         DlpRulesManager::Restriction::kScreenShare);
        DlpBooleanHistogram(dlp::kScreenSharePausedOrResumedUMA, true);
        screen_share->MaybeUpdateNotifications();
      }
      continue;
    }
    if (is_screen_share_warning_mode_enabled_ &&
        IsWarn(info.restriction_info)) {
      // Check which of the contents were already allowed and don't warn for
      // those.
      RemoveAllowedContents(info.confidential_contents,
                            DlpRulesManager::Restriction::kScreenShare);
      if (info.confidential_contents.IsEmpty()) {
        // The user already allowed all the visible content.
        if (!screen_share->IsRunning()) {
          screen_share->Resume();
          screen_share->MaybeUpdateNotifications();
        }
        continue;
      }
      if (screen_share->IsRunning()) {
        screen_share->Pause();
        screen_share->HideNotifications();
      }
      // base::Unretained(this) is safe here because DlpContentManagerAsh is
      // initialized as a singleton that's always available in the system.
      warn_notifier_->ShowDlpScreenShareWarningDialog(
          base::BindOnce(&DlpContentManagerAsh::OnDlpScreenShareWarnDialogReply,
                         base::Unretained(this), info.confidential_contents,
                         screen_share),
          info.confidential_contents, screen_share->GetApplicationTitle());
      continue;
    }
    // No restrictions apply, only resume if necessary.
    if (!screen_share->IsRunning()) {
      screen_share->Resume();
      DlpBooleanHistogram(dlp::kScreenSharePausedOrResumedUMA, false);
      screen_share->MaybeUpdateNotifications();
    }
  }
}

void DlpContentManagerAsh::SetReportingManagerForTesting(
    DlpReportingManager* reporting_manager) {
  reporting_manager_ = reporting_manager;
}

void DlpContentManagerAsh::SetWarnNotifierForTesting(
    std::unique_ptr<DlpWarnNotifier> warn_notifier) {
  DCHECK(warn_notifier);
  warn_notifier_ = std::move(warn_notifier);
}

void DlpContentManagerAsh::ResetWarnNotifierForTesting() {
  warn_notifier_ = std::make_unique<DlpWarnNotifier>();
}

// static
base::TimeDelta DlpContentManagerAsh::GetPrivacyScreenOffDelayForTesting() {
  return kPrivacyScreenOffDelay;
}

void DlpContentManagerAsh::CheckScreenCaptureRestriction(
    ConfidentialContentsInfo info,
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  if (IsBlocked(info.restriction_info)) {
    ShowDlpScreenCaptureDisabledNotification();
    std::move(callback).Run(false);
    return;
  }
  if (IsWarn(info.restriction_info)) {
    // Check which of the contents were already allowed and don't warn for
    // those.
    RemoveAllowedContents(info.confidential_contents,
                          DlpRulesManager::Restriction::kScreenshot);
    if (info.confidential_contents.IsEmpty()) {
      // The user already allowed all the visible content.
      ReportWarningProceededEvent(info.restriction_info.url,
                                  DlpRulesManager::Restriction::kScreenshot,
                                  reporting_manager_);
      std::move(callback).Run(true);
      return;
    }

    ReportWarningEvent(info.restriction_info.url,
                       DlpRulesManager::Restriction::kScreenshot);

    auto reporting_callback = base::BindOnce(
        &MaybeReportWarningProceededEvent, info.restriction_info.url,
        DlpRulesManager::Restriction::kScreenshot, reporting_manager_);
    // base::Unretained(this) is safe here because DlpContentManagerAsh is
    // initialized as a singleton that's always available in the system.
    warn_notifier_->ShowDlpScreenCaptureWarningDialog(
        base::BindOnce(&DlpContentManagerAsh::OnDlpWarnDialogReply,
                       base::Unretained(this), info.confidential_contents,
                       DlpRulesManager::Restriction::kScreenshot,
                       std::move(reporting_callback).Then(std::move(callback))),
        info.confidential_contents);
    return;
  }
  // No restrictions apply.
  std::move(callback).Run(true);
}

void DlpContentManagerAsh::OnDlpScreenShareWarnDialogReply(
    const DlpConfidentialContents& confidential_contents,
    base::WeakPtr<ScreenShareInfo> screen_share,
    bool should_proceed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!screen_share)
    // The screen share was stopped before the dialog was addressed, so no need
    // to do anything.
    return;

  if (should_proceed) {
    screen_share->Resume();
    for (const auto& content : confidential_contents.GetContents()) {
      user_allowed_contents_cache_.Cache(
          content, DlpRulesManager::Restriction::kScreenShare);
    }
  } else {
    screen_share->Stop();
    RemoveScreenShare(screen_share->GetLabel(), screen_share->GetMediaId());
  }
  screen_share->MaybeUpdateNotifications();
}

// ScopedDlpContentManagerAshForTesting
ScopedDlpContentManagerAshForTesting::ScopedDlpContentManagerAshForTesting(
    DlpContentManagerAsh* test_dlp_content_manager) {
  DlpContentManagerAsh::SetDlpContentManagerAshForTesting(
      test_dlp_content_manager);
}

ScopedDlpContentManagerAshForTesting::~ScopedDlpContentManagerAshForTesting() {
  DlpContentManagerAsh::ResetDlpContentManagerAshForTesting();
}

}  // namespace policy
