// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
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

bool IsAnyChildVisible(aura::Window* window) {
  if (window->GetOcclusionState() == aura::Window::OcclusionState::VISIBLE)
    return true;
  for (aura::Window* child : window->children()) {
    if (IsAnyChildVisible(child))
      return true;
  }
  return false;
}

// Retrieves a child representing ExoSurface.
aura::Window* FindSurface(aura::Window* window) {
  if (!window)
    return nullptr;
  if (exo::Surface::AsSurface(window))
    return window;
  for (aura::Window* child : window->children()) {
    auto* found_window = FindSurface(child);
    if (found_window)
      return found_window;
  }
  return nullptr;
}

}  // namespace

static DlpContentManagerAsh* g_dlp_content_manager = nullptr;

// static
DlpContentManagerAsh* DlpContentManagerAsh::Get() {
  if (g_dlp_content_manager)
    return g_dlp_content_manager;
  return static_cast<DlpContentManagerAsh*>(DlpContentObserver::Get());
}

void DlpContentManagerAsh::OnWindowOcclusionChanged(aura::Window* window) {
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManagerAsh::OnWindowDestroying(aura::Window* window) {
  surface_observers_.erase(window);
  window_observers_.erase(window);
  confidential_windows_.erase(window);
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManagerAsh::OnWindowTitleChanged(aura::Window* window) {
  CheckRunningVideoCapture();
  CheckRunningScreenShares();
}

DlpContentRestrictionSet DlpContentManagerAsh::GetOnScreenPresentRestrictions()
    const {
  return on_screen_restrictions_;
}

void DlpContentManagerAsh::CheckScreenshotRestriction(
    const ScreenshotArea& area,
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  if (IsBlocked(info.restriction_info)) {
    MaybeReportEvent(info.restriction_info,
                     DlpRulesManager::Restriction::kScreenshot);
  }
  data_controls::DlpBooleanHistogram(data_controls::dlp::kScreenshotBlockedUMA,
                                     IsBlocked(info.restriction_info));
  data_controls::DlpBooleanHistogram(data_controls::dlp::kScreenshotWarnedUMA,
                                     IsWarn(info.restriction_info));
  CheckScreenCaptureRestriction(info, std::move(callback));
}

void DlpContentManagerAsh::CheckScreenShareRestriction(
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    WarningCallback callback) {
  ConfidentialContentsInfo info = GetScreenShareConfidentialContentsInfo(
      media_id, GetWebContentsFromMediaId(media_id));
  ProcessScreenShareRestriction(application_title, info, std::move(callback));
}

void DlpContentManagerAsh::OnVideoCaptureStarted(const ScreenshotArea& area) {
  DCHECK(!running_video_capture_info_.has_value());
  running_video_capture_info_.emplace(area);
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  // Taking video capture of confidential content with block level restriction
  // should not proceed to this function. Taking video capture should be blocked
  // earlier.
  DCHECK(!IsBlocked(info.restriction_info));
  if (IsReported(info.restriction_info)) {
    // Don't report for the report mode before starting a video capture to avoid
    // reporting multiple times.
    DCHECK(
        running_video_capture_info_->reported_confidential_contents.IsEmpty());
    // TODO(1306306): Consider reporting all visible confidential urls for
    //  onscreen restrictions.
    MaybeReportEvent(info.restriction_info,
                     DlpRulesManager::Restriction::kScreenshot);
    running_video_capture_info_->reported_confidential_contents.InsertOrUpdate(
        info.confidential_contents);
  }
  if (IsWarn(info.restriction_info) && reporting_manager_) {
    ReportWarningProceededEvent(info.restriction_info.url,
                                DlpRulesManager::Restriction::kScreenshot,
                                reporting_manager_);
  }
}

void DlpContentManagerAsh::CheckStoppedVideoCapture(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  if (!running_video_capture_info_.has_value()) {
    std::move(callback).Run(/*proceed=*/true);
    return;
  }
  // If some confidential content was shown during the recording, but not
  // before, warn the user before saving the file.
  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kScreenshotWarnedUMA,
      running_video_capture_info_->had_warning_restriction);
  if (!running_video_capture_info_->confidential_contents.IsEmpty()) {
    const GURL& url =
        running_video_capture_info_->confidential_contents.GetContents()
            .begin()
            ->url;

    ReportWarningEvent(url, DlpRulesManager::Restriction::kScreenshot);

    auto reporting_callback = base::BindOnce(
        &MaybeReportWarningProceededEvent, url,
        DlpRulesManager::Restriction::kScreenshot, reporting_manager_);
    // base::Unretained(this) is safe here because DlpContentManagerAsh is
    // initialized as a singleton that's always available in the system.
    warn_notifier_->ShowDlpVideoCaptureWarningDialog(
        base::BindOnce(&DlpContentManagerAsh::OnDlpWarnDialogReply,
                       base::Unretained(this),
                       running_video_capture_info_->confidential_contents,
                       DlpRulesManager::Restriction::kScreenshot,
                       std::move(reporting_callback).Then(std::move(callback))),
        running_video_capture_info_->confidential_contents);
  } else {
    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kScreenshotWarnSilentProceededUMA, true);
    std::move(callback).Run(/*proceed=*/true);
  }

  running_video_capture_info_.reset();
}

void DlpContentManagerAsh::OnImageCapture(const ScreenshotArea& area) {
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  // Taking screenshots of confidential content with block level restriction
  // should not proceed to this function. Taking screenshot should be blocked
  // earlier.
  DCHECK(!IsBlocked(info.restriction_info));
  if (IsReported(info.restriction_info)) {
    MaybeReportEvent(info.restriction_info,
                     DlpRulesManager::Restriction::kScreenshot);
  }
  if (IsWarn(info.restriction_info) && reporting_manager_) {
    ReportWarningProceededEvent(info.restriction_info.url,
                                DlpRulesManager::Restriction::kScreenshot,
                                reporting_manager_);
  }
}

void DlpContentManagerAsh::CheckCaptureModeInitRestriction(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  const ConfidentialContentsInfo info =
      GetConfidentialContentsOnScreen(DlpContentRestriction::kScreenshot);

  if (IsBlocked(info.restriction_info)) {
    MaybeReportEvent(info.restriction_info,
                     DlpRulesManager::Restriction::kScreenshot);
  }

  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kCaptureModeInitBlockedUMA,
      IsBlocked(info.restriction_info));
  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kCaptureModeInitWarnedUMA,
      IsWarn(info.restriction_info));
  CheckScreenCaptureRestriction(info, std::move(callback));
}

void DlpContentManagerAsh::OnScreenShareStarted(
    const std::string& label,
    std::vector<content::DesktopMediaID> screen_share_ids,
    const std::u16string& application_title,
    base::RepeatingClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const content::DesktopMediaID& id : screen_share_ids) {
    AddOrUpdateScreenShare(label, id, application_title, stop_callback,
                           state_change_callback, source_callback);
  }
  CheckRunningScreenShares();
}

void DlpContentManagerAsh::OnScreenShareStopped(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  RemoveScreenShare(label, media_id);
}

// TODO(b/308912502): migrate from mojo crosapi to wayland IPC. The current
// implementation depends on the client keeping the connection for its whole
// lifetime.
void DlpContentManagerAsh::OnWindowRestrictionChanged(
    mojo::ReceiverId receiver_id,
    const std::string& window_id,
    const DlpContentRestrictionSet& restrictions) {
  aura::Window* window = crosapi::GetShellSurfaceWindow(window_id);
  if (window) {
    pending_restrictions_.erase(window_id);
    confidential_windows_[window] = restrictions;
    window_observers_[window] =
        std::make_unique<DlpWindowObserver>(window, this);
    aura::Window* surface = FindSurface(window);
    if (surface) {
      surface_observers_[window] =
          std::make_unique<DlpWindowObserver>(surface, this);
    }
    MaybeChangeOnScreenRestrictions();
  } else {
    if (restrictions.IsEmpty()) {
      pending_restrictions_.erase(window_id);
      auto iter = pending_restrictions_owner_.find(receiver_id);
      if (iter != pending_restrictions_owner_.end()) {
        iter->second.erase(window_id);
        if (iter->second.empty()) {
          pending_restrictions_owner_.erase(iter);
        }
      }
    } else {
      pending_restrictions_.insert({window_id, {receiver_id, restrictions}});
      auto [iter, is_new] =
          pending_restrictions_owner_.try_emplace(receiver_id);
      iter->second.insert(window_id);
    }
  }
}

void DlpContentManagerAsh::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active) {
    return;
  }
  const std::string* application_id = exo::GetShellApplicationId(gained_active);
  if (!application_id) {
    return;
  }
  auto iter = pending_restrictions_.find(*application_id);
  if (iter != pending_restrictions_.end()) {
    auto [receiver_id, restrictions] =
        pending_restrictions_.extract(iter).mapped();
    OnWindowRestrictionChanged(receiver_id, *application_id, restrictions);
  }
}

void DlpContentManagerAsh::CleanPendingRestrictions(
    mojo::ReceiverId receiver_id) {
  auto iter = pending_restrictions_owner_.find(receiver_id);
  if (iter == pending_restrictions_owner_.end()) {
    return;
  }
  for (const auto& window_id : iter->second) {
    pending_restrictions_.erase(window_id);
  }
  pending_restrictions_owner_.erase(iter);
}

DlpContentManagerAsh::VideoCaptureInfo::VideoCaptureInfo(
    const ScreenshotArea& area)
    : area(area) {}

DlpContentManagerAsh::DlpContentManagerAsh() {
  if (ash::Shell::HasInstance() && ash::Shell::Get()->activation_client()) {
    window_activation_observation_.Observe(
        ash::Shell::Get()->activation_client());
  }
}

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
    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kPrivacyScreenEnforcedUMA, true);
    privacy_screen_helper->SetEnforced(true);
  }

  MaybeReportEvent(added_restriction_info,
                   DlpRulesManager::Restriction::kPrivacyScreen);

  if (removed_restrictions.GetRestrictionLevel(
          DlpContentRestriction::kPrivacyScreen) ==
      DlpRulesManager::Level::kBlock) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kPrivacyScreenEnforcedUMA, false);
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
      // the check from OnWebContentsDestroyed().
      continue;
    }
    if (entry.second.GetRestrictionLevel(restriction) ==
        info.restriction_info.level) {
      info.confidential_contents.Add(entry.first);
    }
  }
  for (auto& entry : confidential_windows_) {
    if (!entry.first->IsVisible() || !IsAnyChildVisible(entry.first))
      continue;
    if (entry.first->is_destroying()) {
      // The window can be in the process of being destroyed during this
      // check, although it has not yet been removed from
      // confidential_windows. For example, this happens when we trigger
      // the check from OnWindowDestroying().
      continue;
    }
    if (entry.second.GetRestrictionLevel(restriction) ==
        info.restriction_info.level) {
      info.confidential_contents.Add(
          entry.first, entry.second.GetRestrictionUrl(restriction));
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

DlpContentManager::ConfidentialContentsInfo
DlpContentManagerAsh::GetScreenShareConfidentialContentsInfo(
    const content::DesktopMediaID& media_id,
    content::WebContents* web_contents) const {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    return GetConfidentialContentsOnScreen(DlpContentRestriction::kScreenShare);
  }
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    return GetScreenShareConfidentialContentsInfoForWebContents(web_contents);
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
    // Check whether the captured window has a confidential Lacros window.
    for (auto& entry : confidential_windows_) {
      if (!window->Contains(entry.first))
        continue;
      if (entry.second.GetRestrictionLevel(
              DlpContentRestriction::kScreenShare) ==
          info.restriction_info.level) {
        info.confidential_contents.Add(
            entry.first, entry.second.GetRestrictionUrl(
                             DlpContentRestriction::kScreenShare));
      } else if (entry.second.GetRestrictionLevel(
                     DlpContentRestriction::kScreenShare) >
                 info.restriction_info.level) {
        info.restriction_info = entry.second.GetRestrictionLevelAndUrl(
            DlpContentRestriction::kScreenShare);
        info.confidential_contents.ClearAndAdd(
            entry.first, entry.second.GetRestrictionUrl(
                             DlpContentRestriction::kScreenShare));
      }
    }
  }
  return info;
}

void DlpContentManagerAsh::TabLocationMaybeChanged(
    content::WebContents* web_contents) {
  CheckRunningVideoCapture();
  CheckRunningScreenShares();
}

void DlpContentManagerAsh::CheckRunningVideoCapture() {
  if (!running_video_capture_info_.has_value())
    return;
  ConfidentialContentsInfo info = GetAreaConfidentialContentsInfo(
      running_video_capture_info_->area, DlpContentRestriction::kScreenshot);

  if (IsReported(info.restriction_info) &&
      !std::includes(running_video_capture_info_->reported_confidential_contents
                         .GetContents()
                         .begin(),
                     running_video_capture_info_->reported_confidential_contents
                         .GetContents()
                         .end(),
                     info.confidential_contents.GetContents().begin(),
                     info.confidential_contents.GetContents().end())) {
    // TODO(1306306): Consider reporting all visible confidential urls for
    //  onscreen restrictions.
    MaybeReportEvent(info.restriction_info,
                     DlpRulesManager::Restriction::kScreenshot);
    running_video_capture_info_->reported_confidential_contents.InsertOrUpdate(
        info.confidential_contents);
  }

  if (IsBlocked(info.restriction_info)) {
    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kVideoCaptureInterruptedUMA, true);
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
    running_video_capture_info_->confidential_contents.InsertOrUpdate(
        info.confidential_contents);
    running_video_capture_info_->had_warning_restriction = true;
    return;
  }
}

// static
base::TimeDelta DlpContentManagerAsh::GetPrivacyScreenOffDelayForTesting() {
  return kPrivacyScreenOffDelay;
}

void DlpContentManagerAsh::CheckScreenCaptureRestriction(
    ConfidentialContentsInfo info,
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  if (IsBlocked(info.restriction_info)) {
    // TODO(296534642): Remove once proper tooling is added.
    LOG(WARNING) << "Screenshot blocked due to following URL(s) visible:";
    for (const auto& content : info.confidential_contents.GetContents()) {
      LOG(WARNING) << content.url;
    }
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
      data_controls::DlpBooleanHistogram(
          data_controls::dlp::kScreenshotWarnSilentProceededUMA, true);
      std::move(callback).Run(true);
      return;
    }

    ReportWarningEvent(info.restriction_info.url,
                       DlpRulesManager::Restriction::kScreenshot);

    // base::Unretained(this) is safe here because DlpContentManagerAsh is
    // initialized as a singleton that's always available in the system.
    warn_notifier_->ShowDlpScreenCaptureWarningDialog(
        base::BindOnce(&DlpContentManagerAsh::OnDlpWarnDialogReply,
                       base::Unretained(this), info.confidential_contents,
                       DlpRulesManager::Restriction::kScreenshot,
                       std::move(callback)),
        info.confidential_contents);
    return;
  }
  // No restrictions apply.
  std::move(callback).Run(true);
}

}  // namespace policy
