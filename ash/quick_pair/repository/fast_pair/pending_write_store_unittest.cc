// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSavedMacAddress[] = "00:11:22:33:44";
constexpr char kHexModelId[] = "aabb11";
constexpr char kHexAccountKey[] = "ffffffffff";

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

TEST_F(PendingWriteStoreTest, AddDevice) {
  ASSERT_TRUE(pending_write_store_->GetPendingAdds().empty());
  pending_write_store_->AddPairedDevice(kSavedMacAddress, kHexModelId);

  auto pending_adds = pending_write_store_->GetPendingAdds();
  ASSERT_EQ(1u, pending_adds.size());
  ASSERT_EQ(kSavedMacAddress, pending_adds[0].mac_address);
  ASSERT_EQ(kHexModelId, pending_adds[0].hex_model_id);

  pending_write_store_->OnPairedDeviceSaved(kSavedMacAddress);
  ASSERT_TRUE(pending_write_store_->GetPendingAdds().empty());
}

TEST_F(PendingWriteStoreTest, DeleteDevice) {
  ASSERT_TRUE(pending_write_store_->GetPendingDeletes().empty());
  pending_write_store_->DeletePairedDevice(kHexAccountKey);

  auto pending_deletes = pending_write_store_->GetPendingDeletes();
  ASSERT_EQ(1u, pending_deletes.size());
  ASSERT_EQ(kHexAccountKey, pending_deletes[0]);

  pending_write_store_->OnPairedDeviceDeleted(kHexAccountKey);
  ASSERT_TRUE(pending_write_store_->GetPendingDeletes().empty());
}

}  // namespace quick_pair
}  // namespace ash
