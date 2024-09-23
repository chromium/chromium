// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/supervised_user_web_content_handler_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/android/website_parent_approval.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/web_contents.h"

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

SupervisedUserWebContentHandlerImpl::SupervisedUserWebContentHandlerImpl(
    content::WebContents* web_contents,
    content::FrameTreeNodeId frame_id,
    int64_t interstitial_navigation_id)
    : ChromeSupervisedUserWebContentHandlerBase(web_contents,
                                                frame_id,
                                                interstitial_navigation_id) {}

SupervisedUserWebContentHandlerImpl::~SupervisedUserWebContentHandlerImpl() =
    default;

void SupervisedUserWebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const supervised_user::UrlFormatter& url_formatter,
    ApprovalRequestInitiatedCallback callback) {
  CHECK(web_contents_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  CHECK(profile);
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());

  GURL target_url = url_formatter.FormatUrl(url);

  WebsiteParentApproval::RequestLocalApproval(
      web_contents_, target_url,
      base::BindOnce(
          &SupervisedUserWebContentHandlerImpl::OnLocalApprovalRequestCompleted,
          weak_ptr_factory_.GetWeakPtr(), std::ref(*settings_service),
          target_url, base::TimeTicks::Now()),
      *profile);

  // Runs the `callback` to inform the caller that the flow initiation was
  // successful.
  std::move(callback).Run(true);
}

void SupervisedUserWebContentHandlerImpl::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    AndroidLocalWebApprovalFlowOutcome request_outcome) {
  WebContentHandler::OnLocalApprovalRequestCompleted(
      settings_service, url, start_time,
      AndroidOutcomeToLocalApprovalResult(request_outcome));
}
