// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/context_hub_backend_impl.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

class ContextHubBackendImplTest : public testing::Test {
 public:
  ContextHubBackendImplTest() = default;
  ~ContextHubBackendImplTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("ContextHub.db");
    backend_ = std::make_unique<ContextHubBackendImpl>(db_path_);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<ContextHubBackendImpl> backend_;
};

TEST_F(ContextHubBackendImplTest, SaveTabAndRetrieve) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://example.com");
  entry.tab_title = "Example";
  entry.selected_text = "Page content";

  base::test::TestFuture<bool> save_future;
  backend_->AddOrUpdateMemoryBankEntry(entry, save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  backend_->GetAllMemoryBankEntries(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(GURL("https://example.com"), entries[0].url);
  EXPECT_EQ("Example", entries[0].tab_title);
  ASSERT_TRUE(entries[0].selected_text.has_value());
  EXPECT_EQ("Page content", entries[0].selected_text.value());
}

TEST_F(ContextHubBackendImplTest, SaveTextSelectionAndDelete) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTextSelection;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://google.com");
  entry.tab_title = "Google";
  entry.selected_text = "Search text";

  base::test::TestFuture<bool> save_future;
  backend_->AddOrUpdateMemoryBankEntry(entry, save_future.GetCallback());
  ASSERT_TRUE(save_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  backend_->GetAllMemoryBankEntries(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(MemoryBankType::kTextSelection, entries[0].type);

  base::test::TestFuture<bool> delete_future;
  backend_->DeleteMemoryBankEntries({entries[0].id},
                                    delete_future.GetCallback());
  ASSERT_TRUE(delete_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> empty_future;
  backend_->GetAllMemoryBankEntries(empty_future.GetCallback());
  EXPECT_TRUE(empty_future.Get().empty());
}

TEST_F(ContextHubBackendImplTest, OperationsQueuedBeforeInit) {
  base::FilePath db_path = temp_dir_.GetPath().AppendASCII("QueuedTest.db");
  auto backend = std::make_unique<ContextHubBackendImpl>(db_path);

  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://queued.com");
  entry.tab_title = "Queued Tab";

  // Queue add, delete, and get operations before running task environment.
  base::test::TestFuture<bool> save_future;
  backend->AddOrUpdateMemoryBankEntry(entry, save_future.GetCallback());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  backend->GetAllMemoryBankEntries(get_future.GetCallback());

  // Wait for queued operations to process once initialization completes.
  ASSERT_TRUE(save_future.Get());

  auto entries = get_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GURL("https://queued.com"), entries[0].url);

  // Queue delete operation.
  base::test::TestFuture<bool> delete_future;
  backend->DeleteMemoryBankEntries({entries[0].id}, delete_future.GetCallback());
  ASSERT_TRUE(delete_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> empty_future;
  backend->GetAllMemoryBankEntries(empty_future.GetCallback());
  EXPECT_TRUE(empty_future.Get().empty());
}

TEST_F(ContextHubBackendImplTest, GetMemoryBankEntriesByIds) {
  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry1.url = GURL("https://example.com/1");
  entry1.tab_title = "Tab 1";

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  entry2.url = GURL("https://example.com/2");
  entry2.tab_title = "Tab 2";

  base::test::TestFuture<bool> save_future1;
  backend_->AddOrUpdateMemoryBankEntry(entry1, save_future1.GetCallback());
  ASSERT_TRUE(save_future1.Get());

  base::test::TestFuture<bool> save_future2;
  backend_->AddOrUpdateMemoryBankEntry(entry2, save_future2.GetCallback());
  ASSERT_TRUE(save_future2.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> all_future;
  backend_->GetAllMemoryBankEntries(all_future.GetCallback());
  auto all_entries = all_future.Get();
  ASSERT_EQ(2u, all_entries.size());

  base::test::TestFuture<std::vector<MemoryBankEntry>> by_ids_future;
  backend_->GetMemoryBankEntriesByIds({all_entries[0].id},
                                       by_ids_future.GetCallback());
  auto selected_entries = by_ids_future.Get();
  ASSERT_EQ(1u, selected_entries.size());
  EXPECT_EQ(all_entries[0].id, selected_entries[0].id);
  EXPECT_EQ(all_entries[0].tab_title, selected_entries[0].tab_title);
}

}  // namespace context_hub
