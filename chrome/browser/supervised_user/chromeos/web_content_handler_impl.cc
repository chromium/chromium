// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/web_content_handler_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
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
      NOTREACHED();
      return;
  }
}

}  // namespace

WebContentHandlerImpl::WebContentHandlerImpl(content::WebContents& web_contents)
    : web_contents_(web_contents) {}

WebContentHandlerImpl::~WebContentHandlerImpl() = default;

void WebContentHandlerImpl::RequestLocalApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const gfx::ImageSkia& favicon,
    ApprovalRequestInitiatedCallback callback) {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetProfileKey());

  crosapi::mojom::ParentAccess* parent_access =
      crosapi::CrosapiManager::Get()->crosapi_ash()->parent_access_ash();
  DCHECK(parent_access);

  parent_access->GetWebsiteParentApproval(
      url.GetWithEmptyPath(), child_display_name, favicon,
      base::BindOnce(&WebContentHandlerImpl::OnLocalApprovalRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::ref(*settings_service), url, base::TimeTicks::Now()));
  std::move(callback).Run(true);
}

void WebContentHandlerImpl::OnLocalApprovalRequestCompleted(
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
