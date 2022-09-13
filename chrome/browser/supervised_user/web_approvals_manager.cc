// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/web_approvals_manager.h"
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/android/website_parent_approval.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#endif

namespace {

void CreateURLAccessRequest(
    const GURL& url,
    PermissionRequestCreator* creator,
    WebApprovalsManager::ApprovalRequestInitiatedCallback callback) {
  creator->CreateURLAccessRequest(url, std::move(callback));
}

}  // namespace

WebApprovalsManager::WebApprovalsManager() = default;

WebApprovalsManager::~WebApprovalsManager() = default;

void WebApprovalsManager::RequestLocalApproval(
    content::WebContents* web_contents,
    const GURL& url,
    ApprovalRequestInitiatedCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1233615): pass completion_callback.
  // TODO(b/195316776): Pass in relevant local web approvals params instead of
  // an empty object.
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New()));
  chromeos::ParentAccessDialog::ShowError result =
      chromeos::ParentAccessDialog::Show(std::move(params));

  if (result != chromeos::ParentAccessDialog::ShowError::kNone) {
    LOG(ERROR) << "Error showing ParentAccessDialog: " << result;
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
#elif BUILDFLAG(IS_ANDROID)
  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetProfileKey());
  WebsiteParentApproval::RequestLocalApproval(
      web_contents, NormalizeUrl(url),
      base::BindOnce(&WebApprovalsManager::OnLocalApprovalRequestCompleted,
                     weak_ptr_factory_.GetWeakPtr(), settings_service, url));
  std::move(callback).Run(true);
#endif
}

void WebApprovalsManager::RequestRemoteApproval(
    const GURL& url,
    ApprovalRequestInitiatedCallback callback) {
  AddRemoteApprovalRequestInternal(
      base::BindRepeating(CreateURLAccessRequest, NormalizeUrl(url)),
      std::move(callback), 0);
}

bool WebApprovalsManager::AreRemoteApprovalRequestsEnabled() const {
  return FindEnabledRemoteApprovalRequestCreator(0) <
         remote_approval_request_creators_.size();
}

void WebApprovalsManager::AddRemoteApprovalRequestCreator(
    std::unique_ptr<PermissionRequestCreator> creator) {
  remote_approval_request_creators_.push_back(std::move(creator));
}

void WebApprovalsManager::ClearRemoteApprovalRequestsCreators() {
  remote_approval_request_creators_.clear();
}

size_t WebApprovalsManager::FindEnabledRemoteApprovalRequestCreator(
    size_t start) const {
  for (size_t i = start; i < remote_approval_request_creators_.size(); ++i) {
    if (remote_approval_request_creators_[i]->IsEnabled())
      return i;
  }
  return remote_approval_request_creators_.size();
}

GURL WebApprovalsManager::NormalizeUrl(const GURL& url) {
  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;
  return url_matcher::util::Normalize(effective_url);
}

void WebApprovalsManager::AddRemoteApprovalRequestInternal(
    const CreateRemoteApprovalRequestCallback& create_request,
    ApprovalRequestInitiatedCallback callback,
    size_t index) {
  size_t next_index = FindEnabledRemoteApprovalRequestCreator(index);
  if (next_index >= remote_approval_request_creators_.size()) {
    std::move(callback).Run(false);
    return;
  }

  create_request.Run(
      remote_approval_request_creators_[next_index].get(),
      base::BindOnce(&WebApprovalsManager::OnRemoteApprovalRequestIssued,
                     weak_ptr_factory_.GetWeakPtr(), create_request,
                     std::move(callback), next_index));
}

void WebApprovalsManager::OnRemoteApprovalRequestIssued(
    const CreateRemoteApprovalRequestCallback& create_request,
    ApprovalRequestInitiatedCallback callback,
    size_t index,
    bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }

  AddRemoteApprovalRequestInternal(create_request, std::move(callback),
                                   index + 1);
}

void WebApprovalsManager::OnLocalApprovalRequestCompleted(
    SupervisedUserSettingsService* settings_service,
    const GURL& url,
    bool request_approved) {
  // TODO(crbug.com/1324945): output metrics.
  VLOG(0) << "Local URL approval final result: " << request_approved;

  if (request_approved) {
    settings_service->RecordLocalWebsiteApproval(url.host());
  }
}
