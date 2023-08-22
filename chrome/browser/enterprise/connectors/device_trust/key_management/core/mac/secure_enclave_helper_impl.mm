// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper_impl.h"

#include <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <memory>

#include "base/apple/bridging.h"
#include "base/mac/scoped_cftyperef.h"

namespace enterprise_connectors {

SecureEnclaveHelperImpl::~SecureEnclaveHelperImpl() = default;

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveHelperImpl::CreateSecureKey(
    CFDictionaryRef attributes,
    OSStatus* error) {
  base::ScopedCFTypeRef<CFErrorRef> error_ref;
  base::ScopedCFTypeRef<SecKeyRef> key(
      SecKeyCreateRandomKey(attributes, error_ref.InitializeInto()));

  if (error && error_ref) {
    *error = CFErrorGetCode(error_ref);
  }

  return key;
}

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveHelperImpl::CopyKey(
    CFDictionaryRef query,
    OSStatus* error) {
  base::ScopedCFTypeRef<SecKeyRef> key;
  OSStatus status = SecItemCopyMatching(
      query, const_cast<CFTypeRef*>(
                 reinterpret_cast<const CFTypeRef*>(key.InitializeInto())));

  if (error) {
    *error = status;
  }

  return key;
}

OSStatus SecureEnclaveHelperImpl::Update(CFDictionaryRef query,
                                         CFDictionaryRef attributes_to_update) {
  return SecItemUpdate(query, attributes_to_update);
}

OSStatus SecureEnclaveHelperImpl::Delete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

bool SecureEnclaveHelperImpl::IsSecureEnclaveSupported() {
  TKTokenWatcher* token_watcher = [[TKTokenWatcher alloc] init];
  return ([token_watcher.tokenIDs
      containsObject:base::apple::CFToNSPtrCast(kSecAttrTokenIDSecureEnclave)]);
}

}  // namespace enterprise_connectors
