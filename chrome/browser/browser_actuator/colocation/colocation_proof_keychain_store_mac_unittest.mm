// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/colocation/colocation_proof_keychain_store_mac.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/containers/to_vector.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace browser_actuator {
namespace {

using ::base::apple::CFToNSPtrCast;
using ::testing::ElementsAreArray;
using ::testing::Optional;

constexpr std::string_view kClientId = "client-id-1";
constexpr std::string_view kOtherClientId = "client-id-2";
constexpr auto kProof = std::to_array<uint8_t>({0x0a, 0x01, 0x02, 0x03});
constexpr auto kNewerProof = std::to_array<uint8_t>({0x0a, 0x04, 0x05});

class ColocationProofKeychainStoreTest : public testing::Test {
 protected:
  crypto::apple::FakeKeychainV2& keychain() {
    return *fake_keychain_.keychain();
  }

  // Returns the attributes of the stored proof item for `client_id`, or nil.
  NSDictionary* FindItem(std::string_view client_id) {
    NSString* account = base::SysUTF8ToNSString(client_id);
    NSString* service =
        base::SysUTF8ToNSString(kColocationProofKeychainService);
    for (const auto& item : keychain().items()) {
      NSDictionary* attributes = CFToNSPtrCast(item.get());
      if ([attributes[CFToNSPtrCast(kSecAttrAccount)] isEqual:account] &&
          [attributes[CFToNSPtrCast(kSecAttrService)] isEqual:service]) {
        return attributes;
      }
    }
    return nil;
  }

  // Returns the stored proof bytes for `client_id`, if any.
  std::optional<std::vector<uint8_t>> ReadProof(std::string_view client_id) {
    NSDictionary* item = FindItem(client_id);
    if (!item) {
      return std::nullopt;
    }
    NSData* data =
        base::apple::ObjCCast<NSData>(item[CFToNSPtrCast(kSecValueData)]);
    return base::ToVector(base::apple::NSDataToSpan(data));
  }

 private:
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{
      kColocationProofKeychainAccessGroup};
};

TEST_F(ColocationProofKeychainStoreTest, WriteStoresProof) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  EXPECT_THAT(ReadProof(kClientId), Optional(ElementsAreArray(kProof)));
}

TEST_F(ColocationProofKeychainStoreTest, SecondWriteReplacesProof) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kNewerProof));

  EXPECT_THAT(ReadProof(kClientId), Optional(ElementsAreArray(kNewerProof)));
}

TEST_F(ColocationProofKeychainStoreTest, SecondWriteKeepsOneItem) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kNewerProof));

  EXPECT_EQ(keychain().items().size(), 1u);
}

TEST_F(ColocationProofKeychainStoreTest, ClientIdsAreStoredSeparately) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  ASSERT_TRUE(WriteColocationProofToKeychain(kOtherClientId, kNewerProof));

  EXPECT_THAT(ReadProof(kClientId), Optional(ElementsAreArray(kProof)));
}

TEST_F(ColocationProofKeychainStoreTest, DeleteRemovesProof) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  ASSERT_TRUE(DeleteColocationProofFromKeychain(kClientId));

  EXPECT_EQ(ReadProof(kClientId), std::nullopt);
}

TEST_F(ColocationProofKeychainStoreTest, DeleteLeavesOtherClientIds) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));
  ASSERT_TRUE(WriteColocationProofToKeychain(kOtherClientId, kNewerProof));

  ASSERT_TRUE(DeleteColocationProofFromKeychain(kClientId));

  EXPECT_THAT(ReadProof(kOtherClientId),
              Optional(ElementsAreArray(kNewerProof)));
}

TEST_F(ColocationProofKeychainStoreTest, DeleteMissingProofSucceeds) {
  EXPECT_TRUE(DeleteColocationProofFromKeychain(kClientId));
}

TEST_F(ColocationProofKeychainStoreTest, WriteFailsWhenUpdateFails) {
  keychain().set_item_update_result(errSecInteractionNotAllowed);

  EXPECT_FALSE(WriteColocationProofToKeychain(kClientId, kProof));
}

TEST_F(ColocationProofKeychainStoreTest, WriteFailsWhenAddFails) {
  keychain().set_item_add_result(errSecMissingEntitlement);

  EXPECT_FALSE(WriteColocationProofToKeychain(kClientId, kProof));
}

TEST_F(ColocationProofKeychainStoreTest, WriteUsesSharedAccessGroup) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecAttrAccessGroup)],
              base::SysUTF8ToNSString(kColocationProofKeychainAccessGroup));
}

TEST_F(ColocationProofKeychainStoreTest, WriteUsesDataProtectionKeychain) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecUseDataProtectionKeychain)],
              @YES);
}

TEST_F(ColocationProofKeychainStoreTest, WriteDoesNotSyncToICloud) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecAttrSynchronizable)], @NO);
}

TEST_F(ColocationProofKeychainStoreTest,
       WriteIsReadableAfterFirstUnlockOnThisDeviceOnly) {
  ASSERT_TRUE(WriteColocationProofToKeychain(kClientId, kProof));

  EXPECT_NSEQ(FindItem(kClientId)[CFToNSPtrCast(kSecAttrAccessible)],
              CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly));
}

}  // namespace
}  // namespace browser_actuator
