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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace enterprise_connectors {

SecureEnclaveHelperImpl::~SecureEnclaveHelperImpl() = default;

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveHelperImpl::CreateSecureKey(
    CFDictionaryRef attributes) {
  base::ScopedCFTypeRef<SecKeyRef> key(
      SecKeyCreateRandomKey(attributes, nullptr));
  return key;
}

bool SecureEnclaveHelperImpl::Update(CFDictionaryRef query,
                                     CFDictionaryRef attributes_to_update) {
  return SecItemUpdate(query, attributes_to_update) == errSecSuccess;
}

bool SecureEnclaveHelperImpl::Delete(CFDictionaryRef query) {
  return SecItemDelete(query) == errSecSuccess;
}

base::ScopedCFTypeRef<SecKeyRef> SecureEnclaveHelperImpl::CopyKey(
    CFDictionaryRef query) {
  base::ScopedCFTypeRef<SecKeyRef> key;
  SecItemCopyMatching(
      query, const_cast<CFTypeRef*>(
                 reinterpret_cast<const CFTypeRef*>(key.InitializeInto())));
  return key;
}

bool SecureEnclaveHelperImpl::IsSecureEnclaveSupported() {
  TKTokenWatcher* token_watcher = [[TKTokenWatcher alloc] init];
  return ([token_watcher.tokenIDs
      containsObject:base::apple::CFToNSPtrCast(kSecAttrTokenIDSecureEnclave)]);
}

}  // namespace enterprise_connectors
