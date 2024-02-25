// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_DEVICE_TRUST_KEY_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_DEVICE_TRUST_KEY_MANAGER_H_

#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

class MockDeviceTrustKeyManager : public DeviceTrustKeyManager {
 public:
  MockDeviceTrustKeyManager();
  ~MockDeviceTrustKeyManager() override;

  MOCK_METHOD(void, StartInitialization, (), (override));

  MOCK_METHOD(void,
              RotateKey,
              (const std::string&, base::OnceCallback<void(KeyRotationResult)>),
              (override));

  MOCK_METHOD(void,
              ExportPublicKeyAsync,
              (base::OnceCallback<void(std::optional<std::string>)>),
              (override));

  MOCK_METHOD(void,
              SignStringAsync,
              (const std::string&,
               base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>),
              (override));

  MOCK_METHOD(std::optional<MockDeviceTrustKeyManager::KeyMetadata>,
              GetLoadedKeyMetadata,
              (),
              (const, override));

  MOCK_METHOD(bool, HasPermanentFailure, (), (const, override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_MOCK_DEVICE_TRUST_KEY_MANAGER_H_
