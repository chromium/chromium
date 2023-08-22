// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_HELPER_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_HELPER_IMPL_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"

namespace enterprise_connectors {

// Implementation of the SecureEnclaveHelper interface.
class SecureEnclaveHelperImpl : public SecureEnclaveHelper {
 public:
  ~SecureEnclaveHelperImpl() override;

  // SecureEnclaveHelper:
  base::apple::ScopedCFTypeRef<SecKeyRef> CreateSecureKey(
      CFDictionaryRef attributes,
      OSStatus* error) override;
  base::apple::ScopedCFTypeRef<SecKeyRef> CopyKey(CFDictionaryRef query,
                                                  OSStatus* error) override;
  OSStatus Update(CFDictionaryRef query,
                  CFDictionaryRef attributes_to_update) override;
  OSStatus Delete(CFDictionaryRef query) override;
  bool IsSecureEnclaveSupported() override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_HELPER_IMPL_H_
