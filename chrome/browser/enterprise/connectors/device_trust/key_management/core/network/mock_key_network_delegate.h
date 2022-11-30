// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_MOCK_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_MOCK_KEY_NETWORK_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace enterprise_connectors {
namespace test {

// Mocked implementation of the KeyNetworkDelegate interface.
class MockKeyNetworkDelegate : public KeyNetworkDelegate {
 public:
  MockKeyNetworkDelegate();
  ~MockKeyNetworkDelegate() override;

  MOCK_METHOD(void,
              SendPublicKeyToDmServer,
              (const GURL&,
               const std::string&,
               const std::string&,
               UploadKeyCompletedCallback),
              (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_MOCK_KEY_NETWORK_DELEGATE_H_
