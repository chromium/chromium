// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/colocation/colocation_proof_keychain_store_mac.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/apple/bridging.h"
#include "base/apple/osstatus_logging.h"
#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_version.h"
#include "crypto/apple/keychain_v2.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace browser_actuator {

const char kColocationProofKeychainAccessGroup[] = MAC_TEAM_IDENTIFIER_STRING
    "." MAC_BUNDLE_IDENTIFIER_STRING ".local-actuation";

const char kColocationProofKeychainService[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".local-actuation.colocation-proof";

namespace {

// Identifies the proof item for `client_id`. Access groups are only enforced
// in the Data Protection Keychain, so every query opts into it.
NSMutableDictionary* ItemQuery(std::string_view client_id) {
  return [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(kColocationProofKeychainAccessGroup),
    CFToNSPtrCast(kSecAttrService) :
        base::SysUTF8ToNSString(kColocationProofKeychainService),
    CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(client_id),
    CFToNSPtrCast(kSecUseDataProtectionKeychain) : @YES,
  }];
}

}  // namespace

bool WriteColocationProofToKeychain(std::string_view client_id,
                                    base::span<const uint8_t> proof) {
  CHECK(!client_id.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  crypto::apple::KeychainV2& keychain =
      crypto::apple::KeychainV2::GetInstance();
  NSData* data = [NSData dataWithBytes:proof.data() length:proof.size()];

  // Update in place if the item exists, so readers never observe a gap.
  NSDictionary* update = @{CFToNSPtrCast(kSecValueData) : data};
  OSStatus status = keychain.ItemUpdate(NSToCFPtrCast(ItemQuery(client_id)),
                                        NSToCFPtrCast(update));
  if (status == errSecSuccess) {
    return true;
  }
  if (status != errSecItemNotFound) {
    OSSTATUS_DLOG(ERROR, status) << "Updating co-location proof failed";
    return false;
  }

  NSMutableDictionary* attributes = ItemQuery(client_id);
  [attributes addEntriesFromDictionary:@{
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
    CFToNSPtrCast(kSecAttrAccessible) :
        CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly),
    CFToNSPtrCast(kSecValueData) : data,
  }];
  status = keychain.ItemAdd(NSToCFPtrCast(attributes), /*result=*/nullptr);
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "Adding co-location proof failed";
    return false;
  }
  return true;
}

bool DeleteColocationProofFromKeychain(std::string_view client_id) {
  CHECK(!client_id.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  OSStatus status = crypto::apple::KeychainV2::GetInstance().ItemDelete(
      NSToCFPtrCast(ItemQuery(client_id)));
  if (status != errSecSuccess && status != errSecItemNotFound) {
    OSSTATUS_DLOG(ERROR, status) << "Deleting co-location proof failed";
    return false;
  }
  return true;
}

}  // namespace browser_actuator
