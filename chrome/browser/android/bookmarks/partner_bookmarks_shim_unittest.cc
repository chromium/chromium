// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using bookmarks::BookmarkNode;
using testing::_;

class MockObserver : public PartnerBookmarksShim::Observer {
 public:
  MockObserver() {}

  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;

  MOCK_METHOD1(PartnerShimChanged, void(PartnerBookmarksShim*));
  MOCK_METHOD1(PartnerShimLoaded, void(PartnerBookmarksShim*));
};

class PartnerBookmarksShimTest : public testing::Test {
 public:
  PartnerBookmarksShimTest() = default;

  PartnerBookmarksShimTest(const PartnerBookmarksShimTest&) = delete;
  PartnerBookmarksShimTest& operator=(const PartnerBookmarksShimTest&) = delete;

  PartnerBookmarksShim* partner_bookmarks_shim() const {
    return PartnerBookmarksShim::BuildForBrowserContext(profile_.get());
  }

 protected:
  // testing::Test
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
  }

  void TearDown() override {
    PartnerBookmarksShim::ClearInBrowserContextForTesting(profile_.get());
    PartnerBookmarksShim::ClearPartnerModelForTesting();
    PartnerBookmarksShim::EnablePartnerBookmarksEditing();
    profile_.reset(NULL);
  }

  std::unique_ptr<TestingProfile> profile_;

  content::BrowserTaskEnvironment task_environment_;

  MockObserver observer_;
};

TEST_F(PartnerBookmarksShimTest, GetNodeByID) {
  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
  BookmarkNode* root_partner_node_ptr = root_partner_node.get();
  BookmarkNode* partner_folder1 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          1, base::Uuid::GenerateRandomV4(), GURL()));

  BookmarkNode* partner_folder2 =
      partner_folder1->Add(std::make_unique<BookmarkNode>(
          2, base::Uuid::GenerateRandomV4(), GURL()));

  partner_folder1->Add(std::make_unique<BookmarkNode>(
      3, base::Uuid::GenerateRandomV4(), GURL("http://www.a.com")));

  BookmarkNode* partner_bookmark2 =
      partner_folder2->Add(std::make_unique<BookmarkNode>(
          4, base::Uuid::GenerateRandomV4(), GURL("http://www.b.com")));

  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  ASSERT_FALSE(shim->IsLoaded());
  shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
  ASSERT_TRUE(shim->IsLoaded());

  ASSERT_TRUE(shim->IsPartnerBookmark(root_partner_node_ptr));
  ASSERT_EQ(shim->GetNodeByID(0), root_partner_node_ptr);
  ASSERT_EQ(shim->GetNodeByID(1), partner_folder1);
  ASSERT_EQ(shim->GetNodeByID(4), partner_bookmark2);
}

TEST_F(PartnerBookmarksShimTest, ObserverNotifiedOfLoadNoPartnerBookmarks) {
  EXPECT_CALL(observer_, PartnerShimLoaded(_)).Times(0);
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(NULL);
}

TEST_F(PartnerBookmarksShimTest, ObserverNotifiedOfLoadWithPartnerBookmarks) {
  EXPECT_CALL(observer_, PartnerShimLoaded(_)).Times(0);
  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();

  int64_t id = 5;
  root_partner_node->Add(std::make_unique<BookmarkNode>(
      id++, base::Uuid::GenerateRandomV4(), GURL("http://www.a.com")));

  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
}

TEST_F(PartnerBookmarksShimTest, RemoveBookmarks) {
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
  BookmarkNode* root_partner_node_ptr = root_partner_node.get();
  root_partner_node->SetTitle(u"Partner bookmarks");

  BookmarkNode* partner_folder1 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          1, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder1->SetTitle(u"a.net");

  BookmarkNode* partner_folder2 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          2, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder2->SetTitle(u"b.net");

  BookmarkNode* partner_bookmark1 =
      partner_folder1->Add(std::make_unique<BookmarkNode>(
          3, base::Uuid::GenerateRandomV4(), GURL("http://www.a.com")));
  partner_bookmark1->SetTitle(u"a.com");

  BookmarkNode* partner_bookmark2 =
      partner_folder2->Add(std::make_unique<BookmarkNode>(
          4, base::Uuid::GenerateRandomV4(), GURL("http://www.b.com")));
  partner_bookmark2->SetTitle(u"b.com");

  BookmarkNode* partner_folder3 =
      partner_folder2->Add(std::make_unique<BookmarkNode>(
          5, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder3->SetTitle(u"c.net");

  BookmarkNode* partner_bookmark3 =
      partner_folder3->Add(std::make_unique<BookmarkNode>(
          6, base::Uuid::GenerateRandomV4(), GURL("http://www.c.com")));
  partner_bookmark3->SetTitle(u"c.com");

  ASSERT_FALSE(shim->IsLoaded());
  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
  ASSERT_TRUE(shim->IsLoaded());

  EXPECT_EQ(root_partner_node_ptr, shim->GetNodeByID(0));
  EXPECT_EQ(partner_folder1, shim->GetNodeByID(1));
  EXPECT_EQ(partner_folder2, shim->GetNodeByID(2));
  EXPECT_EQ(partner_bookmark1, shim->GetNodeByID(3));
  EXPECT_EQ(partner_bookmark2, shim->GetNodeByID(4));
  EXPECT_EQ(partner_folder3, shim->GetNodeByID(5));
  EXPECT_EQ(partner_bookmark3, shim->GetNodeByID(6));

  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));
  EXPECT_TRUE(shim->IsReachable(partner_folder3));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark3));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  shim->RemoveBookmark(partner_bookmark2);
  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  EXPECT_TRUE(shim->IsReachable(partner_folder3));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark3));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  shim->RemoveBookmark(partner_folder1);
  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_FALSE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  EXPECT_TRUE(shim->IsReachable(partner_folder3));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark3));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  shim->RemoveBookmark(root_partner_node_ptr);
  EXPECT_FALSE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_FALSE(shim->IsReachable(partner_folder1));
  EXPECT_FALSE(shim->IsReachable(partner_folder2));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  EXPECT_FALSE(shim->IsReachable(partner_folder3));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark3));
}

TEST_F(PartnerBookmarksShimTest, RenameBookmarks) {
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
  BookmarkNode* root_partner_node_ptr = root_partner_node.get();
  root_partner_node->SetTitle(u"Partner bookmarks");

  BookmarkNode* partner_folder1 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          1, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder1->SetTitle(u"a.net");

  BookmarkNode* partner_folder2 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          2, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder2->SetTitle(u"b.net");

  BookmarkNode* partner_bookmark1 =
      partner_folder1->Add(std::make_unique<BookmarkNode>(
          3, base::Uuid::GenerateRandomV4(), GURL("http://www.a.com")));
  partner_bookmark1->SetTitle(u"a.com");

  BookmarkNode* partner_bookmark2 =
      partner_folder2->Add(std::make_unique<BookmarkNode>(
          4, base::Uuid::GenerateRandomV4(), GURL("http://www.b.com")));
  partner_bookmark2->SetTitle(u"b.com");

  ASSERT_FALSE(shim->IsLoaded());
  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
  ASSERT_TRUE(shim->IsLoaded());

  EXPECT_EQ(root_partner_node_ptr, shim->GetNodeByID(0));
  EXPECT_EQ(partner_folder1, shim->GetNodeByID(1));
  EXPECT_EQ(partner_folder2, shim->GetNodeByID(2));
  EXPECT_EQ(partner_bookmark1, shim->GetNodeByID(3));
  EXPECT_EQ(partner_bookmark2, shim->GetNodeByID(4));

  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  EXPECT_EQ(u"b.com", shim->GetTitle(partner_bookmark2));
  shim->RenameBookmark(partner_bookmark2, u"b2.com");
  EXPECT_EQ(u"b2.com", shim->GetTitle(partner_bookmark2));

  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  EXPECT_EQ(u"a.net", shim->GetTitle(partner_folder1));
  shim->RenameBookmark(partner_folder1, u"a2.net");
  EXPECT_EQ(u"a2.net", shim->GetTitle(partner_folder1));

  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));

  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(1);
  EXPECT_EQ(u"Partner bookmarks", shim->GetTitle(root_partner_node_ptr));
  shim->RenameBookmark(root_partner_node_ptr, u"Partner");
  EXPECT_EQ(u"Partner", shim->GetTitle(root_partner_node_ptr));

  EXPECT_TRUE(shim->IsReachable(root_partner_node_ptr));
  EXPECT_TRUE(shim->IsReachable(partner_folder1));
  EXPECT_TRUE(shim->IsReachable(partner_folder2));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));
}

TEST_F(PartnerBookmarksShimTest, SaveLoadProfile) {
  {
    PartnerBookmarksShim* shim = partner_bookmarks_shim();
    shim->AddObserver(&observer_);

    EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
    EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

    std::unique_ptr<BookmarkNode> root_partner_node =
        PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
    root_partner_node->SetTitle(u"Partner bookmarks");

    BookmarkNode* partner_folder1 =
        root_partner_node->Add(std::make_unique<BookmarkNode>(
            1, base::Uuid::GenerateRandomV4(), GURL()));
    partner_folder1->SetTitle(u"a.net");

    BookmarkNode* partner_bookmark1 =
        partner_folder1->Add(std::make_unique<BookmarkNode>(
            3, base::Uuid::GenerateRandomV4(), GURL("http://a.com")));
    partner_bookmark1->SetTitle(u"a.com");

    BookmarkNode* partner_bookmark2 =
        partner_folder1->Add(std::make_unique<BookmarkNode>(
            5, base::Uuid::GenerateRandomV4(), GURL("http://b.com")));
    partner_bookmark2->SetTitle(u"b.com");

    ASSERT_FALSE(shim->IsLoaded());
    EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
    shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
    ASSERT_TRUE(shim->IsLoaded());

    EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(2);
    shim->RenameBookmark(partner_bookmark1, u"a2.com");
    shim->RemoveBookmark(partner_bookmark2);
    EXPECT_EQ(u"a2.com", shim->GetTitle(partner_bookmark1));
    EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  }

  PartnerBookmarksShim::ClearInBrowserContextForTesting(profile_.get());

  {
    PartnerBookmarksShim* shim = partner_bookmarks_shim();
    shim->AddObserver(&observer_);

    EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
    EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);
    ASSERT_TRUE(shim->IsLoaded());

    const BookmarkNode* partner_bookmark1 = shim->GetNodeByID(3);
    const BookmarkNode* partner_bookmark2 = shim->GetNodeByID(5);

    EXPECT_EQ(u"a2.com", shim->GetTitle(partner_bookmark1));
    EXPECT_FALSE(shim->IsReachable(partner_bookmark2));
  }
}

TEST_F(PartnerBookmarksShimTest, DisableEditing) {
  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  shim->AddObserver(&observer_);

  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(0);
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(0);

  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
  root_partner_node->SetTitle(u"Partner bookmarks");

  BookmarkNode* partner_bookmark1 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          3, base::Uuid::GenerateRandomV4(), GURL("http://a")));
  partner_bookmark1->SetTitle(u"a");

  BookmarkNode* partner_bookmark2 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          3, base::Uuid::GenerateRandomV4(), GURL("http://b")));
  partner_bookmark2->SetTitle(u"b");

  ASSERT_FALSE(shim->IsLoaded());
  EXPECT_CALL(observer_, PartnerShimLoaded(shim)).Times(1);
  shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
  ASSERT_TRUE(shim->IsLoaded());

  // Check that edits work by default.
  EXPECT_CALL(observer_, PartnerShimChanged(shim)).Times(2);
  shim->RenameBookmark(partner_bookmark1, u"a2.com");
  shim->RemoveBookmark(partner_bookmark2);
  EXPECT_EQ(u"a2.com", shim->GetTitle(partner_bookmark1));
  EXPECT_FALSE(shim->IsReachable(partner_bookmark2));

  // Disable edits and check that edits are not applied anymore.
  PartnerBookmarksShim::DisablePartnerBookmarksEditing();
  EXPECT_EQ(u"a", shim->GetTitle(partner_bookmark1));
  EXPECT_TRUE(shim->IsReachable(partner_bookmark2));
}

TEST_F(PartnerBookmarksShimTest, GetPartnerBookmarksMatchingProperties) {
  std::unique_ptr<BookmarkNode> root_partner_node =
      PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting();
  BookmarkNode* partner_folder1 =
      root_partner_node->Add(std::make_unique<BookmarkNode>(
          1, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder1->SetTitle(u"Folder1");

  BookmarkNode* partner_folder2 =
      partner_folder1->Add(std::make_unique<BookmarkNode>(
          2, base::Uuid::GenerateRandomV4(), GURL()));
  partner_folder2->SetTitle(u"Folder2");

  BookmarkNode* partner_bookmark1 =
      partner_folder1->Add(std::make_unique<BookmarkNode>(
          3, base::Uuid::GenerateRandomV4(), GURL("http://www.ugtdat.com")));
  partner_bookmark1->SetTitle(u"wx");

  BookmarkNode* partner_bookmark2 =
      partner_folder2->Add(std::make_unique<BookmarkNode>(
          4, base::Uuid::GenerateRandomV4(), GURL("http://argbhl.com")));
  partner_bookmark2->SetTitle(u"wx yz");

  PartnerBookmarksShim* shim = partner_bookmarks_shim();
  ASSERT_FALSE(shim->IsLoaded());
  shim->SetPartnerBookmarksRoot(std::move(root_partner_node));
  ASSERT_TRUE(shim->IsLoaded());

  // Ensure that search returns case-insensitive matches for title only.
  std::vector<const BookmarkNode*> nodes;
  bookmarks::QueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>(u"WX");
  shim->GetPartnerBookmarksMatchingProperties(query, 100, &nodes);
  ASSERT_EQ(2u, nodes.size());
  ASSERT_EQ(partner_bookmark1, nodes[1]);
  ASSERT_EQ(partner_bookmark2, nodes[0]);

  // Ensure that every word in the search must have a match.
  nodes.clear();
  query.word_phrase_query = std::make_unique<std::u16string>(u"WX Y");
  shim->GetPartnerBookmarksMatchingProperties(query, 100, &nodes);
  ASSERT_EQ(1u, nodes.size());
  ASSERT_EQ(partner_bookmark2, nodes[0]);

  // Ensure that search returns matches for URL only.
  nodes.clear();
  query.word_phrase_query = std::make_unique<std::u16string>(u"dat.com");
  shim->GetPartnerBookmarksMatchingProperties(query, 100, &nodes);
  ASSERT_EQ(1u, nodes.size());
  ASSERT_EQ(partner_bookmark1, nodes[0]);

  // Ensure that folders appear in search results, and that max_count is
  // effective.
  nodes.clear();
  query.word_phrase_query = std::make_unique<std::u16string>(u"folder");

  shim->GetPartnerBookmarksMatchingProperties(query, 100, &nodes);
  ASSERT_EQ(2u, nodes.size());

  nodes.clear();
  shim->GetPartnerBookmarksMatchingProperties(query, 1, &nodes);
  ASSERT_EQ(1u, nodes.size());

  nodes.clear();
  shim->GetPartnerBookmarksMatchingProperties(query, 0, &nodes);
  ASSERT_EQ(0u, nodes.size());

  // Test a scenario with no search results.
  nodes.clear();
  query.word_phrase_query = std::make_unique<std::u16string>(u"foo.com");
  shim->GetPartnerBookmarksMatchingProperties(query, 100, &nodes);
  ASSERT_EQ(0u, nodes.size());
}
