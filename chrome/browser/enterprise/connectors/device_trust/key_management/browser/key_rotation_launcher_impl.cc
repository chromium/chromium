// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_utils.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

KeyRotationLauncherImpl::KeyRotationLauncherImpl(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : dm_token_storage_(dm_token_storage),
      device_management_service_(device_management_service),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(url_loader_factory_);
}

KeyRotationLauncherImpl::~KeyRotationLauncherImpl() = default;

void KeyRotationLauncherImpl::LaunchKeyRotation(
    const std::string& nonce,
    KeyRotationCommand::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!dm_token_storage_) {
    std::move(callback).Run(
        KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN_STORAGE);
    return;
  }

  auto dm_token = dm_token_storage_->RetrieveDMToken();
  if (!dm_token.is_valid()) {
    std::move(callback).Run(KeyRotationCommand::Status::FAILED_INVALID_DMTOKEN);
    return;
  }

  if (!device_management_service_) {
    std::move(callback).Run(
        KeyRotationCommand::Status::FAILED_INVALID_MANAGEMENT_SERVICE);
    return;
  }

  auto dm_server_url = GetUploadBrowserPublicKeyUrl(
      dm_token_storage_->RetrieveClientId(), dm_token.value(),
      /*profile_id=*/std::nullopt, device_management_service_);
  if (!dm_server_url) {
    std::move(callback).Run(
        KeyRotationCommand::Status::FAILED_INVALID_DMSERVER_URL);
    return;
  }

  KeyRotationCommand::Params params{dm_token.value(), dm_server_url.value(),
                                    nonce};
  command_ = KeyRotationCommandFactory::GetInstance()->CreateCommand(
      url_loader_factory_, device_management_service_);
  if (!command_) {
    // Command can be nullptr if trying to create a key on a unsupported
    // platform.
    std::move(callback).Run(KeyRotationCommand::Status::FAILED_INVALID_COMMAND);
    return;
  }

  command_->Trigger(params, std::move(callback));
}

}  // namespace enterprise_connectors
