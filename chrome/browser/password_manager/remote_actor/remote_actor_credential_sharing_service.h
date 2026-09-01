// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/password_specifics.pb.h"

namespace password_manager {

// Interface for the service that coordinates password sharing with remote
// actors.
class RemoteActorCredentialSharingService : public KeyedService {
 public:
  struct ShareParameters {
    // The obfuscated GAIA ID of the user sharing the credential.
    std::string obfuscated_gaia_id;

    // The string representation of the target site's origin (e.g.
    // "https://github.com") for which the credential is valid.
    std::string web_origin;

    // The fully populated sync specifics data for the password.
    sync_pb::PasswordSpecificsData password_data;

    // The duration for which the shared credential remains valid.
    base::TimeDelta time_to_live;

    // The task ID of the remote actor requesting the credential.
    std::string task_id;

    // Computes the client tag hash identifying the credential from its
    // specifics data.
    std::string password_client_tag_hash() const {
      std::string client_tag = GetClientTag(password_data);
      return syncer::ClientTagHash::FromUnhashed(syncer::DataType::PASSWORDS,
                                                 client_tag)
          .value();
    }
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
