// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_web_content_handler_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/chromeos/chromeos_utils.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/web_contents.h"

namespace {

supervised_user::WebContentHandler::LocalApprovalResult
ChromeOSResultToLocalApprovalResult(
    crosapi::mojom::ParentAccessResult::Tag result) {
  switch (result) {
    case crosapi::mojom::ParentAccessResult::Tag::kApproved:
      return supervised_user::WebContentHandler::LocalApprovalResult::kApproved;
    case crosapi::mojom::ParentAccessResult::Tag::kDeclined:
      return supervised_user::WebContentHandler::LocalApprovalResult::kDeclined;
    case crosapi::mojom::ParentAccessResult::Tag::kCanceled:
      return supervised_user::WebContentHandler::LocalApprovalResult::kCanceled;
    case crosapi::mojom::ParentAccessResult::Tag::kError:
      return supervised_user::WebContentHandler::LocalApprovalResult::kError;
    case crosapi::mojom::ParentAccessResult::Tag::kDisabled:
      // Disabled is not a possible result for Local Web Approvals.
      NOTREACHED();
  }
}

void HandleChromeOSErrorResult(
    crosapi::mojom::ParentAccessErrorResult::Type type) {
  switch (type) {
    case crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser:
      // Fatal debug error because this can only occur due to a programming
      // error.
      DLOG(FATAL) << "ParentAccess UI invoked by non-child user";
      return;
    case crosapi::mojom::ParentAccessErrorResult::Type::kAlreadyVisible:
      // Fatal debug error because this can only occur due to a programming
      // error.
      DLOG(FATAL) << "ParentAccess UI invoked while instance already visible";
      return;
    case crosapi::mojom::ParentAccessErrorResult::Type::kUnknown:
      LOG(ERROR) << "Unknown error in ParentAccess UI";
      return;
    case crosapi::mojom::ParentAccessErrorResult::Type::kNone:
      NOTREACHED_IN_MIGRATION();
      return;
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
      profile_(
          *Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
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
    ApprovalRequestInitiatedCallback callback) {
  CHECK(web_contents_);
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetProfileKey());

  crosapi::mojom::ParentAccess* parent_access =
      supervised_user::GetParentAccessApi();
  CHECK(parent_access);
  GURL target_url = url_formatter.FormatUrl(url);

  // TODO(b/322484529): Standardize the url formatting for local approvals
  // across platforms.
  parent_access->GetWebsiteParentApproval(
      target_url.GetWithEmptyPath(), child_display_name,
      // Receiver does not need multi-resolution image. Pass single-resolution
      // ImageSkia for compatibility with mojo interface.
      gfx::ImageSkia::CreateFrom1xBitmap(
          favicon_handler_->GetFaviconOrFallback()),
      base::BindOnce(
          &SupervisedUserWebContentHandlerImpl::OnLocalApprovalRequestCompleted,
          weak_ptr_factory_.GetWeakPtr(), std::ref(*settings_service),
          target_url, base::TimeTicks::Now()));
  std::move(callback).Run(true);
}

void SupervisedUserWebContentHandlerImpl::OnLocalApprovalRequestCompleted(
    supervised_user::SupervisedUserSettingsService& settings_service,
    const GURL& url,
    base::TimeTicks start_time,
    crosapi::mojom::ParentAccessResultPtr result) {
  WebContentHandler::OnLocalApprovalRequestCompleted(
      settings_service, url, start_time,
      ChromeOSResultToLocalApprovalResult(result->which()));

  if (result->is_error()) {
    HandleChromeOSErrorResult(result->get_error()->type);
  }
}
