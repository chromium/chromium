// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/auxiliary_search/auxiliary_search_provider.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/auxiliary_search/proto/auxiliary_search_group.pb.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class AuxiliarySearchProviderTest : public ::testing::Test {
 public:
  AuxiliarySearchProviderTest() = default;

  AuxiliarySearchProviderTest(const AuxiliarySearchProviderTest&) = delete;
  AuxiliarySearchProviderTest& operator=(const AuxiliarySearchProviderTest&) =
      delete;

  ~AuxiliarySearchProviderTest() override = default;

 protected:
  const std::unique_ptr<BookmarkModel> model_ =
      bookmarks::TestBookmarkClient::CreateModel();
  AuxiliarySearchProvider provider_{model_.get()};
};

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

TEST_F(AuxiliarySearchProviderTest, QueryBookmarks) {
  for (int i = 0; i < 200; i++) {
    std::string number = base::NumberToString(i);
    BookmarkNode* node = AsMutable(model_->AddURL(
        model_->bookmark_bar_node(), i, base::UTF8ToUTF16(number),
        GURL("http://foo.com/" + number)));
    node->set_date_last_used(base::Time::FromTimeT(i));
  }
  auxiliary_search::AuxiliarySearchBookmarkGroup group =
      provider_.GetBookmarks();

  EXPECT_EQ(100, group.bookmark_size());

  std::unordered_set<int> bookmark_titles_int;
  for (int i = 0; i < 100; i++) {
    auxiliary_search::AuxiliarySearchEntry bookmark = group.bookmark(i);
    int title_int;

    EXPECT_TRUE(bookmark.has_creation_timestamp());
    EXPECT_TRUE(bookmark.has_last_access_timestamp());
    EXPECT_FALSE(bookmark.has_last_modification_timestamp());
    EXPECT_TRUE(base::StringToInt(bookmark.title(), &title_int));

    EXPECT_TRUE(title_int >= 100 && title_int <= 199);
    bookmark_titles_int.insert(title_int);
  }
  EXPECT_EQ(100u, bookmark_titles_int.size());
}

TEST_F(AuxiliarySearchProviderTest, QueryBookmarks_flagTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      chrome::android::kAuxiliarySearchDonation,
      {{"auxiliary_search_max_donation_bookmark", "150"}});
  auto provider = std::make_unique<AuxiliarySearchProvider>(model_.get());
  for (int i = 0; i < 200; i++) {
    std::string number = base::NumberToString(i);
    BookmarkNode* node = AsMutable(model_->AddURL(
        model_->bookmark_bar_node(), i, base::UTF8ToUTF16(number),
        GURL("http://foo.com/" + number)));
    node->set_date_last_used(base::Time::FromTimeT(i));
  }
  auxiliary_search::AuxiliarySearchBookmarkGroup group =
      provider->GetBookmarks();

  EXPECT_EQ(150, group.bookmark_size());

  std::unordered_set<int> bookmark_titles_int;
  for (int i = 0; i < 150; i++) {
    auxiliary_search::AuxiliarySearchEntry bookmark = group.bookmark(i);
    int title_int;

    EXPECT_TRUE(bookmark.has_creation_timestamp());
    EXPECT_TRUE(bookmark.has_last_access_timestamp());
    EXPECT_FALSE(bookmark.has_last_modification_timestamp());
    EXPECT_TRUE(base::StringToInt(bookmark.title(), &title_int));

    EXPECT_TRUE(title_int >= 50 && title_int <= 199);
    bookmark_titles_int.insert(title_int);
  }
  EXPECT_EQ(150u, bookmark_titles_int.size());
}

TEST_F(AuxiliarySearchProviderTest, QueryBookmarks_nativePageShouldBeFiltered) {
  std::vector<GURL> urls_should_be_filtered = {
      GURL(chrome::kChromeUINativeNewTabURL),
      GURL("content://content_url"),
      GURL("about:about_url"),
      GURL("chrome://chrome_url"),
      GURL("javascript:javascript_url"),
      GURL("file://file_url"),
      GURL("invalidscheme://invalidscheme_url")};

  // Add two normal bookmarks
  BookmarkNode* node = AsMutable(model_->AddURL(model_->bookmark_bar_node(), 0,
                                                u"0", GURL("http://foo.com/")));
  node->set_date_last_used(base::Time::FromTimeT(1));

  node = AsMutable(model_->AddURL(model_->bookmark_bar_node(), 1, u"1",
                                  GURL("https://bar.com/")));
  node->set_date_last_used(base::Time::FromTimeT(2));

  // Add some native page bookmarks
  for (size_t i = 0; i < urls_should_be_filtered.size(); ++i) {
    std::string number = base::NumberToString(i + 2);
    node = AsMutable(model_->AddURL(model_->bookmark_bar_node(), i + 2,
                                    base::UTF8ToUTF16(number),
                                    urls_should_be_filtered.at(i)));
    node->set_date_last_used(base::Time::FromTimeT(i + 2));
  }

  auxiliary_search::AuxiliarySearchBookmarkGroup group =
      provider_.GetBookmarks();

  EXPECT_EQ(2, group.bookmark_size());
  EXPECT_TRUE(group.bookmark(0).has_creation_timestamp());
  EXPECT_TRUE(group.bookmark(0).has_last_access_timestamp());
  EXPECT_FALSE(group.bookmark(0).has_last_modification_timestamp());
  EXPECT_EQ("1", group.bookmark(0).title());
  EXPECT_TRUE(group.bookmark(1).has_creation_timestamp());
  EXPECT_TRUE(group.bookmark(1).has_last_access_timestamp());
  EXPECT_FALSE(group.bookmark(1).has_last_modification_timestamp());
  EXPECT_EQ("0", group.bookmark(1).title());
}
