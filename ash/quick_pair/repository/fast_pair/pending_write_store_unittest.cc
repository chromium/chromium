// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSavedMacAddress1[] = "00:11:22:33:44";
constexpr char kSavedMacAddress2[] = "00:11:22:33:99";
const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
constexpr char kHexAccountKey1[] = "11223344556677889900AABBCCDDEEFF";
const std::vector<uint8_t> kAccountKey2{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0x22};
constexpr char kHexAccountKey2[] = "11223344556677889900AABBCCDDEE22";

const std::vector<uint8_t> kFastPairInfoBytes1{
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x33};

}  // namespace

namespace ash {
namespace quick_pair {

class PendingWriteStoreTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PendingWriteStore::RegisterProfilePrefs(pref_service_->registry());

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(pref_service_.get()));

    pending_write_store_ = std::make_unique<PendingWriteStore>();
  }

 protected:
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<PendingWriteStore> pending_write_store_;
};

TEST_F(PendingWriteStoreTest, WriteDevice) {
  ASSERT_TRUE(pending_write_store_->GetPendingWrites().empty());

  // Initialize fake FastPairInfo to pass to PendingWrite constructor.
  nearby::fastpair::FastPairInfo kFastPairInfo1;
  kFastPairInfo1.ParseFromArray(&kFastPairInfoBytes1[0],
                                kFastPairInfoBytes1.size());

  pending_write_store_->WritePairedDevice(kSavedMacAddress1, kFastPairInfo1);

  auto pending_writes = pending_write_store_->GetPendingWrites();
  ASSERT_EQ(1u, pending_writes.size());
  ASSERT_EQ(kSavedMacAddress1, pending_writes[0].mac_address);
  ASSERT_EQ(kFastPairInfo1.SerializeAsString(),
            pending_writes[0].fast_pair_info.SerializeAsString());

  pending_write_store_->OnPairedDeviceSaved(kSavedMacAddress1);
  ASSERT_TRUE(pending_write_store_->GetPendingWrites().empty());
}

TEST_F(PendingWriteStoreTest, WriteDeviceWithEmptyFastPairInfo) {
  ASSERT_TRUE(pending_write_store_->GetPendingWrites().empty());

  // Initialize fake empty FastPairInfo to pass to PendingWrite constructor.
  nearby::fastpair::FastPairInfo kFastPairInfo1;
  kFastPairInfo1.ParseFromString(std::string());

  pending_write_store_->WritePairedDevice(kSavedMacAddress1, kFastPairInfo1);

  auto pending_writes = pending_write_store_->GetPendingWrites();
  ASSERT_EQ(1u, pending_writes.size());
  ASSERT_EQ(kSavedMacAddress1, pending_writes[0].mac_address);
  ASSERT_TRUE(pending_writes[0].fast_pair_info.SerializeAsString().empty());

  pending_write_store_->OnPairedDeviceSaved(kSavedMacAddress1);
  ASSERT_TRUE(pending_write_store_->GetPendingWrites().empty());
}

TEST_F(PendingWriteStoreTest, DeleteDevice) {
  ASSERT_TRUE(pending_write_store_->GetPendingDeletes().empty());
  pending_write_store_->DeletePairedDevice(kSavedMacAddress1, kHexAccountKey1);

  auto pending_deletes = pending_write_store_->GetPendingDeletes();
  ASSERT_EQ(1u, pending_deletes.size());
  ASSERT_EQ(kSavedMacAddress1, pending_deletes[0].mac_address);
  ASSERT_EQ(kHexAccountKey1, pending_deletes[0].hex_account_key);

  pending_write_store_->OnPairedDeviceDeleted(kSavedMacAddress1);
  ASSERT_TRUE(pending_write_store_->GetPendingDeletes().empty());
}

TEST_F(PendingWriteStoreTest, DeleteDeviceByAccountKey) {
  ASSERT_TRUE(pending_write_store_->GetPendingDeletes().empty());
  // Add 2 pending deletes to the store.
  pending_write_store_->DeletePairedDevice(kSavedMacAddress1, kHexAccountKey1);
  pending_write_store_->DeletePairedDevice(kSavedMacAddress2, kHexAccountKey2);

  auto pending_deletes = pending_write_store_->GetPendingDeletes();
  ASSERT_EQ(2u, pending_deletes.size());
  ASSERT_EQ(kSavedMacAddress1, pending_deletes[0].mac_address);
  ASSERT_EQ(kHexAccountKey1, pending_deletes[0].hex_account_key);
  ASSERT_EQ(kSavedMacAddress2, pending_deletes[1].mac_address);
  ASSERT_EQ(kHexAccountKey2, pending_deletes[1].hex_account_key);

  // Remove the first pending delete from the store.
  pending_write_store_->OnPairedDeviceDeleted(kAccountKey1);

  pending_deletes = pending_write_store_->GetPendingDeletes();
  ASSERT_EQ(1u, pending_deletes.size());
  ASSERT_EQ(kSavedMacAddress2, pending_deletes[0].mac_address);
  ASSERT_EQ(kHexAccountKey2, pending_deletes[0].hex_account_key);

  // Remove the second pending delete from the store.
  pending_write_store_->OnPairedDeviceDeleted(kAccountKey2);
  ASSERT_TRUE(pending_write_store_->GetPendingDeletes().empty());
}

}  // namespace quick_pair
}  // namespace ash
