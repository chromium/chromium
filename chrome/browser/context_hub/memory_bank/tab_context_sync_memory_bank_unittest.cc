// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/tab_context_sync_memory_bank.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "chrome/browser/context_hub/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {
namespace {

using ::testing::_;
using ::testing::Return;

class MockTabContextSyncService
    : public sync_tab_context::TabContextSyncService {
 public:
  MOCK_METHOD(std::optional<sync_tab_context::ContainerId>,
              CreateContainer,
              (),
              (override));
  MOCK_METHOD(bool,
              UploadPageContext,
              (const sync_tab_context::ContainerId&,
               const std::string&,
               std::string),
              (override));
  MOCK_METHOD(void,
              GetContainerAccessToken,
              (const sync_tab_context::ContainerId&,
               base::OnceCallback<void(std::optional<std::string>)>),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegateForContainer,
              (),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegateForItem,
              (),
              (override));
  MOCK_METHOD(bool, IsActiveForTesting, (), (const, override));
};

class TabContextSyncMemoryBankTest : public testing::Test {
 public:
  TabContextSyncMemoryBankTest() = default;
  ~TabContextSyncMemoryBankTest() override = default;

 protected:
  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    memory_bank_ = std::make_unique<TabContextSyncMemoryBank>(
        &pref_service_, mock_sync_service_);
  }

  TestingPrefServiceSimple pref_service_;
  testing::NiceMock<MockTabContextSyncService> mock_sync_service_;
  std::unique_ptr<TabContextSyncMemoryBank> memory_bank_;
};

TEST_F(TabContextSyncMemoryBankTest,
       CreatesContainerAndSavesToPrefsOnFirstSave) {
  const sync_tab_context::ContainerId kExpectedContainerId(
      base::Uuid::GenerateRandomV4());
  std::string apc_data = "Serialized APC data";
  EXPECT_CALL(mock_sync_service_, CreateContainer())
      .WillOnce(Return(kExpectedContainerId));
  EXPECT_CALL(mock_sync_service_,
              UploadPageContext(kExpectedContainerId, _, apc_data))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> save_future;
  MemoryBankEntry entry(MemoryBankType::kTab, GURL("https://example.com"),
                        "tab title", apc_data);
  memory_bank_->SaveMemoryBankEntry(std::move(entry),
                                    save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  // Verify ContainerId was persisted to prefs.
  EXPECT_EQ(
      pref_service_.GetString(prefs::kContextHubTabContextSyncContainerId),
      kExpectedContainerId.value().AsLowercaseString());
}

TEST_F(TabContextSyncMemoryBankTest, InMemoryDebugEntryCachePopulatedOnSave) {
  const sync_tab_context::ContainerId kExpectedContainerId(
      base::Uuid::GenerateRandomV4());
  std::string apc_data = "Serialized APC data";
  std::string tab_title = "Something happening!";
  EXPECT_CALL(mock_sync_service_, CreateContainer())
      .WillOnce(Return(kExpectedContainerId));
  EXPECT_CALL(mock_sync_service_,
              UploadPageContext(kExpectedContainerId, _, apc_data))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> save_future;
  MemoryBankEntry entry(MemoryBankType::kTab, GURL("https://example.com"),
                        tab_title, apc_data);
  memory_bank_->SaveMemoryBankEntry(std::move(entry),
                                    save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  // Verify entry is retrievable from local in-memory debug cache.
  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  memory_bank_->GetAllEntries(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].tab_title, tab_title);
  EXPECT_EQ(entries[0].selected_text, apc_data);
}

TEST_F(TabContextSyncMemoryBankTest, ReusesExistingContainerIdFromPrefs) {
  const base::Uuid kExistingUuid = base::Uuid::GenerateRandomV4();
  const sync_tab_context::ContainerId kExistingContainerId(kExistingUuid);
  pref_service_.SetString(prefs::kContextHubTabContextSyncContainerId,
                          kExistingUuid.AsLowercaseString());
  std::string apc_data = "Some page data";

  // CreateContainer should NOT be called.
  EXPECT_CALL(mock_sync_service_, CreateContainer()).Times(0);
  EXPECT_CALL(mock_sync_service_,
              UploadPageContext(kExistingContainerId, _, apc_data))
      .WillOnce(Return(true));

  base::test::TestFuture<bool> save_future;
  MemoryBankEntry entry(MemoryBankType::kTextSelection,
                        GURL("https://example.com/rubber-ducks"), "tab title",
                        apc_data);
  memory_bank_->SaveMemoryBankEntry(std::move(entry),
                                    save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());
}

TEST_F(TabContextSyncMemoryBankTest, ReturnsFalseWhenContainerCreationFails) {
  EXPECT_CALL(mock_sync_service_, CreateContainer())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mock_sync_service_, UploadPageContext(_, _, _)).Times(0);

  base::test::TestFuture<bool> save_future;
  MemoryBankEntry entry(MemoryBankType::kTab, GURL("https://example.com"),
                        "Title", "Data");
  memory_bank_->SaveMemoryBankEntry(std::move(entry),
                                    save_future.GetCallback());

  ASSERT_FALSE(save_future.Get());
}

TEST_F(TabContextSyncMemoryBankTest, ReturnsFalseWhenUploadFails) {
  const sync_tab_context::ContainerId kContainerId(
      base::Uuid::GenerateRandomV4());
  EXPECT_CALL(mock_sync_service_, CreateContainer())
      .WillOnce(Return(kContainerId));
  EXPECT_CALL(mock_sync_service_, UploadPageContext(kContainerId, _, _))
      .WillOnce(Return(false));

  base::test::TestFuture<bool> save_future;
  MemoryBankEntry entry(MemoryBankType::kTab, GURL("https://example.com"),
                        "Title", "Data");
  memory_bank_->SaveMemoryBankEntry(std::move(entry),
                                    save_future.GetCallback());

  ASSERT_FALSE(save_future.Get());
}

TEST_F(TabContextSyncMemoryBankTest, DeletesEntriesFromCache) {
  const sync_tab_context::ContainerId kContainerId(
      base::Uuid::GenerateRandomV4());
  EXPECT_CALL(mock_sync_service_, CreateContainer())
      .WillOnce(Return(kContainerId));
  EXPECT_CALL(mock_sync_service_, UploadPageContext(kContainerId, _, _))
      .WillOnce(Return(true));
  const int64_t entry_id = 12345;

  MemoryBankEntry entry(MemoryBankType::kTab, GURL("https://example.com"),
                        "Title", "Data");
  entry.id = entry_id;

  base::test::TestFuture<bool> save_future;
  memory_bank_->SaveMemoryBankEntry(std::move(entry),
                                    save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  base::test::TestFuture<bool> delete_future;
  memory_bank_->DeleteEntries(base::span_from_ref(entry_id),
                              delete_future.GetCallback());
  ASSERT_TRUE(delete_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  memory_bank_->GetAllEntries(get_future.GetCallback());
  ASSERT_TRUE(get_future.Get().empty());
}

}  // namespace
}  // namespace context_hub
