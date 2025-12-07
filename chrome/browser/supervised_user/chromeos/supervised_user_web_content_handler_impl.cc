// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_web_content_handler_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

supervised_user::LocalApprovalResult ChromeOSResultToLocalApprovalResult(
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  switch (result->status) {
    case ash::ParentAccessDialog::Result::Status::kApproved:
      return supervised_user::LocalApprovalResult::kApproved;
    case ash::ParentAccessDialog::Result::Status::kDeclined:
      return supervised_user::LocalApprovalResult::kDeclined;
    case ash::ParentAccessDialog::Result::Status::kCanceled:
      return supervised_user::LocalApprovalResult::kCanceled;
    case ash::ParentAccessDialog::Result::Status::kError:
      return supervised_user::LocalApprovalResult::kError;
    case ash::ParentAccessDialog::Result::Status::kDisabled:
      // Disabled is not a possible result for Local Web Approvals.
      NOTREACHED();
  }
}

void HandleChromeOSShowError(
    ash::ParentAccessDialogProvider::ShowError show_error) {
  switch (show_error) {
    case ash::ParentAccessDialogProvider::ShowError::kNotAChildUser:
      // Fatal debug error because this can only occur due to a programming
      // error.
      DLOG(FATAL) << "ParentAccess UI invoked by non-child user";
      return;
    case ash::ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible:
      // Fatal debug error because this can only occur due to a programming
      // error.
      DLOG(FATAL) << "ParentAccess UI invoked while instance already visible";
      return;
    case ash::ParentAccessDialogProvider::ShowError::kNone:
      NOTREACHED();
  }
}
}  // namespace

SupervisedUserWebContentHandlerImpl::SupervisedUserWebContentHandlerImpl(
    content::WebContents* web_contents,
    const GURL& url,
    favicon::LargeIconService& large_icon_service,
    content::FrameTreeNodeId frame_id,
    int64_t interstitial_navigation_id)
    : ChromeSupervisedUserWebContentHandlerBase(web_contents,
                                                frame_id,
                                                interstitial_navigation_id),
      favicon_handler_(std::make_unique<SupervisedUserFaviconRequestHandler>(
          url.GetWithEmptyPath(),
          &large_icon_service)),
      profile_(*Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      dialog_provider_(std::make_unique<ash::ParentAccessDialogProvider>()) {
  CHECK(web_contents_);
  if (supervised_user::IsLocalWebApprovalsEnabled()) {
    // Prefetch the favicon which will be rendered as part of the web approvals
    // ParentAccessDialog. Pass in DoNothing() for the favicon fetched callback
    // because if the favicon is by the time the user triggers the opening of
    // the ParentAccessDialog, we show the default favicon.
    favicon_handler_->StartFaviconFetch(base::DoNothing());
  }
}

SupervisedUserWebContentHandlerImpl::~SupervisedUserWebContentHandlerImpl() =
    default;

void SupervisedUserWebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const supervised_user::UrlFormatter& url_formatter,
    const supervised_user::FilteringBehaviorReason& filtering_behavior_reason,
    ApprovalRequestInitiatedCallback callback) {
  CHECK(web_contents_);
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetProfileKey());

  // Encode the favicon as a PNG bitmap.
  std::optional<std::vector<uint8_t>> favicon_bitmap =
      gfx::PNGCodec::FastEncodeBGRASkBitmap(
          favicon_handler_->GetFaviconOrFallback(),
          /*discard_transparency=*/false);

  GURL target_url = url_formatter.FormatUrl(url);

  // Assemble the parameters for a website access request.
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New(
                  url_formatter.FormatUrl(url).GetWithEmptyPath(),
                  child_display_name,
                  favicon_bitmap.value_or(std::vector<uint8_t>()))),
          /* is_disabled= */ false);
  base::TimeTicks start_time = base::TimeTicks::Now();
  auto show_error = dialog_provider_->Show(
      std::move(params),
      base::BindOnce(
          &SupervisedUserWebContentHandlerImpl::OnLocalApprovalRequestCompleted,
          weak_ptr_factory_.GetWeakPtr(), std::ref(*settings_service),
          target_url, start_time));
  if (show_error == ash::ParentAccessDialogProvider::ShowError::kNone) {
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
    WebContentHandler::OnLocalApprovalRequestCompleted(
        *settings_service, target_url, start_time,
        supervised_user::LocalApprovalResult::kError,
        /*local_approval_error_type=*/std::nullopt);
    HandleChromeOSShowError(show_error);
  }
}

void SupervisedUserWebContentHandlerImpl::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  WebContentHandler::OnLocalApprovalRequestCompleted(
      settings_service, url, start_time,
      ChromeOSResultToLocalApprovalResult(std::move(result)),
      /*local_approval_error_type=*/std::nullopt);
}
