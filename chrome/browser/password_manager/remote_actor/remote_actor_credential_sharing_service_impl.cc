// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_impl.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace password_manager {

RemoteActorCredentialSharingServiceImpl::
    RemoteActorCredentialSharingServiceImpl(
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

RemoteActorCredentialSharingServiceImpl::
    ~RemoteActorCredentialSharingServiceImpl() = default;

void RemoteActorCredentialSharingServiceImpl::SharePassword(
    const ShareParameters& params,
    SharePasswordCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace password_manager
