// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_MOCK_SECURE_ENCLAVE_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_MOCK_SECURE_ENCLAVE_CLIENT_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

// Mocked implementation of the SecureEnclaveClient interface.
class MockSecureEnclaveClient : public SecureEnclaveClient {
 public:
  MockSecureEnclaveClient();
  ~MockSecureEnclaveClient() override;

  MOCK_METHOD(base::apple::ScopedCFTypeRef<SecKeyRef>,
              CreatePermanentKey,
              (),
              (override));
  MOCK_METHOD(base::apple::ScopedCFTypeRef<SecKeyRef>,
              CopyStoredKey,
              (KeyType, OSStatus*),
              (override));
  MOCK_METHOD(bool, UpdateStoredKeyLabel, (KeyType, KeyType), (override));
  MOCK_METHOD(bool, DeleteKey, (KeyType), (override));
  MOCK_METHOD(bool,
              ExportPublicKey,
              (SecKeyRef, std::vector<uint8_t>&, OSStatus*),
              (override));
  MOCK_METHOD(
      bool,
      SignDataWithKey,
      (SecKeyRef, base::span<const uint8_t>, std::vector<uint8_t>&, OSStatus*),
      (override));
  MOCK_METHOD(bool, VerifySecureEnclaveSupported, (), (override));
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_MOCK_SECURE_ENCLAVE_CLIENT_H_
