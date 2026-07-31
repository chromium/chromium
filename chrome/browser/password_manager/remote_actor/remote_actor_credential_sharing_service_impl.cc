// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_permission_client.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_store_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

RemoteActorCredentialSharingServiceImpl::
    RemoteActorCredentialSharingServiceImpl(
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      credential_store_(std::make_unique<RemoteActorCredentialStoreClient>(
          identity_manager,
          url_loader_factory)),
      permission_client_(
          std::make_unique<RemoteActorCredentialPermissionClient>(
              identity_manager,
              std::move(url_loader_factory))) {}

RemoteActorCredentialSharingServiceImpl::
    ~RemoteActorCredentialSharingServiceImpl() = default;

void RemoteActorCredentialSharingServiceImpl::SharePassword(
    const ShareParameters& params,
    SharePasswordCallback callback) {
  CHECK(!callback.is_null());

  if (params.agent_oauth_client_id.empty() || params.web_origin.empty() ||
      params.password_client_tag_hash.empty() ||
      params.obfuscated_gaia_id.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  credential_store_->UpdateCredential(
      params.obfuscated_gaia_id, params.web_origin,
      params.password_client_tag_hash, params.password_data,
      params.time_to_live,
      base::BindOnce(
          &RemoteActorCredentialSharingServiceImpl::OnPassboxCompleted,
          base::Unretained(this), params, std::move(callback)));
}

void RemoteActorCredentialSharingServiceImpl::OnPassboxCompleted(
    const ShareParameters& params,
    SharePasswordCallback callback,
    bool success) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.agent_oauth_client_id = params.agent_oauth_client_id;
  permission.web_origin = params.web_origin;
  permission.password_client_tag_hash = params.password_client_tag_hash;

  permission_client_->GrantPasswordPermission(permission, std::move(callback));
}

}  // namespace password_manager
