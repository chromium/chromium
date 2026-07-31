// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/password_specifics.pb.h"

namespace password_manager {

// Interface for the service that coordinates password sharing with remote
// actors.
class RemoteActorCredentialSharingService : public KeyedService {
 public:
  struct ShareParameters {
    // The obfuscated GAIA ID of the remote actor with whom the credential is
    // shared.
    std::string obfuscated_gaia_id;

    // The string representation of the target site's origin (e.g.
    // "https://github.com") for which the credential is valid.
    std::string web_origin;

    // The client tag hash representing the metadata or key identifier for the
    // password.
    std::string password_client_tag_hash;

    // The fully populated sync specifics data for the password.
    sync_pb::PasswordSpecificsData password_data;

    // The duration for which the shared credential remains valid.
    base::TimeDelta time_to_live;

    // The OAuth2 client ID of the agent receiving the credential.
    std::string agent_oauth_client_id;
  };

  using SharePasswordCallback = base::OnceCallback<void(bool)>;

  ~RemoteActorCredentialSharingService() override = default;

  // Shares a password by uploading it to Passbox and then granting permission
  // in Actor Permission Service.
  virtual void SharePassword(const ShareParameters& params,
                             SharePasswordCallback callback) = 0;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_H_
