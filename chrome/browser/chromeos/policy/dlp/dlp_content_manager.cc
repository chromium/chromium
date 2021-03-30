// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/stl_util.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace policy {

namespace {
// Delay to wait to turn off privacy screen enforcement after confidential data
// becomes not visible. This is done to not blink the privacy screen in case of
// a quick switch from one confidential data to another.
const base::TimeDelta kPrivacyScreenOffDelay =
    base::TimeDelta::FromMilliseconds(500);
}  // namespace

static DlpContentManager* g_dlp_content_manager = nullptr;

// static
DlpContentManager* DlpContentManager::Get() {
  if (!g_dlp_content_manager)
    g_dlp_content_manager = new DlpContentManager();
  return g_dlp_content_manager;
}

void DlpContentManager::OnWindowOcclusionChanged(aura::Window* window) {
  // Stop video captures that now might include restricted content.
  CheckRunningVideoCapture();
}

DlpContentRestrictionSet DlpContentManager::GetConfidentialRestrictions(
    content::WebContents* web_contents) const {
  if (!base::Contains(confidential_web_contents_, web_contents))
    return DlpContentRestrictionSet();
  return confidential_web_contents_.at(web_contents);
}

DlpContentRestrictionSet DlpContentManager::GetOnScreenPresentRestrictions()
    const {
  return on_screen_restrictions_;
}

bool DlpContentManager::IsScreenshotRestricted(
    const ScreenshotArea& area) const {
  const bool restricted =
      IsAreaRestricted(area, DlpContentRestriction::kScreenshot);
  if (restricted)
    SYSLOG(INFO) << "DLP blocked taking a screenshot";
  DlpBooleanHistogram(dlp::kScreenshotBlockedUMA, restricted);
  return restricted;
}

bool DlpContentManager::IsVideoCaptureRestricted(
    const ScreenshotArea& area) const {
  const bool restricted =
      IsAreaRestricted(area, DlpContentRestriction::kVideoCapture);
  if (restricted)
    SYSLOG(INFO) << "DLP blocked taking a video capture";
  DlpBooleanHistogram(dlp::kVideoCaptureBlockedUMA, restricted);
  return restricted;
}

bool DlpContentManager::IsPrintingRestricted(
    content::WebContents* web_contents) const {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedder
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  web_contents =
      guest_view ? guest_view->embedder_web_contents() : web_contents;

  const bool restricted = GetConfidentialRestrictions(web_contents)
                              .HasRestriction(DlpContentRestriction::kPrint);
  if (restricted)
    SYSLOG(INFO) << "DLP blocked printing";
  DlpBooleanHistogram(dlp::kPrintingBlockedUMA, restricted);
  return restricted;
}

bool DlpContentManager::IsScreenCaptureRestricted(
    const content::DesktopMediaID& media_id) const {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    const bool restricted = GetOnScreenPresentRestrictions().HasRestriction(
        DlpContentRestriction::kScreenShare);
    if (restricted)
      SYSLOG(INFO) << "DLP blocked screen sharing";
    DlpBooleanHistogram(dlp::kScreenShareBlockedUMA, restricted);
    return restricted;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              media_id.web_contents_id.render_process_id,
              media_id.web_contents_id.main_render_frame_id));

  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    const bool restricted =
        GetConfidentialRestrictions(web_contents)
            .HasRestriction(DlpContentRestriction::kScreenShare);
    if (restricted)
      SYSLOG(INFO) << "DLP blocked screen sharing";
    DlpBooleanHistogram(dlp::kScreenShareBlockedUMA, restricted);
    return restricted;
  }

  DCHECK_EQ(media_id.type, content::DesktopMediaID::Type::TYPE_WINDOW);
  aura::Window* window = content::DesktopMediaID::GetNativeWindowById(media_id);
  if (!window) {
    DlpBooleanHistogram(dlp::kScreenShareBlockedUMA, false);
    return false;
  }
  for (auto& entry : confidential_web_contents_) {
    aura::Window* web_contents_window = entry.first->GetNativeView();
    if (entry.second.HasRestriction(DlpContentRestriction::kScreenShare) &&
        window->Contains(web_contents_window)) {
      SYSLOG(INFO) << "DLP blocked screen sharing";
      DlpBooleanHistogram(dlp::kScreenShareBlockedUMA, true);
      return true;
    }
  }

  DlpBooleanHistogram(dlp::kScreenShareBlockedUMA, false);
  return false;
}

void DlpContentManager::OnVideoCaptureStarted(const ScreenshotArea& area) {
  if (IsVideoCaptureRestricted(area)) {
    if (ash::features::IsCaptureModeEnabled())
      ChromeCaptureModeDelegate::Get()->InterruptVideoRecordingIfAny();
    return;
  }
  DCHECK(!running_video_capture_area_.has_value());
  running_video_capture_area_.emplace(area);
}

void DlpContentManager::OnVideoCaptureStopped() {
  running_video_capture_area_.reset();
}

bool DlpContentManager::IsCaptureModeInitRestricted() const {
  const bool restricted = GetOnScreenPresentRestrictions().HasRestriction(
                              DlpContentRestriction::kScreenshot) ||
                          GetOnScreenPresentRestrictions().HasRestriction(
                              DlpContentRestriction::kVideoCapture);
  if (restricted)
    SYSLOG(INFO) << "DLP blocked taking a screen capture";
  DlpBooleanHistogram(dlp::kCaptureModeInitBlockedUMA, restricted);
  return restricted;
}

void DlpContentManager::OnScreenCaptureStarted(
    const std::string& label,
    std::vector<content::DesktopMediaID> screen_capture_ids,
    const std::u16string& application_title,
    content::MediaStreamUI::StateChangeCallback state_change_callback) {
  for (const content::DesktopMediaID& id : screen_capture_ids) {
    ScreenCaptureInfo capture_info(label, id, application_title,
                                   state_change_callback);
    DCHECK(!base::Contains(running_screen_captures_, capture_info));
    running_screen_captures_.push_back(capture_info);
  }
  CheckRunningScreenCaptures();
}

void DlpContentManager::OnScreenCaptureStopped(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  base::EraseIf(running_screen_captures_,
                [=](const ScreenCaptureInfo& capture) -> bool {
                  const bool erased =
                      capture.label == label && capture.media_id == media_id;
                  if (erased && capture.showing_paused_notification)
                    HideDlpScreenCapturePausedNotification(capture.label);
                  if (erased && capture.showing_resumed_notification)
                    HideDlpScreenCaptureResumedNotification(capture.label);
                  return erased;
                });
  MaybeUpdateScreenCaptureNotification();
}

/* static */
void DlpContentManager::SetDlpContentManagerForTesting(
    DlpContentManager* dlp_content_manager) {
  if (g_dlp_content_manager)
    delete g_dlp_content_manager;
  g_dlp_content_manager = dlp_content_manager;
}

/* static */
void DlpContentManager::ResetDlpContentManagerForTesting() {
  g_dlp_content_manager = nullptr;
}

DlpContentManager::ScreenCaptureInfo::ScreenCaptureInfo() = default;
DlpContentManager::ScreenCaptureInfo::ScreenCaptureInfo(
    const std::string& label,
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    content::MediaStreamUI::StateChangeCallback state_change_callback)
    : label(label),
      media_id(media_id),
      application_title(application_title),
      state_change_callback(state_change_callback) {}
DlpContentManager::ScreenCaptureInfo::ScreenCaptureInfo(
    const DlpContentManager::ScreenCaptureInfo& other) = default;
DlpContentManager::ScreenCaptureInfo&
DlpContentManager::ScreenCaptureInfo::operator=(
    const DlpContentManager::ScreenCaptureInfo& other) = default;
DlpContentManager::ScreenCaptureInfo::~ScreenCaptureInfo() = default;

bool DlpContentManager::ScreenCaptureInfo::operator==(
    const DlpContentManager::ScreenCaptureInfo& other) const {
  return label == other.label && media_id == other.media_id;
}

bool DlpContentManager::ScreenCaptureInfo::operator!=(
    const DlpContentManager::ScreenCaptureInfo& other) const {
  return !(*this == other);
}

DlpContentManager::DlpContentManager() = default;

DlpContentManager::~DlpContentManager() = default;

void DlpContentManager::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  if (restriction_set.IsEmpty()) {
    RemoveFromConfidential(web_contents);
  } else {
    confidential_web_contents_[web_contents] = restriction_set;
    window_observers_[web_contents] = std::make_unique<DlpWindowObserver>(
        web_contents->GetNativeView(), this);
    if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
      MaybeChangeOnScreenRestrictions();
    }
  }
  CheckRunningScreenCaptures();
}

void DlpContentManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  RemoveFromConfidential(web_contents);
}

DlpContentRestrictionSet DlpContentManager::GetRestrictionSetForURL(
    const GURL& url) const {
  DlpContentRestrictionSet set;
  DlpRulesManager* dlp_rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!dlp_rules_manager)
    return set;

  const size_t kRestrictionsCount = 5;
  static constexpr std::array<
      std::pair<DlpRulesManager::Restriction, DlpContentRestriction>,
      kRestrictionsCount>
      kRestrictionsArray = {{{DlpRulesManager::Restriction::kScreenshot,
                              DlpContentRestriction::kScreenshot},
                             {DlpRulesManager::Restriction::kScreenshot,
                              DlpContentRestriction::kVideoCapture},
                             {DlpRulesManager::Restriction::kPrivacyScreen,
                              DlpContentRestriction::kPrivacyScreen},
                             {DlpRulesManager::Restriction::kPrinting,
                              DlpContentRestriction::kPrint},
                             {DlpRulesManager::Restriction::kScreenShare,
                              DlpContentRestriction::kScreenShare}}};

  for (const auto& restriction : kRestrictionsArray) {
    if (dlp_rules_manager->IsRestricted(url, restriction.first) ==
        DlpRulesManager::Level::kBlock) {
      set.SetRestriction(restriction.second);
    }
  }

  return set;
}

void DlpContentManager::OnVisibilityChanged(
    content::WebContents* web_contents) {
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManager::RemoveFromConfidential(
    content::WebContents* web_contents) {
  confidential_web_contents_.erase(web_contents);
  window_observers_.erase(web_contents);
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManager::MaybeChangeOnScreenRestrictions() {
  DlpContentRestrictionSet new_restriction_set;
  for (const auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() == content::Visibility::VISIBLE) {
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
  CheckRunningScreenCaptures();
}

void DlpContentManager::OnScreenRestrictionsChanged(
    const DlpContentRestrictionSet& added_restrictions,
    const DlpContentRestrictionSet& removed_restrictions) const {
  DCHECK(!(added_restrictions.HasRestriction(
               DlpContentRestriction::kPrivacyScreen) &&
           removed_restrictions.HasRestriction(
               DlpContentRestriction::kPrivacyScreen)));
  if (added_restrictions.HasRestriction(
          DlpContentRestriction::kPrivacyScreen)) {
    SYSLOG(INFO) << "DLP enforced privacy screen";
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, true);
    ash::PrivacyScreenDlpHelper::Get()->SetEnforced(true);
  }

  if (removed_restrictions.HasRestriction(
          DlpContentRestriction::kPrivacyScreen)) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DlpContentManager::MaybeRemovePrivacyScreenEnforcement,
                       base::Unretained(this)),
        kPrivacyScreenOffDelay);
  }
}

void DlpContentManager::MaybeRemovePrivacyScreenEnforcement() const {
  if (!GetOnScreenPresentRestrictions().HasRestriction(
          DlpContentRestriction::kPrivacyScreen)) {
    SYSLOG(INFO) << "DLP removed enforcement of privacy screen";
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, false);
    ash::PrivacyScreenDlpHelper::Get()->SetEnforced(false);
  }
}

bool DlpContentManager::IsAreaRestricted(
    const ScreenshotArea& area,
    DlpContentRestriction restriction) const {
  // Fullscreen - restricted if any confidential data is visible.
  if (area.type == ScreenshotType::kAllRootWindows) {
    return GetOnScreenPresentRestrictions().HasRestriction(restriction);
  }

  // Window - restricted if the window contains confidential data.
  if (area.type == ScreenshotType::kWindow) {
    DCHECK(area.window);
    for (auto& entry : confidential_web_contents_) {
      aura::Window* web_contents_window = entry.first->GetNativeView();
      if (entry.second.HasRestriction(restriction) &&
          area.window->Contains(web_contents_window)) {
        return true;
      }
    }
    return false;
  }

  DCHECK_EQ(area.type, ScreenshotType::kPartialWindow);
  DCHECK(area.rect);
  DCHECK(area.window);
  // Partial - restricted if any visible confidential WebContents intersects
  // with the area.
  for (auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() != content::Visibility::VISIBLE ||
        !entry.second.HasRestriction(restriction)) {
      continue;
    }
    aura::Window* web_contents_window = entry.first->GetNativeView();
    if (web_contents_window->occlusion_state() ==
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
    if (!intersection.IsEmpty() &&
        !web_contents_window->occluded_region_in_root().contains(
            gfx::RectToSkIRect(intersection))) {
      return true;
    }
  }

  return false;
}

void DlpContentManager::CheckRunningVideoCapture() {
  if (!running_video_capture_area_.has_value())
    return;
  if (IsAreaRestricted(*running_video_capture_area_,
                       DlpContentRestriction::kVideoCapture)) {
    if (ash::features::IsCaptureModeEnabled()) {
      SYSLOG(INFO) << "DLP interrupted screen recording";
      DlpBooleanHistogram(dlp::kVideoCaptureInterruptedUMA, true);
      ChromeCaptureModeDelegate::Get()->InterruptVideoRecordingIfAny();
    }
    running_video_capture_area_.reset();
  }
}

void DlpContentManager::MaybeUpdateScreenCaptureNotification() {
  for (auto& capture : running_screen_captures_) {
    // If the capture was paused and a "paused" notification was shown, but the
    // capture is resumed - hide the "paused" notification.
    if (capture.showing_paused_notification && capture.is_running) {
      HideDlpScreenCapturePausedNotification(capture.label);
      capture.showing_paused_notification = false;
      // If the capture was paused and later resumed - show a "resumed"
      // notification if not yet shown.
      if (!capture.showing_resumed_notification) {
        ShowDlpScreenCaptureResumedNotification(capture.label,
                                                capture.application_title);
        capture.showing_resumed_notification = true;
      }
    }
    // If the capture was resumed and "resumed" notification was shown, but the
    // capture is not running anymore - hide the "resumed" notification.
    if (capture.showing_resumed_notification && !capture.is_running) {
      HideDlpScreenCaptureResumedNotification(capture.label);
      capture.showing_resumed_notification = false;
    }
    // If the capture was paused, but no notification is yet shown - show a
    // "paused" notification.
    if (!capture.showing_paused_notification && !capture.is_running) {
      ShowDlpScreenCapturePausedNotification(capture.label,
                                             capture.application_title);
      capture.showing_paused_notification = true;
    }
  }
}

void DlpContentManager::CheckRunningScreenCaptures() {
  for (auto& capture : running_screen_captures_) {
    bool is_allowed = !IsScreenCaptureRestricted(capture.media_id);
    if (is_allowed != capture.is_running) {
      SYSLOG(INFO) << "DLP " << (is_allowed ? "resumed" : "paused")
                   << " running screen share";
      DlpBooleanHistogram(dlp::kScreenSharePausedOrResumedUMA, !is_allowed);
      capture.state_change_callback.Run(
          capture.media_id, capture.is_running
                                ? blink::mojom::MediaStreamStateChange::PAUSE
                                : blink::mojom::MediaStreamStateChange::PLAY);
      capture.is_running = is_allowed;
      MaybeUpdateScreenCaptureNotification();
    }
  }
}

// static
base::TimeDelta DlpContentManager::GetPrivacyScreenOffDelayForTesting() {
  return kPrivacyScreenOffDelay;
}

// ScopedDlpContentManagerForTesting
ScopedDlpContentManagerForTesting::ScopedDlpContentManagerForTesting(
    DlpContentManager* test_dlp_content_manager) {
  DlpContentManager::SetDlpContentManagerForTesting(test_dlp_content_manager);
}

ScopedDlpContentManagerForTesting::~ScopedDlpContentManagerForTesting() {
  DlpContentManager::ResetDlpContentManagerForTesting();
}

}  // namespace policy
