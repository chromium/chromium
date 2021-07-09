// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"
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

// Reports events to `reporting_manager`.
void ReportEvent(GURL url,
                 DlpRulesManager::Restriction restriction,
                 DlpRulesManager::Level level,
                 const DlpReportingManager* reporting_manager) {
  DCHECK(reporting_manager);

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager)
    return;

  const std::string src_url =
      rules_manager->GetSourceUrlPattern(url, restriction, level);

  reporting_manager->ReportEvent(src_url, restriction, level);
}

}  // namespace

static DlpContentManager* g_dlp_content_manager = nullptr;

// static
DlpContentManager* DlpContentManager::Get() {
  if (!g_dlp_content_manager) {
    g_dlp_content_manager = new DlpContentManager();
    g_dlp_content_manager->Init();
  }
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
  RestrictionLevelAndUrl restriction_info =
      GetAreaRestrictionInfo(area, DlpContentRestriction::kScreenshot);
  const bool is_blocked =
      restriction_info.level == DlpRulesManager::Level::kBlock;
  if (is_blocked || restriction_info.level == DlpRulesManager::Level::kReport) {
    SYSLOG(INFO) << "DLP blocked taking a screenshot";
    if (reporting_manager_)
      ReportEvent(restriction_info.url,
                  DlpRulesManager::Restriction::kScreenshot,
                  restriction_info.level, reporting_manager_);
  }
  DlpBooleanHistogram(dlp::kScreenshotBlockedUMA, is_blocked);
  return is_blocked;
}

bool DlpContentManager::IsVideoCaptureRestricted(
    const ScreenshotArea& area) const {
  RestrictionLevelAndUrl restriction_info =
      GetAreaRestrictionInfo(area, DlpContentRestriction::kVideoCapture);
  const bool is_blocked =
      restriction_info.level == DlpRulesManager::Level::kBlock;
  if (is_blocked || restriction_info.level == DlpRulesManager::Level::kReport) {
    SYSLOG(INFO) << "DLP blocked taking a video capture";
    if (reporting_manager_)
      ReportEvent(restriction_info.url,
                  DlpRulesManager::Restriction::kScreenshot,
                  restriction_info.level, reporting_manager_);
  }
  DlpBooleanHistogram(dlp::kVideoCaptureBlockedUMA, is_blocked);
  return is_blocked;
}

bool DlpContentManager::IsPrintingRestricted(
    content::WebContents* web_contents) const {
  RestrictionLevelAndUrl restriction_info =
      GetPrintingRestrictionInfo(web_contents);
  const bool is_blocked =
      restriction_info.level == DlpRulesManager::Level::kBlock;
  DlpBooleanHistogram(dlp::kPrintingBlockedUMA, is_blocked);
  if (is_blocked || restriction_info.level == DlpRulesManager::Level::kReport) {
    SYSLOG(INFO) << "DLP blocked printing";
    if (reporting_manager_)
      ReportEvent(restriction_info.url, DlpRulesManager::Restriction::kPrinting,
                  restriction_info.level, reporting_manager_);
  }

  return is_blocked;
}

bool DlpContentManager::ShouldWarnBeforePrinting(
    content::WebContents* web_contents) const {
  RestrictionLevelAndUrl restriction_info =
      GetPrintingRestrictionInfo(web_contents);
  // TODO(crbug.com/1227700): Add reporting and metrics for WARN
  return restriction_info.level == DlpRulesManager::Level::kWarn;
}

bool DlpContentManager::IsScreenCaptureRestricted(
    const content::DesktopMediaID& media_id) const {
  RestrictionLevelAndUrl restriction_info =
      GetScreenCaptureRestrictionInfo(media_id);
  const bool is_blocked =
      restriction_info.level == DlpRulesManager::Level::kBlock;
  if (is_blocked || restriction_info.level == DlpRulesManager::Level::kReport) {
    SYSLOG(INFO) << "DLP blocked screen sharing";
    if (reporting_manager_)
      ReportEvent(restriction_info.url,
                  DlpRulesManager::Restriction::kScreenShare,
                  restriction_info.level, reporting_manager_);
  }
  DlpBooleanHistogram(dlp::kScreenShareBlockedUMA, is_blocked);
  return is_blocked;
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
  RestrictionLevelAndUrl screenshot_restriction_info =
      GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(
          DlpContentRestriction::kScreenshot);
  RestrictionLevelAndUrl videocapture_restriction_info =
      GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(
          DlpContentRestriction::kVideoCapture);
  RestrictionLevelAndUrl restriction_info =
      screenshot_restriction_info.level >= videocapture_restriction_info.level
          ? screenshot_restriction_info
          : videocapture_restriction_info;
  const bool is_blocked =
      restriction_info.level == DlpRulesManager::Level::kBlock;
  if (is_blocked || restriction_info.level == DlpRulesManager::Level::kReport) {
    SYSLOG(INFO) << "DLP blocked taking a screen capture";
    if (reporting_manager_)
      ReportEvent(restriction_info.url,
                  DlpRulesManager::Restriction::kScreenshot,
                  restriction_info.level, reporting_manager_);
  }
  DlpBooleanHistogram(dlp::kCaptureModeInitBlockedUMA, is_blocked);
  return is_blocked;
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

void DlpContentManager::Init() {
  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (rules_manager)
    reporting_manager_ =
        DlpRulesManagerFactory::GetForPrimaryProfile()->GetReportingManager();
}

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
    DlpRulesManager::Level level =
        dlp_rules_manager->IsRestricted(url, restriction.first);
    if (level == DlpRulesManager::Level::kNotSet ||
        level == DlpRulesManager::Level::kAllow)
      continue;
    set.SetRestriction(restriction.second, level, url);
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
  DCHECK(!(added_restrictions.GetRestrictionLevel(
               DlpContentRestriction::kPrivacyScreen) ==
               DlpRulesManager::Level::kBlock &&
           removed_restrictions.GetRestrictionLevel(
               DlpContentRestriction::kPrivacyScreen) ==
               DlpRulesManager::Level::kBlock));
  RestrictionLevelAndUrl added_restriction_info =
      added_restrictions.GetRestrictionLevelAndUrl(
          DlpContentRestriction::kPrivacyScreen);
  if (added_restriction_info.level == DlpRulesManager::Level::kBlock) {
    SYSLOG(INFO) << "DLP enforced privacy screen";
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, true);
    ash::PrivacyScreenDlpHelper::Get()->SetEnforced(true);
  }
  if (added_restriction_info.level == DlpRulesManager::Level::kBlock ||
      added_restriction_info.level == DlpRulesManager::Level::kReport) {
    if (reporting_manager_)
      ReportEvent(added_restriction_info.url,
                  DlpRulesManager::Restriction::kPrivacyScreen,
                  added_restriction_info.level, reporting_manager_);
  }

  if (removed_restrictions.GetRestrictionLevel(
          DlpContentRestriction::kPrivacyScreen) ==
      DlpRulesManager::Level::kBlock) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DlpContentManager::MaybeRemovePrivacyScreenEnforcement,
                       base::Unretained(this)),
        kPrivacyScreenOffDelay);
  }
}

void DlpContentManager::MaybeRemovePrivacyScreenEnforcement() const {
  if (GetOnScreenPresentRestrictions().GetRestrictionLevel(
          DlpContentRestriction::kPrivacyScreen) !=
      DlpRulesManager::Level::kBlock) {
    SYSLOG(INFO) << "DLP removed enforcement of privacy screen";
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, false);
    ash::PrivacyScreenDlpHelper::Get()->SetEnforced(false);
  }
}

RestrictionLevelAndUrl DlpContentManager::GetAreaRestrictionInfo(
    const ScreenshotArea& area,
    DlpContentRestriction restriction) const {
  // Fullscreen - restricted if any confidential data is visible.
  if (area.type == ScreenshotType::kAllRootWindows) {
    return GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(
        restriction);
  }

  // Window - restricted if the window contains confidential data.
  if (area.type == ScreenshotType::kWindow) {
    DCHECK(area.window);
    RestrictionLevelAndUrl restriction_info;
    for (auto& entry : confidential_web_contents_) {
      aura::Window* web_contents_window = entry.first->GetNativeView();
      if (area.window->Contains(web_contents_window) &&
          entry.second.GetRestrictionLevel(restriction) >
              restriction_info.level) {
        restriction_info = entry.second.GetRestrictionLevelAndUrl(restriction);
      }
    }
    return restriction_info;
  }

  DCHECK_EQ(area.type, ScreenshotType::kPartialWindow);
  DCHECK(area.rect);
  DCHECK(area.window);
  // Partial - restricted if any visible confidential WebContents intersects
  // with the area.
  RestrictionLevelAndUrl restriction_info;
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
    if (!intersection.IsEmpty() &&
        !web_contents_window->occluded_region_in_root().contains(
            gfx::RectToSkIRect(intersection)) &&
        entry.second.GetRestrictionLevel(restriction) >
            restriction_info.level) {
      restriction_info = entry.second.GetRestrictionLevelAndUrl(restriction);
    }
  }

  return restriction_info;
}

RestrictionLevelAndUrl DlpContentManager::GetScreenCaptureRestrictionInfo(
    const content::DesktopMediaID& media_id) const {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    return GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(
        DlpContentRestriction::kScreenShare);
  }
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(
                media_id.web_contents_id.render_process_id,
                media_id.web_contents_id.main_render_frame_id));

    return GetConfidentialRestrictions(web_contents)
        .GetRestrictionLevelAndUrl(DlpContentRestriction::kScreenShare);
  }
  DCHECK_EQ(media_id.type, content::DesktopMediaID::Type::TYPE_WINDOW);
  RestrictionLevelAndUrl restriction_info;
  aura::Window* window = content::DesktopMediaID::GetNativeWindowById(media_id);
  if (window) {
    for (auto& entry : confidential_web_contents_) {
      aura::Window* web_contents_window = entry.first->GetNativeView();
      if (window->Contains(web_contents_window) &&
          entry.second.GetRestrictionLevel(
              DlpContentRestriction::kScreenShare) > restriction_info.level) {
        restriction_info = entry.second.GetRestrictionLevelAndUrl(
            DlpContentRestriction::kScreenShare);
      }
    }
  }
  return restriction_info;
}

void DlpContentManager::CheckRunningVideoCapture() {
  if (!running_video_capture_area_.has_value())
    return;
  RestrictionLevelAndUrl restriction_info = GetAreaRestrictionInfo(
      *running_video_capture_area_, DlpContentRestriction::kVideoCapture);
  if (restriction_info.level == DlpRulesManager::Level::kBlock) {
    if (ash::features::IsCaptureModeEnabled()) {
      SYSLOG(INFO) << "DLP interrupted screen recording";
      DlpBooleanHistogram(dlp::kVideoCaptureInterruptedUMA, true);
      ChromeCaptureModeDelegate::Get()->InterruptVideoRecordingIfAny();
    }
    running_video_capture_area_.reset();
  }
  if (restriction_info.level == DlpRulesManager::Level::kBlock ||
      restriction_info.level == DlpRulesManager::Level::kReport) {
    if (reporting_manager_)
      ReportEvent(restriction_info.url,
                  DlpRulesManager::Restriction::kScreenshot,
                  restriction_info.level, reporting_manager_);
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
    RestrictionLevelAndUrl restriction_info =
        GetScreenCaptureRestrictionInfo(capture.media_id);
    const bool is_allowed =
        restriction_info.level != DlpRulesManager::Level::kBlock;
    const bool is_reported =
        restriction_info.level == DlpRulesManager::Level::kBlock ||
        restriction_info.level == DlpRulesManager::Level::kReport;
    if (is_reported && capture.is_running) {
      SYSLOG(INFO) << "DLP " << (is_allowed ? "resumed" : "paused")
                   << " running screen share";
      if (reporting_manager_) {
        ReportEvent(restriction_info.url,
                    DlpRulesManager::Restriction::kScreenShare,
                    restriction_info.level, reporting_manager_);
      }
    }
    if (is_allowed != capture.is_running) {
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

void DlpContentManager::SetReportingManagerForTesting(
    DlpReportingManager* reporting_manager) {
  reporting_manager_ = reporting_manager;
}

// static
base::TimeDelta DlpContentManager::GetPrivacyScreenOffDelayForTesting() {
  return kPrivacyScreenOffDelay;
}

RestrictionLevelAndUrl DlpContentManager::GetPrintingRestrictionInfo(
    content::WebContents* web_contents) const {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedded
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  web_contents =
      guest_view ? guest_view->embedder_web_contents() : web_contents;

  return GetConfidentialRestrictions(web_contents)
      .GetRestrictionLevelAndUrl(DlpContentRestriction::kPrint);
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
