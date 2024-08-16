// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace policy {

namespace {
// Delay to wait to resume a screen share after a change in the confidentiality
// of captured data, to prevent flickering between resumed and paused states
// while the new content is being loaded. See b/259181514.
base::TimeDelta kScreenShareResumeDelay = base::Milliseconds(500);

// Reports events to `reporting_manager`.
void ReportEvent(GURL url,
                 DlpRulesManager::Restriction restriction,
                 DlpRulesManager::Level level,
                 data_controls::DlpReportingManager* reporting_manager) {
  DCHECK(reporting_manager);

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager)
    return;

  DlpRulesManager::RuleMetadata rule_metadata;
  const std::string src_pattern = rules_manager->GetSourceUrlPattern(
      url, restriction, level, &rule_metadata);
  const std::string src_url = url.is_empty() ? src_pattern : url.spec();
  reporting_manager->ReportEvent(src_url, restriction, level,
                                 rule_metadata.name,
                                 rule_metadata.obfuscated_id);
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

// Maps restriction to the correct suffix used for logging WarnProceeded
// metrics. Returns the suffix for supported restrictions and null otherwise.
const std::optional<std::string> RestrictionToWarnProceededUMASuffix(
    DlpRulesManager::Restriction restriction) {
  switch (restriction) {
    case DlpRulesManager::Restriction::kScreenShare:
      return std::make_optional(
          data_controls::dlp::kScreenShareWarnProceededUMA);
    case DlpRulesManager::Restriction::kPrinting:
      return std::make_optional(data_controls::dlp::kPrintingWarnProceededUMA);
    case DlpRulesManager::Restriction::kScreenshot:
      return std::make_optional(
          data_controls::dlp::kScreenshotWarnProceededUMA);
    case DlpRulesManager::Restriction::kUnknownRestriction:
    case DlpRulesManager::Restriction::kClipboard:
    case DlpRulesManager::Restriction::kPrivacyScreen:
    case DlpRulesManager::Restriction::kFiles:
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
  }
}

}  // namespace

DlpContentManager::WebContentsInfo::WebContentsInfo() = default;

DlpContentManager::WebContentsInfo::WebContentsInfo(
    content::WebContents* web_contents,
    DlpContentRestrictionSet restriction_set,
    std::vector<DlpContentTabHelper::RfhInfo> rfh_info_vector)
    : web_contents(web_contents),
      restriction_set(std::move(restriction_set)),
      rfh_info_vector(std::move(rfh_info_vector)) {}

DlpContentManager::WebContentsInfo::WebContentsInfo(const WebContentsInfo&) =
    default;

DlpContentManager::WebContentsInfo&
DlpContentManager::WebContentsInfo::operator=(const WebContentsInfo&) = default;

DlpContentManager::WebContentsInfo::~WebContentsInfo() = default;

// static
DlpContentManager* DlpContentManager::Get() {
  return static_cast<DlpContentManager*>(DlpContentObserver::Get());
}

DlpContentRestrictionSet DlpContentManager::GetConfidentialRestrictions(
    content::WebContents* web_contents) const {
  if (!base::Contains(confidential_web_contents_, web_contents))
    return DlpContentRestrictionSet();
  return confidential_web_contents_.at(web_contents);
}

bool DlpContentManager::IsScreenShareBlocked(
    content::WebContents* web_contents) const {
  return IsBlocked(GetConfidentialRestrictions(web_contents)
                       .GetRestrictionLevelAndUrl(
                           policy::DlpContentRestriction::kScreenShare));
}

void DlpContentManager::CheckPrintingRestriction(
    content::WebContents* web_contents,
    content::GlobalRenderFrameHostId rfh_id,
    WarningCallback callback) {
  const RestrictionLevelAndUrl restriction_info =
      GetPrintingRestrictionInfo(web_contents, rfh_id);
  MaybeReportEvent(restriction_info, DlpRulesManager::Restriction::kPrinting);
  data_controls::DlpBooleanHistogram(data_controls::dlp::kPrintingBlockedUMA,
                                     IsBlocked(restriction_info));
  data_controls::DlpBooleanHistogram(data_controls::dlp::kPrintingWarnedUMA,
                                     IsWarn(restriction_info));
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
      data_controls::DlpBooleanHistogram(
          data_controls::dlp::kPrintingWarnSilentProceededUMA, true);
      std::move(callback).Run(true);
      return;
    }

    // Immediately report a warning event.
    ReportWarningEvent(restriction_info.url,
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

bool DlpContentManager::IsScreenshotApiRestricted(
    content::WebContents* web_contents) {
  const RestrictionLevelAndUrl restriction_info =
      GetConfidentialRestrictions(web_contents)
          .GetRestrictionLevelAndUrl(DlpContentRestriction::kScreenshot);
  MaybeReportEvent(restriction_info, DlpRulesManager::Restriction::kScreenshot);
  if (IsWarn(restriction_info))
    ReportWarningEvent(restriction_info.url,
                       DlpRulesManager::Restriction::kScreenshot);
  data_controls::DlpBooleanHistogram(data_controls::dlp::kScreenshotBlockedUMA,
                                     IsBlocked(restriction_info));
  data_controls::DlpBooleanHistogram(data_controls::dlp::kScreenshotWarnedUMA,
                                     IsWarn(restriction_info));
  // TODO(crbug.com/1252736): Properly handle WARN for screenshots API.
  return IsBlocked(restriction_info) || IsWarn(restriction_info);
}

void DlpContentManager::SetReportingManagerForTesting(
    data_controls::DlpReportingManager* reporting_manager) {
  DCHECK(!reporting_manager_);
  DCHECK(reporting_manager);
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
void DlpContentManager::SetScreenShareResumeDelayForTesting(
    base::TimeDelta delay) {
  kScreenShareResumeDelay = delay;
}

DlpContentManager::ScreenShareInfo::ScreenShareInfo(
    const std::string& label,
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    base::OnceClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback,
    content::MediaStreamUI::SourceCallback source_callback)
    : label_(label),
      media_id_(media_id),
      application_title_(application_title),
      stop_callback_(std::move(stop_callback)),
      state_change_callback_(std::move(state_change_callback)),
      source_callback_(std::move(source_callback)) {
  auto* web_contents = GetWebContentsFromMediaId(media_id);
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
}

DlpContentManager::ScreenShareInfo::~ScreenShareInfo() {
  // Hide notifications if necessary.
  HideNotifications();
}

void DlpContentManager::ScreenShareInfo::UpdateAfterSourceChange(
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    base::OnceClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  DCHECK(state_ == State::kRunningBeforeSourceChange ||
         state_ == State::kPausedBeforeSourceChange);
  DCHECK(new_media_id_ == media_id);
  DCHECK(media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS);

  media_id_ = media_id;
  stop_callback_ = std::move(stop_callback);
  state_change_callback_ = std::move(state_change_callback);
  source_callback_ = std::move(source_callback);
  auto* web_contents = GetWebContentsFromMediaId(media_id);
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
  // If it's a resume after source change, request to start it if pending.
  StartIfPending();
  // This is called from AddScreenShare() which is only called when a new stream
  // is starting or when the source was successfully changed and the stream is
  // running again, so we can set the state to running.
  if (state_ == State::kPausedBeforeSourceChange) {
    state_ = State::kRunning;
    MaybeUpdateNotifications();
  } else {
    state_ = State::kRunning;
  }
}

bool DlpContentManager::ScreenShareInfo::operator==(
    const DlpContentManager::ScreenShareInfo& other) const {
  return label_ == other.label_ && media_id_ == other.media_id_;
}

bool DlpContentManager::ScreenShareInfo::operator!=(
    const DlpContentManager::ScreenShareInfo& other) const {
  return !(*this == other);
}

const content::DesktopMediaID& DlpContentManager::ScreenShareInfo::media_id()
    const {
  return media_id_;
}

const content::DesktopMediaID&
DlpContentManager::ScreenShareInfo::new_media_id() const {
  return new_media_id_;
}

void DlpContentManager::ScreenShareInfo::set_new_media_id(
    const content::DesktopMediaID& media_id) {
  new_media_id_ = media_id;
}

const std::string& DlpContentManager::ScreenShareInfo::label() const {
  return label_;
}

const std::u16string& DlpContentManager::ScreenShareInfo::application_title()
    const {
  return application_title_;
}

DlpContentManager::ScreenShareInfo::State
DlpContentManager::ScreenShareInfo::state() const {
  return state_;
}

base::WeakPtr<content::WebContents>
DlpContentManager::ScreenShareInfo::web_contents() const {
  return web_contents_;
}

void DlpContentManager::ScreenShareInfo::set_dialog_widget(
    base::WeakPtr<views::Widget> dialog_widget) {
  DCHECK(!HasOpenDialogWidget());
  dialog_widget_ = dialog_widget;
}

void DlpContentManager::ScreenShareInfo::set_latest_confidential_contents_info(
    ConfidentialContentsInfo confidential_contents_info) {
  latest_confidential_contents_info_ = confidential_contents_info;
}

void DlpContentManager::ScreenShareInfo::StartIfPending() {
  if (pending_start_on_source_change_) {
    state_change_callback_.Run(media_id_,
                               blink::mojom::MediaStreamStateChange::PLAY);
    pending_start_on_source_change_ = false;
  }
}

const RestrictionLevelAndUrl&
DlpContentManager::ScreenShareInfo::GetLatestRestriction() const {
  return latest_confidential_contents_info_.restriction_info;
}

const DlpConfidentialContents&
DlpContentManager::ScreenShareInfo::GetConfidentialContents() const {
  return latest_confidential_contents_info_.confidential_contents;
}

void DlpContentManager::ScreenShareInfo::Pause() {
  DCHECK_EQ(state_, State::kRunning);
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PAUSE);
  state_ = State::kPaused;
}

void DlpContentManager::ScreenShareInfo::Resume() {
  DCHECK_EQ(state_, State::kPaused);
  // In case of a tab share try to update the source to the current WebContents
  // frame id in case it was navigated to a different page with another frame.
  // Switching to a new source will resume the share so we don't need to do it
  // here explicitly.
  if (media_id_.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      web_contents_ && source_callback_) {
    content::RenderFrameHost* main_frame = web_contents_->GetPrimaryMainFrame();
    DCHECK(main_frame);
    source_callback_.Run(
        content::DesktopMediaID(
            content::DesktopMediaID::TYPE_WEB_CONTENTS,
            content::DesktopMediaID::kNullId,
            content::WebContentsMediaCaptureId(
                main_frame->GetProcess()->GetID(), main_frame->GetRoutingID())),
        captured_surface_control_active_);
    // Start after source will be changed and notified.
    pending_start_on_source_change_ = true;
  } else {
    state_change_callback_.Run(media_id_,
                               blink::mojom::MediaStreamStateChange::PLAY);
  }
  state_ = State::kRunning;
}

void DlpContentManager::ScreenShareInfo::ChangeStateBeforeSourceChange() {
  DCHECK(state_ == State::kPaused || state_ == State::kRunning);
  DCHECK(media_id_.type == content::DesktopMediaID::TYPE_WEB_CONTENTS);
  if (state_ == State::kPaused) {
    state_ = ScreenShareInfo::State::kPausedBeforeSourceChange;
  } else if (state_ == State::kRunning) {
    state_ = State::kRunningBeforeSourceChange;
  } else {
    // This should only be called if state_ is Running or Paused.
    NOTREACHED_IN_MIGRATION();
  }
}

void DlpContentManager::ScreenShareInfo::Stop() {
  DCHECK_NE(state_, State::kStopped);
  if (stop_callback_) {
    std::move(stop_callback_).Run();
    state_ = State::kStopped;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void DlpContentManager::ScreenShareInfo::MaybeUpdateNotifications() {
  UpdatePausedNotification(/*show=*/state_ == State::kPaused);
  UpdateResumedNotification(/*show=*/state_ == State::kRunning);
}

void DlpContentManager::ScreenShareInfo::HideNotifications() {
  UpdatePausedNotification(/*show=*/false);
  UpdateResumedNotification(/*show=*/false);
}

void DlpContentManager::ScreenShareInfo::MaybeCloseDialogWidget() {
  if (dialog_widget_) {
    dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

bool DlpContentManager::ScreenShareInfo::HasOpenDialogWidget() {
  return dialog_widget_ && !dialog_widget_->IsClosed();
}

void DlpContentManager::ScreenShareInfo::SetCapturedSurfaceControlActive() {
  captured_surface_control_active_ = true;
}

base::WeakPtr<DlpContentManager::ScreenShareInfo>
DlpContentManager::ScreenShareInfo::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DlpContentManager::ScreenShareInfo::UpdatePausedNotification(bool show) {
  if ((notification_state_ == NotificationState::kShowingPausedNotification) ==
      show)
    return;
  if (show) {
    DCHECK_EQ(state_, State::kPaused);
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
    DCHECK_EQ(state_, State::kRunning);
    ShowDlpScreenShareResumedNotification(label_, application_title_);
    notification_state_ = NotificationState::kShowingResumedNotification;
  } else {
    HideDlpScreenShareResumedNotification(label_);
    notification_state_ = NotificationState::kNotShowingNotification;
  }
}

void DlpContentManager::AddObserver(DlpContentManagerObserver* observer,
                                    DlpContentRestriction restriction) {
  observer_lists_[static_cast<int>(restriction)].AddObserver(observer);
}

void DlpContentManager::OnScreenShareSourceChanging(
    const std::string& label,
    const content::DesktopMediaID& old_media_id,
    const content::DesktopMediaID& new_media_id,
    bool captured_surface_control_active) {
  for (auto& screen_share : running_screen_shares_) {
    if (screen_share->label() == label &&
        screen_share->media_id() == old_media_id) {
      if (captured_surface_control_active) {
        screen_share->SetCapturedSurfaceControlActive();
      }
      if (screen_share->new_media_id() != new_media_id) {
        screen_share->ChangeStateBeforeSourceChange();
        screen_share->set_new_media_id(new_media_id);
      }
    }
  }
}

void DlpContentManager::RemoveObserver(
    const DlpContentManagerObserver* observer,
    DlpContentRestriction restriction) {
  observer_lists_[static_cast<int>(restriction)].RemoveObserver(observer);
}

std::vector<DlpContentManager::WebContentsInfo>
DlpContentManager::GetWebContentsInfo() const {
  std::vector<WebContentsInfo> web_contents_info_vector;
  for (const auto& [web_contents, restriction_set] :
       confidential_web_contents_) {
    DlpContentManager::WebContentsInfo web_contents_info(web_contents,
                                                         restriction_set, {});
    auto* tab_helper = DlpContentTabHelper::FromWebContents(web_contents);
    if (tab_helper) {
      web_contents_info.rfh_info_vector = tab_helper->GetFramesInfo();
    }
    web_contents_info_vector.push_back(std::move(web_contents_info));
  }
  return web_contents_info_vector;
}

DlpContentManager::DlpContentManager() {
  // Start observing tab strip models for all browsers.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list)
    browser->tab_strip_model()->AddObserver(this);
  browser_list->AddObserver(this);
}

DlpContentManager::~DlpContentManager() {
  BrowserList* browser_list = BrowserList::GetInstance();
  browser_list->RemoveObserver(this);
  for (Browser* browser : *browser_list)
    browser->tab_strip_model()->RemoveObserver(this);
}

// static
void DlpContentManager::ReportWarningProceededEvent(
    const GURL& url,
    DlpRulesManager::Restriction restriction,
    data_controls::DlpReportingManager* reporting_manager) {
  if (!reporting_manager)
    return;

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (rules_manager) {
    DlpRulesManager::RuleMetadata rule_metadata;
    const std::string src_pattern = rules_manager->GetSourceUrlPattern(
        url, restriction, DlpRulesManager::Level::kWarn, &rule_metadata);
    const std::string src_url = url.is_empty() ? src_pattern : url.spec();
    reporting_manager->ReportWarningProceededEvent(
        src_url, restriction, rule_metadata.name, rule_metadata.obfuscated_id);
  }
}

// static
bool DlpContentManager::MaybeReportWarningProceededEvent(
    GURL url,
    DlpRulesManager::Restriction restriction,
    data_controls::DlpReportingManager* reporting_manager,
    bool should_proceed) {
  if (should_proceed) {
    ReportWarningProceededEvent(url, restriction, reporting_manager);
  }
  return should_proceed;
}

// static
content::WebContents* DlpContentManager::GetWebContentsFromMediaId(
    const content::DesktopMediaID& media_id) {
  if (media_id.type != content::DesktopMediaID::Type::TYPE_WEB_CONTENTS)
    return nullptr;
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(
          media_id.web_contents_id.render_process_id,
          media_id.web_contents_id.main_render_frame_id));
}

void DlpContentManager::Init() {
  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (rules_manager)
    reporting_manager_ =
        DlpRulesManagerFactory::GetForPrimaryProfile()->GetReportingManager();
  warn_notifier_ = std::make_unique<DlpWarnNotifier>();
}

void DlpContentManager::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  DlpContentRestrictionSet old_restriction_set;
  if (confidential_web_contents_.contains(web_contents)) {
    old_restriction_set = confidential_web_contents_[web_contents];
  }
  UpdateConfidentiality(web_contents, restriction_set);
  NotifyOnConfidentialityChanged(old_restriction_set, restriction_set,
                                 web_contents);
}

void DlpContentManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  RemoveFromConfidential(web_contents);
}

void DlpContentManager::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void DlpContentManager::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}

void DlpContentManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Checking only after selecting the possible moved tab as in this case it
  // already was added to the new window.
  if (change.type() == TabStripModelChange::kSelectionOnly) {
    TabLocationMaybeChanged(selection.new_contents);
  }
}

void DlpContentManager::RemoveFromConfidential(
    content::WebContents* web_contents) {
  confidential_web_contents_.erase(web_contents);
}

RestrictionLevelAndUrl DlpContentManager::GetPrintingRestrictionInfo(
    content::WebContents* web_contents,
    content::GlobalRenderFrameHostId rfh_id) const {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedded
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHostId(rfh_id);
  web_contents =
      guest_view ? guest_view->embedder_web_contents() : web_contents;

  return GetConfidentialRestrictions(web_contents)
      .GetRestrictionLevelAndUrl(DlpContentRestriction::kPrint);
}

DlpContentManager::ConfidentialContentsInfo
DlpContentManager::GetScreenShareConfidentialContentsInfoForWebContents(
    content::WebContents* web_contents) const {
  ConfidentialContentsInfo info;
  if (web_contents && !web_contents->IsBeingDestroyed()) {
    info.restriction_info =
        GetConfidentialRestrictions(web_contents)
            .GetRestrictionLevelAndUrl(DlpContentRestriction::kScreenShare);
    if (info.restriction_info.level != DlpRulesManager::Level::kNotSet)
      info.confidential_contents.Add(web_contents);
  }
  return info;
}

void DlpContentManager::ProcessScreenShareRestriction(
    const std::u16string& application_title,
    ConfidentialContentsInfo info,
    WarningCallback callback) {
  data_controls::DlpBooleanHistogram(data_controls::dlp::kScreenShareBlockedUMA,
                                     IsBlocked(info.restriction_info));
  data_controls::DlpBooleanHistogram(data_controls::dlp::kScreenShareWarnedUMA,
                                     IsWarn(info.restriction_info));
  if (IsBlocked(info.restriction_info)) {
    MaybeReportEvent(info.restriction_info,
                     DlpRulesManager::Restriction::kScreenShare);
    ShowDlpScreenShareDisabledNotification(application_title);
    std::move(callback).Run(false);
    return;
  }
  if (IsWarn(info.restriction_info)) {
    // Check which of the contents were already allowed and don't warn for
    // those.
    RemoveAllowedContents(info.confidential_contents,
                          DlpRulesManager::Restriction::kScreenShare);
    if (info.confidential_contents.IsEmpty()) {
      // The user already allowed all the visible content.
      data_controls::DlpBooleanHistogram(
          data_controls::dlp::kScreenShareWarnSilentProceededUMA, true);
      std::move(callback).Run(true);
      return;
    }

    ReportWarningEvent(info.restriction_info.url,
                       DlpRulesManager::Restriction::kScreenShare);

    // base::Unretained(this) is safe here because DlpContentManager is
    // initialized as a singleton that's always available in the system.
    //
    // Don't report warning proceeded events here. They are reported in
    // DlpContentManager::CheckRunningScreenShares(), which is called when
    // screen share starts by DlpContentManager::OnScreenShareStarted().
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

void DlpContentManager::AddOrUpdateScreenShare(
    const std::string& label,
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    base::RepeatingClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  auto screen_share_it = base::ranges::find_if(
      running_screen_shares_,
      [&label, media_id](const std::unique_ptr<ScreenShareInfo>& info) {
        return info && info->label() == label &&
               info->new_media_id() == media_id;
      });
  if (screen_share_it != running_screen_shares_.end()) {
    // This should only happen for tab shares, and only if there was a source
    // change.
    DCHECK(media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS);
    ScreenShareInfo* screen_share = screen_share_it->get();
    DCHECK(screen_share->state() ==
               ScreenShareInfo::State::kPausedBeforeSourceChange ||
           screen_share->state() ==
               ScreenShareInfo::State::kRunningBeforeSourceChange);
    if (screen_share->state() ==
        ScreenShareInfo::State::kPausedBeforeSourceChange) {
      data_controls::DlpBooleanHistogram(
          data_controls::dlp::kScreenSharePausedOrResumedUMA, false);
    }
    screen_share->UpdateAfterSourceChange(
        media_id, application_title, std::move(stop_callback),
        state_change_callback, source_callback);
  } else {
    auto screen_share_info = std::make_unique<ScreenShareInfo>(
        label, media_id, application_title, std::move(stop_callback),
        state_change_callback, source_callback);
    running_screen_shares_.push_back(std::move(screen_share_info));
  }
}

void DlpContentManager::RemoveScreenShare(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  std::erase_if(
      running_screen_shares_,
      [=](const std::unique_ptr<ScreenShareInfo>& screen_share_info) -> bool {
        return screen_share_info->label() == label &&
               screen_share_info->media_id() == media_id &&
               screen_share_info->state() !=
                   ScreenShareInfo::State::kRunningBeforeSourceChange &&
               screen_share_info->state() !=
                   ScreenShareInfo::State::kPausedBeforeSourceChange;
      });
}

void DlpContentManager::CheckRunningScreenShares() {
  for (auto& screen_share : running_screen_shares_) {
    ConfidentialContentsInfo info = GetScreenShareConfidentialContentsInfo(
        screen_share->media_id(), screen_share->web_contents().get());
    if (IsReported(info.restriction_info) && reporting_manager_ &&
        last_reported_screen_share_.ShouldReportAndUpdate(
            screen_share->label(), info.confidential_contents)) {
      ReportEvent(info.restriction_info.url,
                  DlpRulesManager::Restriction::kScreenShare,
                  info.restriction_info.level, reporting_manager_);
    }

    // TODO(crbug.com/1326541): Fix for new tab shares.
    if (screen_share->GetLatestRestriction() == info.restriction_info &&
        screen_share->GetConfidentialContents() == info.confidential_contents) {
      // No change in restrictions that apply to this screen share.
      // Additional information, such as the titles, might have changed so we
      // check if need to update the warning.
      if (screen_share->HasOpenDialogWidget()) {
        if (EqualWithTitles(screen_share->GetConfidentialContents(),
                            info.confidential_contents))
          continue;

        screen_share->set_latest_confidential_contents_info(info);
        screen_share->MaybeCloseDialogWidget();
        RemoveAllowedContents(info.confidential_contents,
                              DlpRulesManager::Restriction::kScreenShare);
        // base::Unretained(this) is safe here because DlpContentManager is
        // initialized as a singleton that's always available in the system.
        screen_share->set_dialog_widget(
            warn_notifier_->ShowDlpScreenShareWarningDialog(
                base::BindOnce(
                    &DlpContentManager::OnDlpScreenShareWarnDialogReply,
                    base::Unretained(this), info, screen_share->GetWeakPtr()),
                info.confidential_contents, screen_share->application_title()));
      }
      continue;
    }

    screen_share->set_latest_confidential_contents_info(info);

    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kScreenShareBlockedUMA,
        IsBlocked(info.restriction_info));
    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kScreenShareWarnedUMA,
        IsWarn(info.restriction_info));
    if (IsBlocked(info.restriction_info)) {
      if (screen_share->state() == ScreenShareInfo::State::kRunning) {
        screen_share->Pause();
        data_controls::DlpBooleanHistogram(
            data_controls::dlp::kScreenSharePausedOrResumedUMA, true);
        screen_share->MaybeUpdateNotifications();
      }
      continue;
    }

    if (IsWarn(info.restriction_info)) {
      // Close previously opened dialog, if any.
      screen_share->MaybeCloseDialogWidget();
      // Check which of the contents were already allowed and don't warn for
      // those.
      RemoveAllowedContents(info.confidential_contents,
                            DlpRulesManager::Restriction::kScreenShare);
      if (info.confidential_contents.IsEmpty()) {
        // The user already allowed all the visible content.
        if (reporting_manager_ &&
            last_reported_screen_share_.ShouldReportAndUpdate(
                screen_share->label(), info.confidential_contents)) {
          ReportWarningProceededEvent(
              info.restriction_info.url,
              DlpRulesManager::Restriction::kScreenShare, reporting_manager_);
        }
        if (screen_share->state() == ScreenShareInfo::State::kPaused) {
          screen_share->Resume();
          screen_share->MaybeUpdateNotifications();
        }
        data_controls::DlpBooleanHistogram(
            data_controls::dlp::kScreenShareWarnSilentProceededUMA, true);
        continue;
      }
      if (screen_share->state() == ScreenShareInfo::State::kRunning) {
        screen_share->Pause();
        screen_share->HideNotifications();
      }

      ReportWarningEvent(info.restriction_info.url,
                         DlpRulesManager::Restriction::kScreenShare);

      // base::Unretained(this) is safe here because DlpContentManager is
      // initialized as a singleton that's always available in the system.
      screen_share->set_dialog_widget(
          warn_notifier_->ShowDlpScreenShareWarningDialog(
              base::BindOnce(
                  &DlpContentManager::OnDlpScreenShareWarnDialogReply,
                  base::Unretained(this), info, screen_share->GetWeakPtr()),
              info.confidential_contents, screen_share->application_title()));
      continue;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DlpContentManager::MaybeResumeScreenShare,
                       base::Unretained(this), screen_share->GetWeakPtr()),
        kScreenShareResumeDelay);
  }
}

void DlpContentManager::MaybeResumeScreenShare(
    base::WeakPtr<ScreenShareInfo> screen_share) {
  if (!screen_share ||
      screen_share->state() != ScreenShareInfo::State::kPaused) {
    return;
  }

  ConfidentialContentsInfo info = GetScreenShareConfidentialContentsInfo(
      screen_share->media_id(), screen_share->web_contents().get());
  if (IsBlocked(info.restriction_info) || IsWarn(info.restriction_info)) {
    return;
  }
  screen_share->Resume();
  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kScreenSharePausedOrResumedUMA, false);
  screen_share->MaybeUpdateNotifications();
}

void DlpContentManager::OnDlpScreenShareWarnDialogReply(
    const ConfidentialContentsInfo& info,
    base::WeakPtr<ScreenShareInfo> screen_share,
    bool should_proceed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!screen_share)
    // The screen share was stopped before the dialog was addressed, so no need
    // to do anything.
    return;

  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kScreenShareWarnProceededUMA, should_proceed);
  if (should_proceed) {
    if (reporting_manager_ &&
        last_reported_screen_share_.ShouldReportAndUpdate(
            screen_share->label(), info.confidential_contents))
      ReportWarningProceededEvent(info.restriction_info.url,
                                  DlpRulesManager::Restriction::kScreenShare,
                                  reporting_manager_);

    screen_share->Resume();
    for (const auto& content : info.confidential_contents.GetContents()) {
      user_allowed_contents_cache_.Cache(
          content, DlpRulesManager::Restriction::kScreenShare);
    }
    screen_share->MaybeUpdateNotifications();
  } else {
    screen_share->Stop();
    RemoveScreenShare(screen_share->label(), screen_share->media_id());
  }
}

// TODO(1293512): Consider moving reporting of warning proceeded events inside
// OnDlpWarnDialogReply().
void DlpContentManager::OnDlpWarnDialogReply(
    const DlpConfidentialContents& confidential_contents,
    DlpRulesManager::Restriction restriction,
    WarningCallback callback,
    bool should_proceed) {
  auto suffix = RestrictionToWarnProceededUMASuffix(restriction);
  if (suffix.has_value())
    data_controls::DlpBooleanHistogram(suffix.value(), should_proceed);
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
    const GURL& url,
    DlpRulesManager::Restriction restriction) {
  if (reporting_manager_) {
    ReportEvent(url, restriction, DlpRulesManager::Level::kWarn,
                reporting_manager_);
  }
}

void DlpContentManager::RemoveAllowedContents(
    DlpConfidentialContents& contents,
    DlpRulesManager::Restriction restriction) {
  base::EraseIf(
      contents.GetContents(), [=, this](const DlpConfidentialContent& content) {
        return user_allowed_contents_cache_.Contains(content, restriction);
      });
}

void DlpContentManager::UpdateConfidentiality(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  if (restriction_set.IsEmpty()) {
    RemoveFromConfidential(web_contents);
  } else {
    confidential_web_contents_[web_contents] = restriction_set;
  }
}

void DlpContentManager::NotifyOnConfidentialityChanged(
    const DlpContentRestrictionSet& old_restriction_set,
    const DlpContentRestrictionSet& new_restriction_set,
    content::WebContents* web_contents) {
  for (int i = 0; i <= static_cast<int>(DlpContentRestriction::kMaxValue);
       ++i) {
    auto restriction = static_cast<DlpContentRestriction>(i);
    auto old_level = old_restriction_set.GetRestrictionLevel(restriction);
    auto new_level = new_restriction_set.GetRestrictionLevel(restriction);
    if (old_level == new_level) {
      // If there is no change in this restriction, do not notify its
      // observers.
      continue;
    }
    auto& observer_list = observer_lists_[static_cast<int>(restriction)];
    for (DlpContentManagerObserver& observer : observer_list) {
      observer.OnConfidentialityChanged(old_level, new_level, web_contents);
    }
  }
}

bool DlpContentManager::LastReportedScreenShare::ShouldReportAndUpdate(
    const std::string& label,
    const DlpConfidentialContents& confidential_contents) {
  // Ignore reporting for empty labels. A media streams with an empty label is
  // most likely is an audio stream.
  if (label.empty())
    return false;

  if (label != label_) {
    label_ = label;
    confidential_contents_ = confidential_contents;
    return true;
  }
  // TODO(1306306): Consider reporting all visible confidential urls for
  //  onscreen restrictions.
  if (!std::includes(confidential_contents_.GetContents().begin(),
                     confidential_contents_.GetContents().end(),
                     confidential_contents.GetContents().begin(),
                     confidential_contents.GetContents().end())) {
    confidential_contents_.InsertOrUpdate(confidential_contents);
    return true;
  }
  return false;
}

}  // namespace policy
