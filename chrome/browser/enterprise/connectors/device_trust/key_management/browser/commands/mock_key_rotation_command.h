// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MOCK_KEY_ROTATION_COMMAND_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MOCK_KEY_ROTATION_COMMAND_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

class MockKeyRotationCommand : public KeyRotationCommand {
 public:
  MockKeyRotationCommand();
  ~MockKeyRotationCommand() override;

  // KeyRotationCommand:
  MOCK_METHOD(void, Trigger, (const Params&, Callback), (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_MOCK_KEY_ROTATION_COMMAND_H_
