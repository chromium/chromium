// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mac_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/syslog_logging.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

constexpr char kStableChannelHostName[] = "m.google.com";

bool ValidRotationCommand(const std::string& host_name) {
  return chrome::GetChannel() != version_info::Channel::STABLE ||
         host_name == kStableChannelHostName;
}

// Processes the `result` of the key rotation and returns it to the
// rotation `callback`. In the case of a key conflict error the `local_prefs`
// is used to disable the key creation process. The `key_rotation_manager` is
// given to keep the object that carries out the key rotation alive.
void OnKeyRotated(PrefService* local_prefs,
                  std::unique_ptr<KeyRotationManager> key_rotation_manager,
                  KeyRotationCommand::Callback callback,
                  KeyRotationManager::Result result) {
  if (result == KeyRotationManager::Result::FAILED) {
    SYSLOG(ERROR) << "Device trust key rotation failed.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  if (result == KeyRotationManager::Result::FAILED_KEY_CONFLICT) {
    SYSLOG(ERROR) << "Device trust key rotation failed. Conflict "
                     "with the key that exists on the server.";
    local_prefs->SetBoolean(kDeviceTrustDisableKeyCreationPref, true);
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  std::move(callback).Run(KeyRotationCommand::Status::SUCCEEDED);
}

}  // namespace

MacKeyRotationCommand::MacKeyRotationCommand(
    PrefService* local_prefs,
    std::unique_ptr<KeyRotationManager> key_rotation_manager)
    : local_prefs_(local_prefs),
      key_rotation_manager_(std::move(key_rotation_manager)),
      client_(SecureEnclaveClient::Create()) {
  DCHECK(key_rotation_manager_);
  DCHECK(client_);
}

MacKeyRotationCommand::MacKeyRotationCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_prefs)
    : local_prefs_(local_prefs),
      key_rotation_manager_(
          KeyRotationManager::Create(std::make_unique<MojoKeyNetworkDelegate>(
              std::move(url_loader_factory.get())))),
      client_(SecureEnclaveClient::Create()) {
  DCHECK(key_rotation_manager_);
  DCHECK(client_);
}

MacKeyRotationCommand::~MacKeyRotationCommand() = default;

void MacKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params,
                                    Callback callback) {
  if (!client_->VerifySecureEnclaveSupported()) {
    SYSLOG(ERROR) << "Device trust key rotation failed. The secure enclave is "
                     "not supported.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  GURL dm_server_url(params.dm_server_url);
  if (!ValidRotationCommand(dm_server_url.host())) {
    SYSLOG(ERROR)
        << "Device trust key rotation failed. The server URL is invalid.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }
  // TODO: b/243652906 update the rotation to happen from within a thread pool.
  key_rotation_manager_.get()->Rotate(
      dm_server_url, params.dm_token, params.nonce,
      base::BindOnce(OnKeyRotated, local_prefs_,
                     std::move(key_rotation_manager_), std::move(callback)));
}

}  // namespace enterprise_connectors
