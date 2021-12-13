// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
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

// Reports events to `reporting_manager`.
void ReportEvent(GURL url,
                 DlpRulesManager::Restriction restriction,
                 DlpRulesManager::Level level,
                 DlpReportingManager* reporting_manager) {
  DCHECK(reporting_manager);

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager)
    return;

  const std::string src_url =
      rules_manager->GetSourceUrlPattern(url, restriction, level);

  reporting_manager->ReportEvent(src_url, restriction, level);
}

// Reports events of type DlpPolicyEvent::Mode::WARN_PROCEED to
// `reporting_manager`.
void ReportWarningProceededEvent(const GURL& url,
                                 DlpRulesManager::Restriction restriction,
                                 DlpReportingManager* reporting_manager) {
  if (!reporting_manager)
    return;

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (rules_manager) {
    const std::string src_url = rules_manager->GetSourceUrlPattern(
        url, restriction, DlpRulesManager::Level::kWarn);
    reporting_manager->ReportWarningProceededEvent(src_url, restriction);
  }
}

// Helper method to create a callback with ReportWarningProceededEvent function.
bool MaybeReportWarningProceededEvent(GURL url,
                                      DlpRulesManager::Restriction restriction,
                                      DlpReportingManager* reporting_manager,
                                      bool should_proceed) {
  if (should_proceed) {
    ReportWarningProceededEvent(url, restriction, reporting_manager);
  }
  return should_proceed;
}

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

bool DlpContentManager::IsScreenshotRestricted(const ScreenshotArea& area) {
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kScreenshotBlockedUMA,
                      IsBlocked(info.restriction_info));
  return IsBlocked(info.restriction_info);
}

bool DlpContentManager::IsScreenshotApiRestricted(const ScreenshotArea& area) {
  const ConfidentialContentsInfo info =
      GetAreaConfidentialContentsInfo(area, DlpContentRestriction::kScreenshot);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kScreenshotBlockedUMA,
                      IsBlocked(info.restriction_info));
  // TODO(crbug.com/1252736): Properly handle WARN for screenshots API.
  return IsBlocked(info.restriction_info) || IsWarn(info.restriction_info);
}

void DlpContentManager::CheckScreenshotRestriction(
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

void DlpContentManager::CheckPrintingRestriction(
    content::WebContents* web_contents,
    OnDlpRestrictionCheckedCallback callback) {
  const RestrictionLevelAndUrl restriction_info =
      GetPrintingRestrictionInfo(web_contents);
  MaybeReportEvent(restriction_info, DlpRulesManager::Restriction::kPrinting);
  DlpBooleanHistogram(dlp::kPrintingBlockedUMA, IsBlocked(restriction_info));
  if (IsBlocked(restriction_info)) {
    ShowDlpPrintDisabledNotification();
    std::move(callback).Run(false);
    return;
  }

  if (IsWarn(restriction_info)) {
    // Check if the contents were already allowed and don't warn in that case.
    if (user_allowed_contents_cache_.Contains(
            web_contents, DlpRulesManager::Restriction::kPrinting)) {
      ReportWarningProceededEvent(restriction_info.url,
                                  DlpRulesManager::Restriction::kPrinting,
                                  reporting_manager_);
      std::move(callback).Run(true);
      return;
    }

    // Immediately report a warning event.
    ReportWarningEvent(restriction_info,
                       DlpRulesManager::Restriction::kPrinting);

    // Report a warning proceeded event only after a user proceeds with printing
    // in the warn dialog.
    auto reporting_callback = base::BindOnce(
        &MaybeReportWarningProceededEvent, restriction_info.url,
        DlpRulesManager::Restriction::kPrinting, reporting_manager_);
    warn_notifier_->ShowDlpPrintWarningDialog(base::BindOnce(
        &DlpContentManager::OnDlpWarnDialogReply, base::Unretained(this),
        DlpConfidentialContents({web_contents}),
        DlpRulesManager::Restriction::kPrinting,
        std::move(reporting_callback).Then(std::move(callback))));
    return;
  }

  // No restrictions apply.
  std::move(callback).Run(true);
}

bool DlpContentManager::IsScreenCaptureRestricted(
    const content::DesktopMediaID& media_id) {
  const ConfidentialContentsInfo info =
      GetScreenShareConfidentialContentsInfo(media_id);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenShare);
  DlpBooleanHistogram(dlp::kScreenShareBlockedUMA,
                      IsBlocked(info.restriction_info));
  return IsBlocked(info.restriction_info);
}

void DlpContentManager::CheckScreenShareRestriction(
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    OnDlpRestrictionCheckedCallback callback) {
  ConfidentialContentsInfo info =
      GetScreenShareConfidentialContentsInfo(media_id);
  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenShare);
  DlpBooleanHistogram(dlp::kScreenShareBlockedUMA,
                      IsBlocked(info.restriction_info));
  if (IsBlocked(info.restriction_info)) {
    ShowDlpScreenShareDisabledNotification(application_title);
    std::move(callback).Run(false);
    return;
  }
  if (is_screen_share_warning_mode_enabled_ && IsWarn(info.restriction_info)) {
    // Check which of the contents were already allowed and don't warn for
    // those.
    RemoveAllowedContents(info.confidential_contents,
                          DlpRulesManager::Restriction::kScreenShare);
    if (info.confidential_contents.IsEmpty()) {
      // The user already allowed all the visible content.
      std::move(callback).Run(true);
      return;
    }
    // base::Unretained(this) is safe here because DlpContentManager is
    // initialized as a singleton that's always available in the system.
    warn_notifier_->ShowDlpScreenShareWarningDialog(
        base::BindOnce(&DlpContentManager::OnDlpWarnDialogReply,
                       base::Unretained(this), info.confidential_contents,
                       DlpRulesManager::Restriction::kScreenShare,
                       std::move(callback)),
        info.confidential_contents, application_title);
    return;
  }
  // No restrictions apply.
  std::move(callback).Run(true);
}

void DlpContentManager::OnVideoCaptureStarted(const ScreenshotArea& area) {
  if (IsScreenshotRestricted(area)) {
    InterruptVideoRecording();
    return;
  }
  DCHECK(!running_video_capture_info_.has_value());
  running_video_capture_info_.emplace(area);
}

void DlpContentManager::CheckStoppedVideoCapture(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  // If some confidential content was shown during the recording, but not
  // before, warn the user before saving the file.
  if (running_video_capture_info_.has_value() &&
      !running_video_capture_info_->confidential_contents.IsEmpty()) {
    warn_notifier_->ShowDlpVideoCaptureWarningDialog(
        std::move(callback),
        running_video_capture_info_->confidential_contents);
  } else {
    std::move(callback).Run(/*proceed=*/true);
  }

  running_video_capture_info_.reset();
}

bool DlpContentManager::IsCaptureModeInitRestricted() {
  const RestrictionLevelAndUrl restriction_info =
      GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(
          DlpContentRestriction::kScreenshot);
  MaybeReportEvent(restriction_info, DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kCaptureModeInitBlockedUMA,
                      IsBlocked(restriction_info));
  return IsBlocked(restriction_info);
}

void DlpContentManager::CheckCaptureModeInitRestriction(
    ash::OnCaptureModeDlpRestrictionChecked callback) {
  const ConfidentialContentsInfo info =
      GetConfidentialContentsOnScreen(DlpContentRestriction::kScreenshot);

  MaybeReportEvent(info.restriction_info,
                   DlpRulesManager::Restriction::kScreenshot);
  DlpBooleanHistogram(dlp::kCaptureModeInitBlockedUMA,
                      IsBlocked(info.restriction_info));
  CheckScreenCaptureRestriction(info, std::move(callback));
}

void DlpContentManager::OnScreenCaptureStarted(
    const std::string& label,
    std::vector<content::DesktopMediaID> screen_capture_ids,
    const std::u16string& application_title,
    content::MediaStreamUI::StateChangeCallback state_change_callback) {
  for (const content::DesktopMediaID& id : screen_capture_ids) {
    ScreenShareInfo screen_share_info(label, id, application_title,
                                      state_change_callback);
    DCHECK(!base::Contains(running_screen_shares_, screen_share_info));
    running_screen_shares_.push_back(screen_share_info);
  }
  CheckRunningScreenShares();
}

void DlpContentManager::OnScreenCaptureStopped(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  base::EraseIf(
      running_screen_shares_, [=](ScreenShareInfo& screen_share_info) -> bool {
        const bool erased = screen_share_info.GetLabel() == label &&
                            screen_share_info.GetMediaId() == media_id;
        // Hide notifications if necessary.
        screen_share_info.HideNotifications();
        return erased;
      });
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

DlpContentManager::ScreenShareInfo::ScreenShareInfo() = default;
DlpContentManager::ScreenShareInfo::ScreenShareInfo(
    const std::string& label,
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    content::MediaStreamUI::StateChangeCallback state_change_callback)
    : label_(label),
      media_id_(media_id),
      application_title_(application_title),
      state_change_callback_(state_change_callback) {}
DlpContentManager::ScreenShareInfo::ScreenShareInfo(
    const DlpContentManager::ScreenShareInfo& other) = default;
DlpContentManager::ScreenShareInfo&
DlpContentManager::ScreenShareInfo::operator=(
    const DlpContentManager::ScreenShareInfo& other) = default;
DlpContentManager::ScreenShareInfo::~ScreenShareInfo() = default;

bool DlpContentManager::ScreenShareInfo::operator==(
    const DlpContentManager::ScreenShareInfo& other) const {
  return label_ == other.label_ && media_id_ == other.media_id_;
}

bool DlpContentManager::ScreenShareInfo::operator!=(
    const DlpContentManager::ScreenShareInfo& other) const {
  return !(*this == other);
}

const content::DesktopMediaID& DlpContentManager::ScreenShareInfo::GetMediaId()
    const {
  return media_id_;
}

const std::string& DlpContentManager::ScreenShareInfo::GetLabel() const {
  return label_;
}

const std::u16string& DlpContentManager::ScreenShareInfo::GetApplicationTitle()
    const {
  // TODO(crbug.com/1264793): Don't cache the application name, but compute it
  // here.
  return application_title_;
}

bool DlpContentManager::ScreenShareInfo::IsRunning() const {
  return is_running_;
}

void DlpContentManager::ScreenShareInfo::Pause() {
  DCHECK(is_running_);
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PAUSE);
  is_running_ = false;
}

void DlpContentManager::ScreenShareInfo::Resume() {
  DCHECK(!is_running_);
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PLAY);
  is_running_ = true;
}

void DlpContentManager::ScreenShareInfo::MaybeUpdateNotifications() {
  UpdatePausedNotification(/*show=*/!is_running_);
  UpdateResumedNotification(/*show=*/is_running_);
}

void DlpContentManager::ScreenShareInfo::HideNotifications() {
  UpdatePausedNotification(/*show=*/false);
  UpdateResumedNotification(/*show=*/false);
}

void DlpContentManager::ScreenShareInfo::UpdatePausedNotification(bool show) {
  if ((notification_state_ == NotificationState::kShowingPausedNotification) ==
      show)
    return;
  if (show) {
    ShowDlpScreenSharePausedNotification(label_, application_title_);
    notification_state_ = NotificationState::kShowingPausedNotification;
  } else {
    HideDlpScreenSharePausedNotification(label_);
    notification_state_ = NotificationState::kNotShowingNotification;
  }
}

void DlpContentManager::ScreenShareInfo::UpdateResumedNotification(bool show) {
  if ((notification_state_ == NotificationState::kShowingResumedNotification) ==
      show)
    return;
  if (show) {
    ShowDlpScreenShareResumedNotification(label_, application_title_);
    notification_state_ = NotificationState::kShowingResumedNotification;
  } else {
    HideDlpScreenShareResumedNotification(label_);
    notification_state_ = NotificationState::kNotShowingNotification;
  }
}

DlpContentManager::VideoCaptureInfo::VideoCaptureInfo(
    const ScreenshotArea& area)
    : area(area) {}

DlpContentManager::DlpContentManager() = default;

void DlpContentManager::Init() {
  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (rules_manager)
    reporting_manager_ =
        DlpRulesManagerFactory::GetForPrimaryProfile()->GetReportingManager();
  warn_notifier_ = std::make_unique<DlpWarnNotifier>();
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
  CheckRunningScreenShares();
}

void DlpContentManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  RemoveFromConfidential(web_contents);
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
  CheckRunningScreenShares();
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

  if (added_restriction_info.level == DlpRulesManager::Level::kBlock ||
      added_restriction_info.level == DlpRulesManager::Level::kReport) {
    if (reporting_manager_) {
      ReportEvent(added_restriction_info.url,
                  DlpRulesManager::Restriction::kPrivacyScreen,
                  added_restriction_info.level, reporting_manager_);
    }
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
    DlpBooleanHistogram(dlp::kPrivacyScreenEnforcedUMA, false);
    ash::PrivacyScreenDlpHelper::Get()->SetEnforced(false);
  }
}

DlpContentManager::ConfidentialContentsInfo
DlpContentManager::GetConfidentialContentsOnScreen(
    DlpContentRestriction restriction) const {
  DlpContentManager::ConfidentialContentsInfo info;
  info.restriction_info =
      GetOnScreenPresentRestrictions().GetRestrictionLevelAndUrl(restriction);
  for (auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() != content::Visibility::VISIBLE)
      continue;
    if (entry.second.GetRestrictionLevel(restriction) ==
        info.restriction_info.level) {
      info.confidential_contents.Add(entry.first);
    }
  }
  return info;
}

DlpContentManager::ConfidentialContentsInfo
DlpContentManager::GetAreaConfidentialContentsInfo(
    const ScreenshotArea& area,
    DlpContentRestriction restriction) const {
  DlpContentManager::ConfidentialContentsInfo info;
  // Fullscreen - restricted if any confidential data is visible.
  if (area.type == ScreenshotType::kAllRootWindows) {
    return GetConfidentialContentsOnScreen(restriction);
  }

  // Window - restricted if the window contains confidential data.
  if (area.type == ScreenshotType::kWindow) {
    DCHECK(area.window);
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
    return info;
  }

  DCHECK_EQ(area.type, ScreenshotType::kPartialWindow);
  DCHECK(area.rect);
  DCHECK(area.window);
  // Partial - restricted if any visible confidential WebContents intersects
  // with the area.
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
  return info;
}

DlpContentManager::ConfidentialContentsInfo
DlpContentManager::GetScreenShareConfidentialContentsInfo(
    const content::DesktopMediaID& media_id) const {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    return GetConfidentialContentsOnScreen(DlpContentRestriction::kScreenShare);
  }
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(
                media_id.web_contents_id.render_process_id,
                media_id.web_contents_id.main_render_frame_id));
    ConfidentialContentsInfo info;
    if (web_contents && !web_contents->IsBeingDestroyed()) {
      info.restriction_info =
          GetConfidentialRestrictions(web_contents)
              .GetRestrictionLevelAndUrl(DlpContentRestriction::kScreenShare);
      info.confidential_contents.Add(web_contents);
    } else {
      NOTREACHED();
    }
    return info;
  }
  DCHECK_EQ(media_id.type, content::DesktopMediaID::Type::TYPE_WINDOW);
  ConfidentialContentsInfo info;
  aura::Window* window = content::DesktopMediaID::GetNativeWindowById(media_id);
  if (window) {
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
  }
  return info;
}

void DlpContentManager::CheckRunningVideoCapture() {
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

void DlpContentManager::CheckRunningScreenShares() {
  for (auto& screen_share : running_screen_shares_) {
    ConfidentialContentsInfo info =
        GetScreenShareConfidentialContentsInfo(screen_share.GetMediaId());
    if (IsBlocked(info.restriction_info)) {
      if (screen_share.IsRunning()) {
        screen_share.Pause();
        MaybeReportEvent(info.restriction_info,
                         DlpRulesManager::Restriction::kScreenShare);
        DlpBooleanHistogram(dlp::kScreenSharePausedOrResumedUMA, true);
        screen_share.MaybeUpdateNotifications();
      }
      return;
    }
    if (is_screen_share_warning_mode_enabled_ &&
        IsWarn(info.restriction_info)) {
      // Check which of the contents were already allowed and don't warn for
      // those.
      RemoveAllowedContents(info.confidential_contents,
                            DlpRulesManager::Restriction::kScreenShare);
      if (info.confidential_contents.IsEmpty()) {
        // The user already allowed all the visible content.
        if (!screen_share.IsRunning()) {
          screen_share.Resume();
          screen_share.MaybeUpdateNotifications();
        }
        return;
      }
      if (screen_share.IsRunning()) {
        screen_share.Pause();
        screen_share.HideNotifications();
      }
      // base::Unretained(this) is safe here because DlpContentManager is
      // initialized as a singleton that's always available in the system.
      warn_notifier_->ShowDlpScreenShareWarningDialog(
          base::BindOnce(&DlpContentManager::OnDlpScreenShareWarnDialogReply,
                         base::Unretained(this), info.confidential_contents,
                         screen_share),
          info.confidential_contents, screen_share.GetApplicationTitle());
      return;
    }
    // No restrictions apply, only resume if necessary.
    if (!screen_share.IsRunning()) {
      screen_share.Resume();
      DlpBooleanHistogram(dlp::kScreenSharePausedOrResumedUMA, false);
      screen_share.MaybeUpdateNotifications();
    }
  }
}

void DlpContentManager::SetReportingManagerForTesting(
    DlpReportingManager* reporting_manager) {
  reporting_manager_ = reporting_manager;
}

void DlpContentManager::SetWarnNotifierForTesting(
    std::unique_ptr<DlpWarnNotifier> warn_notifier) {
  DCHECK(warn_notifier);
  warn_notifier_ = std::move(warn_notifier);
}

void DlpContentManager::ResetWarnNotifierForTesting() {
  warn_notifier_ = std::make_unique<DlpWarnNotifier>();
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

void DlpContentManager::CheckScreenCaptureRestriction(
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
      std::move(callback).Run(true);
      return;
    }
    // base::Unretained(this) is safe here because DlpContentManager is
    // initialized as a singleton that's always available in the system.
    warn_notifier_->ShowDlpScreenCaptureWarningDialog(
        base::BindOnce(&DlpContentManager::OnDlpWarnDialogReply,
                       base::Unretained(this), info.confidential_contents,
                       DlpRulesManager::Restriction::kScreenshot,
                       std::move(callback)),
        info.confidential_contents);
    return;
  }
  // No restrictions apply.
  std::move(callback).Run(true);
}

void DlpContentManager::OnDlpScreenShareWarnDialogReply(
    const DlpConfidentialContents& confidential_contents,
    ScreenShareInfo screen_share,
    bool should_proceed) {
  if (should_proceed) {
    screen_share.Resume();
    for (const auto& content : confidential_contents.GetContents()) {
      user_allowed_contents_cache_.Cache(
          content, DlpRulesManager::Restriction::kScreenShare);
    }
  } else {
    // TODO(crbug.com/1259605): stop instead of pause.
    screen_share.Pause();
  }
  screen_share.MaybeUpdateNotifications();
}

void DlpContentManager::OnDlpWarnDialogReply(
    const DlpConfidentialContents& confidential_contents,
    DlpRulesManager::Restriction restriction,
    OnDlpRestrictionCheckedCallback callback,
    bool should_proceed) {
  if (should_proceed) {
    for (const auto& content : confidential_contents.GetContents()) {
      user_allowed_contents_cache_.Cache(content, restriction);
    }
  }
  std::move(callback).Run(should_proceed);
}

void DlpContentManager::MaybeReportEvent(
    const RestrictionLevelAndUrl& restriction_info,
    DlpRulesManager::Restriction restriction) {
  // TODO(crbug.com/1260302): Add reporting and metrics for WARN restrictions.
  if (IsReported(restriction_info) && reporting_manager_) {
    ReportEvent(restriction_info.url, restriction, restriction_info.level,
                reporting_manager_);
  }
}

void DlpContentManager::ReportWarningEvent(
    const RestrictionLevelAndUrl& restriction_info,
    DlpRulesManager::Restriction restriction) {
  DCHECK(IsWarn(restriction_info));
  if (reporting_manager_) {
    ReportEvent(restriction_info.url, restriction,
                DlpRulesManager::Level::kWarn, reporting_manager_);
  }
}

void DlpContentManager::RemoveAllowedContents(
    DlpConfidentialContents& contents,
    DlpRulesManager::Restriction restriction) {
  base::EraseIf(
      contents.GetContents(), [=](const DlpConfidentialContent& content) {
        return user_allowed_contents_cache_.Contains(content, restriction);
      });
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
