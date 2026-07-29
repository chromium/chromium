// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/database_memory_bank.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/storage/context_hub_backend_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

class DatabaseMemoryBankTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_path =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ContextHub.db"));
    backend_ = std::make_unique<ContextHubBackendImpl>(db_path);
    memory_bank_ = std::make_unique<DatabaseMemoryBank>(*backend_);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ContextHubBackendImpl> backend_;
  std::unique_ptr<DatabaseMemoryBank> memory_bank_;
};

TEST_F(DatabaseMemoryBankTest, SaveTabAndRetrieve) {
  base::test::TestFuture<void> save_future;
  memory_bank_->SaveTab(GURL("https://example.com"), "Example", "Page content",
                        save_future.GetCallback());
  EXPECT_TRUE(save_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  memory_bank_->GetAllEntries(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(GURL("https://example.com"), entries[0].url);
  EXPECT_EQ("Example", entries[0].tab_title);
  ASSERT_TRUE(entries[0].selected_text.has_value());
  EXPECT_EQ("Page content", entries[0].selected_text.value());
}

TEST_F(DatabaseMemoryBankTest, SaveTextSelectionAndDelete) {
  base::test::TestFuture<void> save_future;
  memory_bank_->SaveTextSelection(GURL("https://google.com"), "Google",
                                  "Search text", save_future.GetCallback());
  EXPECT_TRUE(save_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> get_future;
  memory_bank_->GetAllEntries(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(MemoryBankType::kTextSelection, entries[0].type);

  base::test::TestFuture<void> delete_future;
  std::vector<int64_t> ids = {entries[0].id};
  memory_bank_->DeleteEntries(ids, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> empty_future;
  memory_bank_->GetAllEntries(empty_future.GetCallback());
  EXPECT_TRUE(empty_future.Get().empty());
}

TEST_F(DatabaseMemoryBankTest, GetEntriesByIds) {
  base::test::TestFuture<void> save_future1;
  memory_bank_->SaveTab(GURL("https://example.com/1"), "Tab 1", "Content 1",
                        save_future1.GetCallback());
  EXPECT_TRUE(save_future1.Wait());

  base::test::TestFuture<void> save_future2;
  memory_bank_->SaveTab(GURL("https://example.com/2"), "Tab 2", "Content 2",
                        save_future2.GetCallback());
  EXPECT_TRUE(save_future2.Wait());

  base::test::TestFuture<std::vector<MemoryBankEntry>> all_future;
  memory_bank_->GetAllEntries(all_future.GetCallback());
  auto all_entries = all_future.Get();
  ASSERT_EQ(2u, all_entries.size());

  base::test::TestFuture<std::vector<MemoryBankEntry>> by_ids_future;
  memory_bank_->GetEntriesByIds({all_entries[0].id},
                                 by_ids_future.GetCallback());
  auto selected_entries = by_ids_future.Get();
  ASSERT_EQ(1u, selected_entries.size());
  EXPECT_EQ(all_entries[0].id, selected_entries[0].id);
  EXPECT_EQ(all_entries[0].tab_title, selected_entries[0].tab_title);
}

}  // namespace context_hub
