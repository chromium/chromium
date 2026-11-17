// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/profile_registry/profile_registry_keychain_store_mac.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/browser_actuator/local_actuation/local_actuation_names.h"
#include "crypto/apple/keychain_v2.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace browser_actuator {

namespace {

// Returns the attributes common to every registry query and item.
NSMutableDictionary* BaseQuery() {
  return [@{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(kLocalActuationKeychainAccessGroup),
    CFToNSPtrCast(kSecAttrService) :
        base::SysUTF8ToNSString(kProfileRegistryKeychainService),
    // Only the Data Protection keychain enforces access groups.
    CFToNSPtrCast(kSecUseDataProtectionKeychain) : @YES,
  } mutableCopy];
}

// Returns a query that matches the registry item for `client_id`.
NSMutableDictionary* ItemQuery(std::string_view client_id) {
  NSMutableDictionary* query = BaseQuery();
  query[CFToNSPtrCast(kSecAttrAccount)] = base::SysUTF8ToNSString(client_id);
  return query;
}

}  // namespace

ProfileRegistryKeychainItem::ProfileRegistryKeychainItem() = default;
ProfileRegistryKeychainItem::ProfileRegistryKeychainItem(
    std::string client_id,
    std::vector<uint8_t> entry_bytes)
    : client_id(std::move(client_id)), entry_bytes(std::move(entry_bytes)) {}
ProfileRegistryKeychainItem::ProfileRegistryKeychainItem(
    const ProfileRegistryKeychainItem&) = default;
ProfileRegistryKeychainItem& ProfileRegistryKeychainItem::operator=(
    const ProfileRegistryKeychainItem&) = default;
ProfileRegistryKeychainItem::ProfileRegistryKeychainItem(
    ProfileRegistryKeychainItem&&) noexcept = default;
ProfileRegistryKeychainItem& ProfileRegistryKeychainItem::operator=(
    ProfileRegistryKeychainItem&&) noexcept = default;
ProfileRegistryKeychainItem::~ProfileRegistryKeychainItem() = default;

bool WriteProfileRegistryEntryToKeychain(
    std::string_view client_id,
    base::span<const uint8_t> entry_bytes) {
  CHECK(!client_id.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  crypto::apple::KeychainV2& keychain =
      crypto::apple::KeychainV2::GetInstance();
  NSData* data = [NSData dataWithBytes:entry_bytes.data()
                                length:entry_bytes.size()];

  // Update in place if the item exists, so readers never observe a gap.
  NSDictionary* update = @{CFToNSPtrCast(kSecValueData) : data};
  OSStatus status = keychain.ItemUpdate(NSToCFPtrCast(ItemQuery(client_id)),
                                        NSToCFPtrCast(update));
  if (status == errSecSuccess) {
    return true;
  }
  if (status != errSecItemNotFound) {
    OSSTATUS_DLOG(ERROR, status) << "Updating profile registry entry failed";
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
    OSSTATUS_DLOG(ERROR, status) << "Adding profile registry entry failed";
    return false;
  }
  return true;
}

bool DeleteProfileRegistryEntryFromKeychain(std::string_view client_id) {
  CHECK(!client_id.empty());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  OSStatus status = crypto::apple::KeychainV2::GetInstance().ItemDelete(
      NSToCFPtrCast(ItemQuery(client_id)));
  if (status != errSecSuccess && status != errSecItemNotFound) {
    OSSTATUS_DLOG(ERROR, status) << "Deleting profile registry entry failed";
    return false;
  }
  return true;
}

std::vector<ProfileRegistryKeychainItem>
ReadAllProfileRegistryEntriesFromKeychain() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSMutableDictionary* query = BaseQuery();
  [query addEntriesFromDictionary:@{
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitAll),
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
    CFToNSPtrCast(kSecReturnData) : @YES,
  }];

  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = crypto::apple::KeychainV2::GetInstance().ItemCopyMatching(
      NSToCFPtrCast(query), result.InitializeInto());
  if (status == errSecItemNotFound) {
    return {};
  }
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "Listing profile registry entries failed";
    return {};
  }

  std::vector<ProfileRegistryKeychainItem> items;
  CFArrayRef array = base::apple::CFCast<CFArrayRef>(result.get());
  if (!array) {
    return items;
  }
  for (CFIndex i = 0; i < CFArrayGetCount(array); ++i) {
    CFDictionaryRef item =
        base::apple::CFCast<CFDictionaryRef>(CFArrayGetValueAtIndex(array, i));
    if (!item) {
      continue;
    }
    CFStringRef account =
        base::apple::GetValueFromDictionary<CFStringRef>(item, kSecAttrAccount);
    CFDataRef data =
        base::apple::GetValueFromDictionary<CFDataRef>(item, kSecValueData);
    if (!account || !data) {
      continue;
    }
    items.emplace_back(base::SysCFStringRefToUTF8(account),
                       base::ToVector(base::apple::CFDataToSpan(data)));
  }
  return items;
}

}  // namespace browser_actuator
