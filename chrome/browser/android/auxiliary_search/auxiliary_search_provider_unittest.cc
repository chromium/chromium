// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/auxiliary_search/proto/auxiliary_search_group.pb.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserContext;

class AuxiliarySearchProviderTest : public ::testing::Test {
 public:
  AuxiliarySearchProviderTest();

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    provider = std::make_unique<AuxiliarySearchProvider>(profile_.get());
  }

  void TearDown() override { profile_.reset(nullptr); }

 protected:
  std::unique_ptr<AuxiliarySearchProvider> provider;
  std::unique_ptr<BookmarkModel> model_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

AuxiliarySearchProviderTest::AuxiliarySearchProviderTest() {
  model_ = bookmarks::TestBookmarkClient::CreateModel();
}

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

TEST_F(AuxiliarySearchProviderTest, QueryBookmarks) {
  auxiliary_search::AuxiliarySearchBookmarkGroup group;
  for (int i = 0; i < 200; i++) {
    std::string number = base::NumberToString(i);
    BookmarkNode* node = AsMutable(model_->AddURL(
        model_->bookmark_bar_node(), i, base::UTF8ToUTF16(number),
        GURL("http://foo.com/" + number)));
    node->set_date_last_used(base::Time::FromTimeT(i));
  }
  provider->GetBookmarks(model_.get(), &group);

  EXPECT_EQ(100, group.bookmark_size());

  std::unordered_set<int> bookmark_titles_int;
  for (int i = 0; i < 100; i++) {
    auxiliary_search::AuxiliarySearchBookmarkGroup_Bookmark bookmark =
        group.bookmark(i);
    int title_int;
    EXPECT_TRUE(base::StringToInt(bookmark.title(), &title_int));

    EXPECT_TRUE(title_int >= 100 && title_int <= 199);
    bookmark_titles_int.insert(title_int);
  }
  EXPECT_EQ(100u, bookmark_titles_int.size());
}
