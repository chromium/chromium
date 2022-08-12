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
#include "chrome/common/channel_info.h"
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

}  // namespace

MacKeyRotationCommand::MacKeyRotationCommand(
    std::unique_ptr<KeyRotationManager> key_rotation_manager)
    : key_rotation_manager_(std::move(key_rotation_manager)),
      client_(SecureEnclaveClient::Create()) {
  DCHECK(key_rotation_manager_);
  DCHECK(client_);
}

MacKeyRotationCommand::MacKeyRotationCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : key_rotation_manager_(
          KeyRotationManager::Create(std::make_unique<MojoKeyNetworkDelegate>(
              std::move(url_loader_factory.get())))),
      client_(SecureEnclaveClient::Create()) {
  DCHECK(key_rotation_manager_);
  DCHECK(client_);
}

MacKeyRotationCommand::~MacKeyRotationCommand() = default;

void MacKeyRotationCommand::Trigger(const KeyRotationCommand::Params& params,
                                    Callback callback) {
  if (!client_->VerifyKeychainUnlocked()) {
    SYSLOG(ERROR)
        << "Device trust key rotation failed. The keychain is not unlocked.";
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

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

  key_rotation_manager_.get()->Rotate(
      dm_server_url, params.dm_token, params.nonce,
      base::BindOnce(
          [](std::unique_ptr<KeyRotationManager> manager, Callback callback,
             bool result) {
            if (!result) {
              SYSLOG(ERROR) << "Device trust key rotation failed.";
              std::move(callback).Run(KeyRotationCommand::Status::FAILED);
              return;
            }
            std::move(callback).Run(KeyRotationCommand::Status::SUCCEEDED);
          },
          std::move(key_rotation_manager_), std::move(callback)));
}

}  // namespace enterprise_connectors
