// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/mock_key_rotation_command.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {

ScopedKeyRotationCommandFactory::ScopedKeyRotationCommandFactory() {
  KeyRotationCommandFactory::SetFactoryInstanceForTesting(this);
}

ScopedKeyRotationCommandFactory::~ScopedKeyRotationCommandFactory() {
  KeyRotationCommandFactory::ClearFactoryInstanceForTesting();
}

void ScopedKeyRotationCommandFactory::SetMock(
    std::unique_ptr<test::MockKeyRotationCommand> mock_key_rotation_command) {
  DCHECK(mock_key_rotation_command);

  mock_key_rotation_command_ = std::move(mock_key_rotation_command);
  return_invalid_command = false;
}

void ScopedKeyRotationCommandFactory::ReturnInvalidCommand() {
  return_invalid_command = true;
}

std::unique_ptr<KeyRotationCommand>
ScopedKeyRotationCommandFactory::CreateCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    policy::DeviceManagementService* device_management_service) {
  if (return_invalid_command) {
    return nullptr;
  }

  if (mock_key_rotation_command_) {
    return std::move(mock_key_rotation_command_);
  }
  return std::make_unique<testing::StrictMock<test::MockKeyRotationCommand>>();
}

}  // namespace enterprise_connectors
