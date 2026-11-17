// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/profile_registry/profile_registry_keychain_store_mac.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/containers/to_vector.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_actuator/local_actuation/local_actuation_names.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace browser_actuator {
namespace {

using ::base::apple::CFToNSPtrCast;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

constexpr char kClientId[] = "client-id-1";
constexpr char kOtherClientId[] = "client-id-2";
constexpr auto kEntry = std::to_array<uint8_t>({0x0a, 0x01, 0x02, 0x03});
constexpr auto kNewerEntry = std::to_array<uint8_t>({0x0a, 0x04, 0x05});

class ProfileRegistryKeychainStoreTest : public testing::Test {
 protected:
  crypto::apple::FakeKeychainV2& keychain() {
    return *fake_keychain_.keychain();
  }

  // Returns the attributes of the stored registry item for `client_id`, or nil.
  NSDictionary* FindItem(std::string_view client_id) {
    NSString* account = base::SysUTF8ToNSString(client_id);
    NSString* service =
        base::SysUTF8ToNSString(kProfileRegistryKeychainService);
    for (const auto& item : keychain().items()) {
      NSDictionary* attributes = CFToNSPtrCast(item.get());
      if ([attributes[CFToNSPtrCast(kSecAttrAccount)] isEqual:account] &&
          [attributes[CFToNSPtrCast(kSecAttrService)] isEqual:service]) {
        return attributes;
      }
    }
    return nil;
  }

  // Returns the stored entry bytes for `client_id`, if any.
  std::optional<std::vector<uint8_t>> ReadEntry(std::string_view client_id) {
    NSDictionary* item = FindItem(client_id);
    if (!item) {
      return std::nullopt;
    }
    NSData* data =
        base::apple::ObjCCast<NSData>(item[CFToNSPtrCast(kSecValueData)]);
    return base::ToVector(base::apple::NSDataToSpan(data));
  }

  // Adds an item for `client_id` under a different service in the same access
  // group, as the co-location proof store would.
  void AddItemUnderOtherService(std::string_view client_id) {
    static constexpr char kOtherService[] =
        "com.google.Chrome.local-actuation.other";
    NSDictionary* attributes = @{
      CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
      CFToNSPtrCast(kSecAttrAccessGroup) :
          base::SysUTF8ToNSString(kLocalActuationKeychainAccessGroup),
      CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(kOtherService),
      CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(client_id),
      CFToNSPtrCast(kSecUseDataProtectionKeychain) : @YES,
      CFToNSPtrCast(kSecValueData) : [NSData dataWithBytes:"x" length:1],
    };
    ASSERT_EQ(keychain().ItemAdd(base::apple::NSToCFPtrCast(attributes),
                                 /*result=*/nullptr),
              errSecSuccess);
  }

 private:
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{
      kLocalActuationKeychainAccessGroup};
};

TEST_F(ProfileRegistryKeychainStoreTest, WriteStoresEntry) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  EXPECT_THAT(ReadEntry(kClientId), Optional(ElementsAreArray(kEntry)));
}

TEST_F(ProfileRegistryKeychainStoreTest, SecondWriteReplacesEntry) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kNewerEntry));

  EXPECT_THAT(ReadEntry(kClientId), Optional(ElementsAreArray(kNewerEntry)));
  EXPECT_EQ(keychain().items().size(), 1u);
}

TEST_F(ProfileRegistryKeychainStoreTest, ClientIdsAreStoredSeparately) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kOtherClientId, kNewerEntry));

  EXPECT_THAT(ReadEntry(kClientId), Optional(ElementsAreArray(kEntry)));
}

TEST_F(ProfileRegistryKeychainStoreTest, DeleteRemovesEntry) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  ASSERT_TRUE(DeleteProfileRegistryEntryFromKeychain(kClientId));

  EXPECT_EQ(ReadEntry(kClientId), std::nullopt);
}

TEST_F(ProfileRegistryKeychainStoreTest, DeleteLeavesOtherClientIds) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kOtherClientId, kNewerEntry));

  ASSERT_TRUE(DeleteProfileRegistryEntryFromKeychain(kClientId));

  EXPECT_THAT(ReadEntry(kOtherClientId),
              Optional(ElementsAreArray(kNewerEntry)));
}

TEST_F(ProfileRegistryKeychainStoreTest, DeleteMissingEntrySucceeds) {
  EXPECT_TRUE(DeleteProfileRegistryEntryFromKeychain(kClientId));
}

TEST_F(ProfileRegistryKeychainStoreTest, WriteFailsWhenUpdateFails) {
  keychain().set_item_update_result(errSecInteractionNotAllowed);

  EXPECT_FALSE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));
}

TEST_F(ProfileRegistryKeychainStoreTest, WriteFailsWhenAddFails) {
  keychain().set_item_add_result(errSecMissingEntitlement);

  EXPECT_FALSE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));
}

TEST_F(ProfileRegistryKeychainStoreTest, WriteUsesSharedAccessGroup) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecAttrAccessGroup)],
              base::SysUTF8ToNSString(kLocalActuationKeychainAccessGroup));
}

TEST_F(ProfileRegistryKeychainStoreTest, WriteUsesDataProtectionKeychain) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecUseDataProtectionKeychain)],
              @YES);
}

TEST_F(ProfileRegistryKeychainStoreTest, WriteDoesNotSyncToICloud) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecAttrSynchronizable)], @NO);
}

TEST_F(ProfileRegistryKeychainStoreTest,
       WriteIsReadableAfterFirstUnlockOnThisDeviceOnly) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecAttrAccessible)],
              CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly));
}

TEST_F(ProfileRegistryKeychainStoreTest, ReadAllIsEmptyWhenNothingStored) {
  EXPECT_THAT(ReadAllProfileRegistryEntriesFromKeychain(), IsEmpty());
}

TEST_F(ProfileRegistryKeychainStoreTest, ReadAllReturnsEveryEntry) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kOtherClientId, kNewerEntry));

  EXPECT_THAT(
      ReadAllProfileRegistryEntriesFromKeychain(),
      UnorderedElementsAre(
          ProfileRegistryKeychainItem(kClientId, base::ToVector(kEntry)),
          ProfileRegistryKeychainItem(kOtherClientId,
                                      base::ToVector(kNewerEntry))));
}

TEST_F(ProfileRegistryKeychainStoreTest, ReadAllIgnoresOtherServices) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));
  ASSERT_NO_FATAL_FAILURE(AddItemUnderOtherService(kClientId));

  EXPECT_THAT(ReadAllProfileRegistryEntriesFromKeychain(),
              UnorderedElementsAre(ProfileRegistryKeychainItem(
                  kClientId, base::ToVector(kEntry))));
}

TEST_F(ProfileRegistryKeychainStoreTest, DeleteLeavesOtherServices) {
  ASSERT_TRUE(WriteProfileRegistryEntryToKeychain(kClientId, kEntry));
  ASSERT_NO_FATAL_FAILURE(AddItemUnderOtherService(kClientId));

  ASSERT_TRUE(DeleteProfileRegistryEntryFromKeychain(kClientId));

  // The registry item is gone; the item under the other service remains.
  EXPECT_EQ(ReadEntry(kClientId), std::nullopt);
  EXPECT_EQ(keychain().items().size(), 1u);
}

}  // namespace
}  // namespace browser_actuator
