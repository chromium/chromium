// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_STORE_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_STORE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/protocol/password_specifics.pb.h"

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

class RemoteActorRequest;

// Helper client used by `RemoteActorCredentialSharingServiceImpl` to
// communicate with the Passbox service (OnePlatform REST API) for managing
// remote actor credentials.
//
// Supported operations:
// - UpdateCredential: Uploads or updates a password credential payload in
//   Passbox with a specified Time-To-Live (TTL) for a web origin.
// - DeleteCredential: Deletes/revokes a stored credential from Passbox for a
//   given user, web origin, and credential tag hash.
//
// Authenticates all network requests via OAuth2 tokens fetched asynchronously
// from IdentityManager.
class RemoteActorCredentialStoreClient {
 public:
  using UpdateCallback = base::OnceCallback<void(bool)>;
  using DeleteCallback = base::OnceCallback<void(bool)>;

  RemoteActorCredentialStoreClient(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  RemoteActorCredentialStoreClient(const RemoteActorCredentialStoreClient&) =
      delete;
  RemoteActorCredentialStoreClient& operator=(
      const RemoteActorCredentialStoreClient&) = delete;
  ~RemoteActorCredentialStoreClient();

  // Uploads the credential to Passbox with a TTL.
  void UpdateCredential(const std::string& obfuscated_gaia_id,
                        const std::string& web_origin,
                        sync_pb::PasswordSpecificsData password_data,
                        base::TimeDelta ttl,
                        UpdateCallback callback);

  // Deletes the credential from Passbox.
  void DeleteCredential(const std::string& obfuscated_gaia_id,
                        const std::string& web_origin,
                        const std::string& password_client_tag_hash,
                        DeleteCallback callback);

 private:
  void StartRequest(std::unique_ptr<RemoteActorRequest> request);
  void OnRequestCompleted(base::OnceCallback<void(bool)> callback,
                          RemoteActorRequest* request,
                          bool success);
  void DeleteRequest(RemoteActorRequest* request);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::vector<std::unique_ptr<RemoteActorRequest>> pending_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RemoteActorCredentialStoreClient> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_STORE_CLIENT_H_
