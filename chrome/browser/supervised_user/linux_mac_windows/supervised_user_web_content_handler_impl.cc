// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_web_content_handler_impl.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_dialog_result_observer.h"
#include "chrome/browser/supervised_user/linux_mac_windows/parent_access_view.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

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
    const supervised_user::FilteringBehaviorReason& filtering_reason,
    ApprovalRequestInitiatedCallback callback) {
  CHECK(base::FeatureList::IsEnabled(supervised_user::kLocalWebApprovals));
  CHECK(web_contents_);

  GURL target_url = url_formatter.FormatUrl(url);
  base::TimeTicks start_time = base::TimeTicks::Now();
  // Creates web contents observer for the parent approval dialog.
  auto create_observer_callback = base::BindOnce(
      &SupervisedUserWebContentHandlerImpl::CreateObserverFromContents,
      weak_ptr_factory_.GetWeakPtr(), start_time, target_url);
  auto abort_dialog_callback = base::BindOnce(
      &SupervisedUserWebContentHandlerImpl::AbortUrlApprovalDialog,
      weak_ptr_factory_.GetWeakPtr());

  auto dialog_result_observer_reset_callback = base::BindOnce(
      &SupervisedUserWebContentHandlerImpl::ResetDialogResultContentObserver,
      weak_ptr_factory_.GetWeakPtr());

  weak_parent_access_view_ = ParentAccessView::ShowParentAccessDialog(
      web_contents_, target_url, filtering_reason,
      std::move(create_observer_callback), std::move(abort_dialog_callback),
      std::move(dialog_result_observer_reset_callback));

  // Runs the `callback` to inform the caller that the flow initiation was
  // successful.
  std::move(callback).Run(true);
}

void SupervisedUserWebContentHandlerImpl::MaybeCloseLocalApproval() {
  // There is no local web approval instance open, do nothing.
  if (!dialog_web_contents_observer_) {
    return;
  }
  CloseDialog();
}

void SupervisedUserWebContentHandlerImpl::CreateObserverFromContents(
    base::TimeTicks start_time,
    const GURL& target_url,
    content::WebContents* contents) {
  CHECK(contents);
  // The parent approval dialog and its new contents have been created. We start
  // observing them.
  dialog_web_contents_observer_ =
      std::make_unique<ParentAccessDialogResultObserver>(
          contents,
          /*url_approval_result_callback=*/
          base::BindOnce(&SupervisedUserWebContentHandlerImpl::
                             CompleteUrlApprovalAndCloseDialog,
                         weak_ptr_factory_.GetWeakPtr(), target_url,
                         start_time));
}

void SupervisedUserWebContentHandlerImpl::CompleteUrlApprovalAndCloseDialog(
    const GURL& target_url,
    base::TimeTicks start_time,
    supervised_user::LocalApprovalResult result) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  CHECK(settings_service);

  supervised_user::WebContentHandler::OnLocalApprovalRequestCompleted(
      *settings_service, target_url, start_time, result);

  CloseDialog();
}

void SupervisedUserWebContentHandlerImpl::CloseDialog() {
  // The `weak_parent_access_view_` might have been invalidated through an
  // accelerator.
  if (weak_parent_access_view_) {
    weak_parent_access_view_->CloseView();
  }
}

void SupervisedUserWebContentHandlerImpl::AbortUrlApprovalDialog() {
  if (!dialog_web_contents_observer_) {
    return;
  }
  // Sets the approval result to error and destructs the result observer.
  // The destructor records the metrics of the approval outcome (Error).
  dialog_web_contents_observer_->SetResultToError();
  ResetDialogResultContentObserver();
}

void SupervisedUserWebContentHandlerImpl::ResetDialogResultContentObserver() {
  if (!dialog_web_contents_observer_) {
    return;
  }
  dialog_web_contents_observer_->StopObserving();
  dialog_web_contents_observer_.reset();
}
