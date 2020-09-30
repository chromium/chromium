// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using BookmarkNode = bookmarks::BookmarkNode;
using ReadingListEntries = ReadingListModelImpl::ReadingListEntries;

namespace {

constexpr char kURL[] = "https://www.example.com";
constexpr char kTitle[] =
    "In earlier tellings, the dog had a better reputation than the cat, "
    "however the president vetoed it.";
constexpr char kTitle1[] = "boring title.";
constexpr char kReadStatusKey[] = "read_status";
constexpr char kReadStatusRead[] = "true";
constexpr char kReadStatusUnread[] = "false";

class ReadingListManagerImplTest : public testing::Test {
 public:
  ReadingListManagerImplTest() = default;
  ~ReadingListManagerImplTest() override = default;

  void SetUp() override {
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        /*storage_layer=*/nullptr, /*pref_service=*/nullptr, &clock_);
    manager_ =
        std::make_unique<ReadingListManagerImpl>(reading_list_model_.get());
  }

 protected:
  ReadingListManager* manager() { return manager_.get(); }
  ReadingListModelImpl* reading_list_model() {
    return reading_list_model_.get();
  }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  std::unique_ptr<ReadingListManager> manager_;
};

// Verifies the states without any reading list data.
TEST_F(ReadingListManagerImplTest, RootWithEmptyReadingList) {
  const auto* root = manager()->GetRoot();
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_folder());
  EXPECT_EQ(0u, manager()->size());
}

// Verifies load data into reading list model will update |manager_| as well.
TEST_F(ReadingListManagerImplTest, Load) {
  // Load data into reading list model.
  auto entries = std::make_unique<ReadingListEntries>();
  GURL url(kURL);
  entries->emplace(url, ReadingListEntry(url, kTitle, clock()->Now()));
  reading_list_model()->StoreLoaded(std::move(entries));

  const auto* node = manager()->Get(url);
  EXPECT_TRUE(node);
  EXPECT_EQ(url, node->url());
  EXPECT_EQ(1u, manager()->size());
  EXPECT_EQ(1u, manager()->unread_size());
}

// Verifes Add(), Get(), Delete() API in reading list manager.
TEST_F(ReadingListManagerImplTest, AddGetDelete) {
  // Adds a node.
  GURL url(kURL);
  manager()->Add(url, kTitle);
  EXPECT_EQ(1u, manager()->size());
  EXPECT_EQ(1u, manager()->unread_size());
  EXPECT_EQ(1u, manager()->GetRoot()->children().size())
      << "The reading list node should be the child of the root.";

  // Gets the node, and verifies its content.
  const BookmarkNode* node = manager()->Get(url);
  ASSERT_TRUE(node);
  EXPECT_EQ(url, node->url());
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(node->GetTitle()));
  std::string read_status;
  node->GetMetaInfo(kReadStatusKey, &read_status);
  EXPECT_EQ(kReadStatusUnread, read_status)
      << "By default the reading list node is marked as unread.";

  // Deletes the node.
  manager()->Delete(url);
  EXPECT_EQ(0u, manager()->size());
  EXPECT_EQ(0u, manager()->unread_size());
  EXPECT_TRUE(manager()->GetRoot()->children().empty());
}

// Verifies Add() the same URL twice will not invalidate returned pointers, and
// the content is updated.
TEST_F(ReadingListManagerImplTest, AddTwice) {
  // Adds a node.
  GURL url(kURL);
  const auto* node = manager()->Add(url, kTitle);
  const auto* new_node = manager()->Add(url, kTitle1);
  EXPECT_EQ(node, new_node) << "Add same URL shouldn't invalidate pointers.";
  EXPECT_EQ(kTitle1, base::UTF16ToUTF8(node->GetTitle()));
}

// Verifes SetReadStatus() API.
TEST_F(ReadingListManagerImplTest, SetReadStatus) {
  GURL url(kURL);
  manager()->SetReadStatus(url, true);
  EXPECT_EQ(0u, manager()->size());

  // Add a node.
  manager()->Add(url, kTitle);
  manager()->SetReadStatus(url, true);

  // Mark as read.
  const BookmarkNode* node = manager()->Get(url);
  ASSERT_TRUE(node);
  EXPECT_EQ(url, node->url());
  std::string read_status;
  node->GetMetaInfo(kReadStatusKey, &read_status);
  EXPECT_EQ(kReadStatusRead, read_status);
  EXPECT_EQ(0u, manager()->unread_size());

  // Mark as unread.
  manager()->SetReadStatus(url, false);
  node = manager()->Get(url);
  node->GetMetaInfo(kReadStatusKey, &read_status);
  EXPECT_EQ(kReadStatusUnread, read_status);
  EXPECT_EQ(1u, manager()->unread_size());
}

}  // namespace
