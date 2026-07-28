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
class RemoteActorCredentialStore;

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
  void SharePassword(const ShareParameters& params,
                     SharePasswordCallback callback) override;

 private:
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_IMPL_H_
