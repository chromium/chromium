// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_SCOPED_KEY_ROTATION_COMMAND_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_SCOPED_KEY_ROTATION_COMMAND_FACTORY_H_

#include <memory>
#include <optional>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace enterprise_connectors {

namespace test {
class MockKeyRotationCommand;
}  // namespace test

class ScopedKeyRotationCommandFactory : public KeyRotationCommandFactory {
 public:
  ScopedKeyRotationCommandFactory();
  ~ScopedKeyRotationCommandFactory() override;

  // Will set `mock_key_rotation_command` to be the next value returned by the
  // KeyRotationCommandFactory. If nullptr, will clear all settings and default
  // to the original implementation.
  void SetMock(
      std::unique_ptr<test::MockKeyRotationCommand> mock_key_rotation_command);

  // Will force the factory to return nullptr as the next commands.
  void ReturnInvalidCommand();

  // KeyRotationCommandFactory:
  std::unique_ptr<KeyRotationCommand> CreateCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      policy::DeviceManagementService* device_management_service) override;

 private:
  bool return_invalid_command = false;

  std::unique_ptr<test::MockKeyRotationCommand> mock_key_rotation_command_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_SCOPED_KEY_ROTATION_COMMAND_FACTORY_H_
