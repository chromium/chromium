// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_SCOPED_KEY_ROTATION_COMMAND_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_SCOPED_KEY_ROTATION_COMMAND_FACTORY_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"

#include <memory>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace enterprise_connectors {

namespace test {
class MockKeyRotationCommand;
}  // namespace test

class ScopedKeyRotationCommandFactory : public KeyRotationCommandFactory {
 public:
  ScopedKeyRotationCommandFactory();
  ~ScopedKeyRotationCommandFactory() override;

  void SetMock(
      std::unique_ptr<test::MockKeyRotationCommand> mock_key_rotation_command);

  // KeyRotationCommandFactory:
  std::unique_ptr<KeyRotationCommand> CreateCommand(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_prefs) override;

 private:
  std::unique_ptr<test::MockKeyRotationCommand> mock_key_rotation_command_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_SCOPED_KEY_ROTATION_COMMAND_FACTORY_H_
