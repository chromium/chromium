// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_KEY_ROTATION_LAUNCHER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_KEY_ROTATION_LAUNCHER_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

class MockKeyRotationLauncher : public KeyRotationLauncher {
 public:
  MockKeyRotationLauncher();
  ~MockKeyRotationLauncher() override;

  MOCK_METHOD(void,
              LaunchKeyRotation,
              (const std::string&, KeyRotationCommand::Callback),
              (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_KEY_ROTATION_LAUNCHER_H_
