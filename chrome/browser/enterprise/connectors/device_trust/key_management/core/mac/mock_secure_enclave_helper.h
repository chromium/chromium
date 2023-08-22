// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_MOCK_SECURE_ENCLAVE_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_MOCK_SECURE_ENCLAVE_HELPER_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

// Mocked implementation of the SecureEnclaveHelper interface.
class MockSecureEnclaveHelper : public SecureEnclaveHelper {
 public:
  MockSecureEnclaveHelper();
  ~MockSecureEnclaveHelper() override;

  MOCK_METHOD(base::apple::ScopedCFTypeRef<SecKeyRef>,
              CreateSecureKey,
              (CFDictionaryRef, OSStatus*),
              (override));
  MOCK_METHOD(base::apple::ScopedCFTypeRef<SecKeyRef>,
              CopyKey,
              (CFDictionaryRef, OSStatus*),
              (override));
  MOCK_METHOD(OSStatus, Update, (CFDictionaryRef, CFDictionaryRef), (override));
  MOCK_METHOD(OSStatus, Delete, (CFDictionaryRef), (override));
  MOCK_METHOD(bool, IsSecureEnclaveSupported, (), (override));
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_MOCK_SECURE_ENCLAVE_HELPER_H_
