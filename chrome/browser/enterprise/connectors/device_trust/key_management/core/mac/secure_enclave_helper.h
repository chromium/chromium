// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_HELPER_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"

namespace enterprise_connectors {

// Wrapper for Security Framework Keychain APIs and Crypto utilities
// to allow them to be mocked in tests.
class SecureEnclaveHelper {
 public:
  virtual ~SecureEnclaveHelper() = default;

  static void SetInstanceForTesting(
      std::unique_ptr<SecureEnclaveHelper> helper);

  static std::unique_ptr<SecureEnclaveHelper> Create();

  // Uses the SecKeyCreateRandomKey API to create the secure key with its key
  // `attributes`. Returns the key or a nullptr on failure. If an `error`
  // pointer is given, its value will be set to the OSStatus returned by the
  // Keychain API call.
  virtual base::apple::ScopedCFTypeRef<SecKeyRef> CreateSecureKey(
      CFDictionaryRef attributes,
      OSStatus* error) = 0;

  // Uses the SecItemCopyMatching API to search the keychain using the
  // `query` dictionary. Returns the reference to the secure key or a nullptr
  // if the key is not found. If an `error` pointer
  // is given, its value will be set to the OSStatus returned by the Keychain
  // API call.
  virtual base::apple::ScopedCFTypeRef<SecKeyRef> CopyKey(CFDictionaryRef query,
                                                          OSStatus* error) = 0;

  // Uses the SecItemUpdate API to update the the key retrieved with the
  // `query` with its `attributes_to_update`. Returns the OSStatus value
  // returned by the Keychain API call.
  virtual OSStatus Update(CFDictionaryRef query,
                          CFDictionaryRef attributes_to_update) = 0;

  // Uses the SecItemDelete API to delete the key retrieved with the `query`.
  // Returns the OSStatus value returned by the Keychain API call.
  virtual OSStatus Delete(CFDictionaryRef query) = 0;

  // Uses the crypto library to check whether the Secure Enclave is supported on
  // the device. Returns true if it is supported and false otherwise.
  virtual bool IsSecureEnclaveSupported() = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_HELPER_H_
