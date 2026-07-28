// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_PERMISSION_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_PERMISSION_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

class RemoteActorRequest;

// Helper class to communicate with Agentic Permission Service (APS)
// for password sharing.
class RemoteActorCredentialPermissionClient {
 public:
  // Struct containing the permission details. Populated by the coordinator
  // (RemoteActorCredentialSharingServiceImpl).
  struct PasswordPermission {
    std::string agent_oauth_client_id;
    std::string web_origin;
    // TODO(crbug.com/532483845): Use this to grant permission for a specific
    // credential instead of all affiliated passwords.
    std::string password_client_tag_hash;
  };

  using GrantPasswordPermissionCallback = base::OnceCallback<void(bool)>;

  RemoteActorCredentialPermissionClient(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  RemoteActorCredentialPermissionClient(
      const RemoteActorCredentialPermissionClient&) = delete;
  RemoteActorCredentialPermissionClient& operator=(
      const RemoteActorCredentialPermissionClient&) = delete;
  ~RemoteActorCredentialPermissionClient();

  // Grants permission to `agent_oauth_client_id` to access the credential
  // identified by `password_client_tag_hash` for `web_origin`.
  void GrantPasswordPermission(const PasswordPermission& permission,
                               GrantPasswordPermissionCallback callback);

  void StartRequest(std::unique_ptr<RemoteActorRequest> request);
  void OnRequestCompleted(base::OnceCallback<void(bool)> callback,
                          RemoteActorRequest* request,
                          bool success);
  void DeleteRequest(RemoteActorRequest* request);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<std::unique_ptr<RemoteActorRequest>> pending_requests_;

  base::WeakPtrFactory<RemoteActorCredentialPermissionClient> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_PERMISSION_CLIENT_H_
