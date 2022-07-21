// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper_impl.h"

#include <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"

namespace enterprise_connectors {

SecureEnclaveHelperImpl::~SecureEnclaveHelperImpl() = default;

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveHelperImpl::CreateSecureKey(
    CFDictionaryRef attributes) {
  base::ScopedCFTypeRef<CFErrorRef> error;
  base::ScopedCFTypeRef<SecKeyRef> key(
      SecKeyCreateRandomKey(attributes, error.InitializeInto()));
  return key;
}

bool SecureEnclaveHelperImpl::Update(CFDictionaryRef query,
                                     CFDictionaryRef attributes_to_update) {
  return SecItemUpdate(query, attributes_to_update) == errSecSuccess;
}

bool SecureEnclaveHelperImpl::Delete(CFDictionaryRef query) {
  return SecItemDelete(query) == errSecSuccess;
}

bool SecureEnclaveHelperImpl::CheckExists(CFDictionaryRef query) {
  base::ScopedCFTypeRef<CFTypeRef> key;
  SecItemCopyMatching(query, key.InitializeInto());
  return key != nullptr;
}

bool SecureEnclaveHelperImpl::CheckKeychainUnlocked() {
  base::ScopedCFTypeRef<SecKeychainRef> keychain;
  auto status = SecKeychainCopyDefault(keychain.InitializeInto());
  if (status != noErr)
    return false;

  SecKeychainStatus keychain_status;
  status = SecKeychainGetStatus(keychain, &keychain_status);
  if (status != noErr)
    return false;

  return keychain_status & kSecUnlockStateStatus;
}

bool SecureEnclaveHelperImpl::IsSecureEnclaveSupported() {
  base::scoped_nsobject<TKTokenWatcher> token_watcher(
      [[TKTokenWatcher alloc] init]);
  return ([token_watcher.get().tokenIDs
      containsObject:base::mac::CFToNSCast(kSecAttrTokenIDSecureEnclave)]);
}

}  // namespace enterprise_connectors
