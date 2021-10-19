// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/web_approvals_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "components/policy/core/browser/url_util.h"
#include "url/gurl.h"

namespace {

void CreateURLAccessRequest(const GURL& url,
                            PermissionRequestCreator* creator,
                            WebApprovalsManager::RemoteRequestSent callback) {
  creator->CreateURLAccessRequest(url, std::move(callback));
}

}  // namespace

WebApprovalsManager::WebApprovalsManager() = default;

WebApprovalsManager::~WebApprovalsManager() = default;

void WebApprovalsManager::RequestRemoteApproval(const GURL& url,
                                                RemoteRequestSent callback) {
  GURL effective_url = policy::url_util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;
  AddPermissionRequestInternal(
      base::BindRepeating(CreateURLAccessRequest,
                          policy::url_util::Normalize(effective_url)),
      std::move(callback), 0);
}

bool WebApprovalsManager::AreRemoteApprovalRequestsEnabled() const {
  return FindEnabledApprovalRequestCreator(0) <
         remote_approval_request_creators_.size();
}

void WebApprovalsManager::AddRemoteApprovalRequestCreator(
    std::unique_ptr<PermissionRequestCreator> creator) {
  remote_approval_request_creators_.push_back(std::move(creator));
}

void WebApprovalsManager::ClearRemoteApprovalRequestsCreators() {
  remote_approval_request_creators_.clear();
}

size_t WebApprovalsManager::FindEnabledApprovalRequestCreator(
    size_t start) const {
  for (size_t i = start; i < remote_approval_request_creators_.size(); ++i) {
    if (remote_approval_request_creators_[i]->IsEnabled())
      return i;
  }
  return remote_approval_request_creators_.size();
}

void WebApprovalsManager::AddPermissionRequestInternal(
    const CreatePermissionRequestCallback& create_request,
    RemoteRequestSent callback,
    size_t index) {
  // Find a permission request creator that is enabled.
  size_t next_index = FindEnabledApprovalRequestCreator(index);
  if (next_index >= remote_approval_request_creators_.size()) {
    std::move(callback).Run(false);
    return;
  }

  create_request.Run(
      remote_approval_request_creators_[next_index].get(),
      base::BindOnce(&WebApprovalsManager::OnPermissionRequestIssued,
                     weak_ptr_factory_.GetWeakPtr(), create_request,
                     std::move(callback), next_index));
}

void WebApprovalsManager::OnPermissionRequestIssued(
    const CreatePermissionRequestCallback& create_request,
    RemoteRequestSent callback,
    size_t index,
    bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }

  AddPermissionRequestInternal(create_request, std::move(callback), index + 1);
}
