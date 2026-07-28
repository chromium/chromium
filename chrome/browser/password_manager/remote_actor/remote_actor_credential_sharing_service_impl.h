// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {
class IdentityManager;
}

namespace password_manager {

class RemoteActorCredentialPermissionClient;
class RemoteActorCredentialStoreClient;

class RemoteActorCredentialSharingServiceImpl
    : public RemoteActorCredentialSharingService {
 public:
  RemoteActorCredentialSharingServiceImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  RemoteActorCredentialSharingServiceImpl(
      const RemoteActorCredentialSharingServiceImpl&) = delete;
  RemoteActorCredentialSharingServiceImpl& operator=(
      const RemoteActorCredentialSharingServiceImpl&) = delete;
  ~RemoteActorCredentialSharingServiceImpl() override;

  // RemoteActorCredentialSharingService:
  // Coordinates the sequential password sharing flow:
  // 1. Uploads the credential to the Passbox store service (setting the TTL).
  // 2. If the upload succeeds, calls the Agentic Permission Service (APS) to
  //    grant the agent access to the credential.
  // 3. If APS grant succeeds, the callback is invoked with `true`.
  //
  // In case of failures:
  // - If the Passbox upload fails, the flow aborts immediately and the callback
  //   is run with `false`.
  // - If the Passbox upload succeeds but the subsequent APS grant fails, the
  //   callback is run with `false`. We do not attempt to clean up or delete the
  //   credential from Passbox in this case; instead, we rely on the credential's
  //   Time-To-Live (TTL) to automatically expire and delete it from Passbox.
  void SharePassword(const ShareParameters& params,
                     SharePasswordCallback callback) override;

 private:
  void OnPassboxCompleted(const ShareParameters& params,
                          SharePasswordCallback callback,
                          bool success);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<RemoteActorCredentialStoreClient> credential_store_;
  std::unique_ptr<RemoteActorCredentialPermissionClient> permission_client_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_IMPL_H_
