// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/web_content_handler_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/android/website_parent_approval.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

supervised_user::WebContentHandler::LocalApprovalResult
AndroidOutcomeToLocalApprovalResult(
    AndroidLocalWebApprovalFlowOutcome outcome) {
  switch (outcome) {
    case AndroidLocalWebApprovalFlowOutcome::kApproved:
      return supervised_user::WebContentHandler::LocalApprovalResult::kApproved;
    case AndroidLocalWebApprovalFlowOutcome::kRejected:
      return supervised_user::WebContentHandler::LocalApprovalResult::kDeclined;
    case AndroidLocalWebApprovalFlowOutcome::kIncomplete:
      return supervised_user::WebContentHandler::LocalApprovalResult::kCanceled;
  }
}

}  // namespace

WebContentHandlerImpl::WebContentHandlerImpl(content::WebContents* web_contents)
    : supervised_user::WebContentHandler(), web_contents_(web_contents) {
  CHECK(web_contents_);
}

WebContentHandlerImpl::~WebContentHandlerImpl() = default;

void WebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    ApprovalRequestInitiatedCallback callback) {
  CHECK(web_contents_);
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetProfileKey());

  WebsiteParentApproval::RequestLocalApproval(
      web_contents_.get(), supervised_user::NormalizeUrl(url),
      base::BindOnce(&WebContentHandlerImpl::OnLocalApprovalRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::ref(*settings_service), url, base::TimeTicks::Now()));
  // Runs the `callback` to inform the caller that the flow initiation was
  // successful.
  std::move(callback).Run(true);
}

bool WebContentHandlerImpl::IsMainFrame(int frame_id) {
  return web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId() == frame_id;
}

void WebContentHandlerImpl::CleanUpInfoBarOnMainFrame(int frame_id) {
  if (IsMainFrame(frame_id)) {
    supervised_user::CleanUpInfoBarForContent(web_contents_.get());
  }
}

void WebContentHandlerImpl::ShowFeedback(GURL url, std::u16string reason) {
  std::string message = l10n_util::GetStringFUTF8(
      IDS_BLOCK_INTERSTITIAL_DEFAULT_FEEDBACK_TEXT, reason);
  ReportChildAccountFeedback(web_contents_.get(), message, url);
}

void WebContentHandlerImpl::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    AndroidLocalWebApprovalFlowOutcome request_outcome) {
  WebContentHandler::OnLocalApprovalRequestCompleted(
      settings_service, url, start_time,
      AndroidOutcomeToLocalApprovalResult(request_outcome));
}
