// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_notifier.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "url/gurl.h"

namespace policy {

namespace {
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

}  // namespace

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

void DlpContentManager::SetIsScreenShareWarningModeEnabledForTesting(
    bool is_enabled) {
  is_screen_share_warning_mode_enabled_ = is_enabled;
}

DlpContentManager::DlpContentManager() = default;
DlpContentManager::~DlpContentManager() = default;

void DlpContentManager::ReportWarningProceededEvent(
    const GURL& url,
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

bool DlpContentManager::MaybeReportWarningProceededEvent(
    GURL url,
    DlpRulesManager::Restriction restriction,
    DlpReportingManager* reporting_manager,
    bool should_proceed) {
  if (should_proceed) {
    ReportWarningProceededEvent(url, restriction, reporting_manager);
  }
  return should_proceed;
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
  if (restriction_set.IsEmpty()) {
    RemoveFromConfidential(web_contents);
  } else {
    confidential_web_contents_[web_contents] = restriction_set;
  }
}

void DlpContentManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  RemoveFromConfidential(web_contents);
}

void DlpContentManager::RemoveFromConfidential(
    content::WebContents* web_contents) {
  confidential_web_contents_.erase(web_contents);
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

DlpContentManager::ConfidentialContentsInfo
DlpContentManager::GetScreenShareConfidentialContentsInfoForWebContents(
    const content::WebContentsMediaCaptureId& web_contents_id) const {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              web_contents_id.render_process_id,
              web_contents_id.main_render_frame_id));
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

void DlpContentManager::ProcessScreenShareRestriction(
    const std::u16string& application_title,
    ConfidentialContentsInfo info,
    OnDlpRestrictionCheckedCallback callback) {
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
      contents.GetContents(), [=](const DlpConfidentialContent& content) {
        return user_allowed_contents_cache_.Contains(content, restriction);
      });
}

}  // namespace policy
