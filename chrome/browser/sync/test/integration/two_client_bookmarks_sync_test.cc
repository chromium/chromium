// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/loopback_server/persistent_permanent_entity.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::BookmarkModelMatchesFakeServerChecker;
using bookmarks_helper::BookmarksMatchChecker;
using bookmarks_helper::CheckFaviconExpired;
using bookmarks_helper::CheckHasNoFavicon;
using bookmarks_helper::ContainsDuplicateBookmarks;
using bookmarks_helper::CountAllBookmarks;
using bookmarks_helper::CountBookmarksWithTitlesMatching;
using bookmarks_helper::CountBookmarksWithUrlsMatching;
using bookmarks_helper::CountFoldersWithTitlesMatching;
using bookmarks_helper::CreateFavicon;
using bookmarks_helper::DeleteFaviconMappings;
using bookmarks_helper::ExpireFavicon;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::GetBookmarkModel;
using bookmarks_helper::GetManagedNode;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::GetSyncedBookmarksNode;
using bookmarks_helper::GetUniqueNodeByURL;
using bookmarks_helper::HasNodeWithURL;
using bookmarks_helper::IndexedFolderName;
using bookmarks_helper::IndexedSubfolderName;
using bookmarks_helper::IndexedSubsubfolderName;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;
using bookmarks_helper::IsFolderWithTitle;
using bookmarks_helper::IsFolderWithTitleAndChildren;
using bookmarks_helper::IsFolderWithTitleAndChildrenAre;
using bookmarks_helper::IsUrlBookmarkWithTitleAndUrl;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::ReverseChildOrder;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using bookmarks_helper::SetURL;
using bookmarks_helper::SortChildren;
using testing::Contains;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::NotNull;
using testing::SizeIs;
using testing::UnorderedElementsAreArray;

using BookmarkNodeMatcher = testing::Matcher<std::unique_ptr<BookmarkNode>>;

const char kGenericURL[] = "http://www.host.ext:1234/path/filename";
const char kGenericURLTitle[] = "URL Title";
const char kGenericFolderName[] = "Folder Name";
const char kGenericSubfolderName[] = "Subfolder Name";
const char kValidPassphrase[] = "passphrase!";

class TwoClientBookmarksSyncTest : public SyncTest {
 public:
  TwoClientBookmarksSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientBookmarksSyncTest() override = default;

  void TearDownInProcessBrowserTestFixture() override {
    SyncTest::TearDownInProcessBrowserTestFixture();
    policy_provider_.Shutdown();
  }

 protected:
  // Needs to be deleted after all Profiles are deleted.
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GURL google_url("http://www.google.com");
  ASSERT_NE(nullptr, AddURL(0, "Google", google_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_NE(nullptr, AddURL(1, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  const BookmarkNode* new_folder = AddFolder(0, 2, "New Folder");
  Move(0, GetUniqueNodeByURL(0, google_url), new_folder, 0);
  SetTitle(0, GetBookmarkBarNode(0)->children().front().get(), "Yahoo!!");
  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 1, "CNN",
                            GURL("http://www.cnn.com")));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_NE(nullptr, AddURL(1, "Facebook", GURL("http://www.facebook.com")));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  SortChildren(1, GetBookmarkBarNode(1));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  SetTitle(0, GetUniqueNodeByURL(0, google_url), "Google++");
  SetTitle(1, GetUniqueNodeByURL(1, google_url), "Google--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL initial_url("http://www.google.com");
  const GURL second_url("http://www.google.com/abc");
  const GURL third_url("http://www.google.com/def");
  const std::string title = "Google";

  ASSERT_NE(nullptr, AddURL(0, title, initial_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_NE(nullptr, SetURL(0, GetUniqueNodeByURL(0, initial_url), second_url));
  ASSERT_NE(nullptr, SetURL(1, GetUniqueNodeByURL(1, initial_url), third_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  SetTitle(0, GetBookmarkBarNode(0)->children().front().get(), "Google1");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddFirstFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_NE(nullptr, AddFolder(0, kGenericFolderName));
  EXPECT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitle(kGenericFolderName)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_Add3FoldersInShuffledOrder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_NE(nullptr, AddFolder(0, 0, IndexedFolderName(0)));
  ASSERT_NE(nullptr, AddFolder(0, 1, IndexedFolderName(2)));
  ASSERT_NE(nullptr, AddFolder(0, 1, IndexedFolderName(1)));
  EXPECT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitle(IndexedFolderName(0)),
                          IsFolderWithTitle(IndexedFolderName(1)),
                          IsFolderWithTitle(IndexedFolderName(2))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_AddFirstBMWithoutFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  EXPECT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddFirstBMWithFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  const GURL page_url(kGenericURL);
  const GURL icon_url("http://www.google.com/favicon.ico");
  const gfx::Image favicon = CreateFavicon(SK_ColorWHITE);

  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  SetFavicon(0, bookmark, icon_url, favicon, bookmarks_helper::FROM_UI);
  EXPECT_TRUE(BookmarksMatchChecker().Wait());

  const BookmarkNode* remote_node = GetUniqueNodeByURL(1, page_url);
  ASSERT_THAT(remote_node, NotNull());
  const gfx::Image remote_favicon =
      GetBookmarkModel(1)->GetFavicon(remote_node);
  EXPECT_TRUE(favicon.As1xPNGBytes()->Equals(remote_favicon.As1xPNGBytes()));
}

// Test that the history service logic for not losing the hidpi versions of
// favicons as a result of sync does not result in dropping sync updates.
// In particular, the synced 16x16 favicon bitmap should overwrite 16x16
// favicon bitmaps on all clients. (Though non-16x16 favicon bitmaps
// are unchanged).
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_SetFaviconHiDPI) {
  // Set the supported scale factors to include 2x such that CreateFavicon()
  // creates a favicon with hidpi representations and that methods in the
  // FaviconService request hidpi favicons.
  ui::SetSupportedResourceScaleFactors({ui::k100Percent, ui::k200Percent});

  const GURL page_url(kGenericURL);
  const GURL icon_url1("http://www.google.com/favicon1.ico");
  const GURL icon_url2("http://www.google.com/favicon2.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(nullptr, bookmark0);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  gfx::Image favicon = CreateFavicon(SK_ColorWHITE);
  SetFavicon(0, bookmark0, icon_url1, favicon, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  // BookmarksMatchChecker waits until favicons are the same in both bookmark
  // models. And SetFavicon for the 0th model waits until it is loaded before it
  // returns. This guarantees that the 1st model will have favicon loaded.
  ASSERT_FALSE(GetBookmarkModel(1)->GetFavicon(bookmark1).IsEmpty());
  EXPECT_TRUE(GetBookmarkModel(1)->GetFavicon(bookmark1).As1xPNGBytes()->Equals(
      favicon.As1xPNGBytes()));

  favicon = CreateFavicon(SK_ColorBLUE);
  SetFavicon(1, bookmark1, icon_url1, favicon, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  bookmark0 = GetUniqueNodeByURL(0, page_url);
  EXPECT_FALSE(GetBookmarkModel(0)->GetFavicon(bookmark0).IsEmpty());
  EXPECT_TRUE(GetBookmarkModel(0)->GetFavicon(bookmark0).As1xPNGBytes()->Equals(
      favicon.As1xPNGBytes()));

  favicon = CreateFavicon(SK_ColorGREEN);
  SetFavicon(0, bookmark0, icon_url2, favicon, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_FALSE(GetBookmarkModel(1)->GetFavicon(bookmark1).IsEmpty());
  EXPECT_TRUE(GetBookmarkModel(1)->GetFavicon(bookmark1).As1xPNGBytes()->Equals(
      favicon.As1xPNGBytes()));
}

// Test that if sync does not modify a favicon bitmap's data that it does not
// modify the favicon bitmap's "last updated time" either. This is important
// because the last updated time is used to determine whether a bookmark's
// favicon should be redownloaded when the web when the bookmark is visited.
// If sync prevents the "last updated time" from expiring, the favicon is
// never redownloaded from the web. (http://crbug.com/481414)
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_UpdatingTitleDoesNotUpdateFaviconLastUpdatedTime) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ui::SetSupportedResourceScaleFactors({ui::k100Percent});

  const GURL page_url(kGenericURL);
  const GURL icon_url("http://www.google.com/favicon.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(bookmark0, nullptr);
  gfx::Image favicon0 = CreateFavicon(SK_ColorBLUE);
  ASSERT_FALSE(favicon0.IsEmpty());
  SetFavicon(0, bookmark0, icon_url, favicon0, bookmarks_helper::FROM_UI);

  // Expire the favicon (e.g. as a result of the user "clearing browsing
  // history from the beginning of time")
  ExpireFavicon(0, bookmark0);
  CheckFaviconExpired(0, icon_url);

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Change the bookmark's title for profile 1. Changing the title will cause
  // the bookmark's favicon data to be synced from profile 1 to profile 0 even
  // though the favicon data did not change.
  const std::string kNewTitle = "New Title";
  ASSERT_NE(kNewTitle, kGenericURLTitle);
  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  SetTitle(1, bookmark1, kNewTitle);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(0)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(kNewTitle, page_url)));

  // The favicon for profile 0 should still be expired.
  CheckFaviconExpired(0, icon_url);
}

// When two bookmarks use the same icon URL, both bookmarks use the same row
// in the favicons table in the Favicons database. Test that when the favicon
// is updated for one bookmark it is also updated for the other bookmark. This
// ensures that sync has the most up to date data and prevents sync from
// reverting the newly updated bookmark favicon back to the old favicon.
// crbug.com/485657
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_SetFaviconTwoBookmarksSameIconURL) {
  const GURL page_url1("http://www.google.com/a");
  const GURL page_url2("http://www.google.com/b");
  const GURL icon_url("http://www.google.com/favicon.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* bookmark01 = AddURL(0, kGenericURLTitle, page_url1);
  ASSERT_NE(nullptr, bookmark01);
  const BookmarkNode* bookmark02 = AddURL(0, kGenericURLTitle, page_url2);
  ASSERT_NE(nullptr, bookmark02);

  const gfx::Image favicon0 = CreateFavicon(SK_ColorWHITE);
  SetFavicon(0, bookmark01, icon_url, favicon0, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  const BookmarkNode* bookmark11 = GetUniqueNodeByURL(1, page_url1);
  ASSERT_FALSE(GetBookmarkModel(1)->GetFavicon(bookmark11).IsEmpty());

  // Set |page_url2| with the new (blue) favicon at |icon_url|. The sync favicon
  // for both |page_url1| and |page_url2| should be updated to the blue one.
  const gfx::Image favicon1 = CreateFavicon(SK_ColorBLUE);
  SetFavicon(0, bookmark02, icon_url, favicon1, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_TRUE(GetBookmarkModel(1)
                  ->GetFavicon(bookmark11)
                  .As1xPNGBytes()
                  ->Equals(favicon1.As1xPNGBytes()));
  const BookmarkNode* bookmark12 = GetUniqueNodeByURL(1, page_url1);
  EXPECT_TRUE(GetBookmarkModel(1)
                  ->GetFavicon(bookmark12)
                  .As1xPNGBytes()
                  ->Equals(favicon1.As1xPNGBytes()));

  // Set the title for |page_url1|. This should not revert either of the
  // bookmark favicons back to white.
  const char kNewTitle[] = "New Title";
  ASSERT_STRNE(kGenericURLTitle, kNewTitle);
  SetTitle(1, bookmark11, std::string(kNewTitle));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_THAT(
      GetBookmarkBarNode(0)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, page_url2),
                  IsUrlBookmarkWithTitleAndUrl(kNewTitle, page_url1)));

  EXPECT_TRUE(GetBookmarkModel(0)
                  ->GetFavicon(bookmark01)
                  .As1xPNGBytes()
                  ->Equals(favicon1.As1xPNGBytes()));
  EXPECT_TRUE(GetBookmarkModel(0)
                  ->GetFavicon(bookmark02)
                  .As1xPNGBytes()
                  ->Equals(favicon1.As1xPNGBytes()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_DeleteFavicon) {
  const GURL page_url("http://www.google.com/a");
  const GURL icon_url("http://www.google.com/favicon.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(nullptr, bookmark0);

  SetFavicon(0, bookmark0, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  ASSERT_FALSE(GetBookmarkModel(1)->GetFavicon(bookmark1).IsEmpty());

  DeleteFaviconMappings(0, bookmark0, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Set the title for |page_url|. This should not revert the deletion of
  // favicon mappings.
  const char kNewTitle[] = "New Title";
  ASSERT_STRNE(kGenericURLTitle, kNewTitle);
  SetTitle(1, bookmark1, std::string(kNewTitle));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_THAT(GetBookmarkBarNode(0)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(kNewTitle, page_url)));

  // |page_url| should still have no mapping.
  CheckHasNoFavicon(0, page_url);
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddNonHTTPBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_NE(nullptr,
            AddURL(0, "FTP UR", GURL("ftp://user:password@host:1234/path")));
  ASSERT_NE(nullptr, AddURL(0, "File UR", GURL("file://host/path")));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(
          IsUrlBookmarkWithTitleAndUrl("File UR", GURL("file://host/path")),
          IsUrlBookmarkWithTitleAndUrl(
              "FTP UR", GURL("ftp://user:password@host:1234/path"))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddFirstBMUnderFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  ASSERT_NE(nullptr, AddURL(0, folder, 0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildrenAre(
                  kGenericFolderName, IsUrlBookmarkWithTitleAndUrl(
                                          kGenericURLTitle, kGenericURL))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_AddSeveralBMsUnderBMBarAndOtherBM) {
  const size_t kNumBookmarks = 20;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < kNumBookmarks; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(0, GetOtherNode(0), i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetOtherNode(1)->children(), ElementsAreArray(matchers));
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_AddSeveralBMsAndFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> bookmark_bar_matchers;
  for (size_t i = 0; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
      bookmark_bar_matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
    } else {
      const std::string title = IndexedFolderName(i);
      const BookmarkNode* folder = AddFolder(0, i, title);
      ASSERT_NE(nullptr, folder);
      std::vector<BookmarkNodeMatcher> matchers_in_folder;
      if (base::RandDouble() > 0.4) {
        for (size_t j = 0; j < 20; ++j) {
          const std::string url_title = IndexedURLTitle(j);
          const GURL url = GURL(IndexedURL(j));
          ASSERT_NE(nullptr, AddURL(0, folder, j, url_title, url));
          matchers_in_folder.push_back(
              IsUrlBookmarkWithTitleAndUrl(url_title, url));
        }
      }
      bookmark_bar_matchers.push_back(IsFolderWithTitleAndChildren(
          title, ElementsAreArray(matchers_in_folder)));
    }
  }

  std::vector<BookmarkNodeMatcher> other_matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, GetOtherNode(0), i, title, url));
    other_matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(bookmark_bar_matchers));
  EXPECT_THAT(GetOtherNode(1)->children(), ElementsAreArray(other_matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DuplicateBMWithDifferentURLSameName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL url0 = GURL(IndexedURL(0));
  const GURL url1 = GURL(IndexedURL(1));
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, url0));
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, url1));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, url1),
                  IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, url0)));
}

// Add bookmarks with different name and same URL.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DuplicateBookmarksWithSameURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string title0 = IndexedURLTitle(0);
  const std::string title1 = IndexedURLTitle(1);
  ASSERT_NE(nullptr, AddURL(0, title0, GURL(kGenericURL)));
  ASSERT_NE(nullptr, AddURL(0, title1, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title1, kGenericURL),
                          IsUrlBookmarkWithTitleAndUrl(title0, kGenericURL)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title, kGenericURL)));

  const std::string new_title = IndexedURLTitle(2);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(new_title, kGenericURL)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL url = GURL(IndexedURL(1));
  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, url)));

  const GURL new_url = GURL(IndexedURL(2));
  ASSERT_NE(nullptr, SetURL(0, bookmark, new_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, new_url)));
}

// Renaming the same bookmark name twice.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_TwiceRenamingBookmarkName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title, kGenericURL)));

  const std::string new_title = IndexedURLTitle(2);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(new_title, kGenericURL)));

  SetTitle(0, bookmark, title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title, kGenericURL)));
}

// Renaming the same bookmark URL twice.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_TwiceRenamingBookmarkURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL url = GURL(IndexedURL(1));
  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, url)));

  const GURL new_url = GURL(IndexedURL(2));
  ASSERT_NE(nullptr, SetURL(0, bookmark, new_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, new_url)));

  ASSERT_NE(nullptr, SetURL(0, bookmark, url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, url)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_NE(nullptr, AddURL(0, folder, 0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsFolderWithTitleAndChildrenAre(
          title, IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL))));

  const std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildrenAre(
                  new_title, IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle,
                                                          kGenericURL))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameEmptyBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitle(title)));

  const std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitle(new_title)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_RenameBMFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  const std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_NE(nullptr, folder);
  for (size_t i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      const std::string url_title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, url_title, url));
      matchers_in_folder.push_back(
          IsUrlBookmarkWithTitleAndUrl(url_title, url));
    } else {
      const std::string subfolder_title = IndexedSubfolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, folder, i, subfolder_title));
      matchers_in_folder.push_back(IsFolderWithTitle(subfolder_title));
    }
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  title, ElementsAreArray(matchers_in_folder))));

  const std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  new_title, ElementsAreArray(matchers_in_folder))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_RenameBMFolderThatHasParentAndChildren) {
  const size_t kNumSubfolderUrls = 120;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (size_t i = 1; i < 15; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  const std::string title = IndexedSubfolderName(1);
  const BookmarkNode* subfolder = AddFolder(0, folder, 0, title);
  std::vector<BookmarkNodeMatcher> matchers_in_subfolder;
  for (size_t i = 0; i < kNumSubfolderUrls; ++i) {
    if (base::RandDouble() > 0.15) {
      const std::string url_title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, subfolder, i, url_title, url));
      matchers_in_subfolder.push_back(
          IsUrlBookmarkWithTitleAndUrl(url_title, url));
    } else {
      const std::string subfolder_title = IndexedSubsubfolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, subfolder, i, subfolder_title));
      matchers_in_subfolder.push_back(IsFolderWithTitle(subfolder_title));
    }
  }
  // Insert a |folder| matcher with its |subfolder|.
  matchers.insert(
      matchers.begin(),
      IsFolderWithTitleAndChildrenAre(
          kGenericFolderName,
          IsFolderWithTitleAndChildren(
              title, ElementsAreArray(std::move(matchers_in_subfolder)))));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(std::move(matchers)));

  const std::string new_title = IndexedSubfolderName(2);
  SetTitle(0, subfolder, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      Contains(IsFolderWithTitleAndChildrenAre(
          kGenericFolderName,
          IsFolderWithTitleAndChildren(new_title, SizeIs(kNumSubfolderUrls)))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_RenameBMNameAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL url = GURL(IndexedURL(1));
  const std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title, url)));

  const GURL new_url = GURL(IndexedURL(2));
  const std::string new_title = IndexedURLTitle(2);
  bookmark = SetURL(0, bookmark, new_url);
  ASSERT_NE(nullptr, bookmark);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(new_title, new_url)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DeleteBMEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL)));

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 20; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  Remove(0, GetBookmarkBarNode(0), 0);
  matchers.erase(matchers.begin());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelFirstBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));

  Remove(0, folder, 0);
  matchers.erase(matchers.begin());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelLastBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));

  Remove(0, folder, folder->children().size() - 1);
  matchers.erase(matchers.end() - 1);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelMiddleBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));

  Remove(0, folder, 4);
  matchers.erase(matchers.begin() + 4);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMsUnderBMFoldEmptyFolderAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers))));

  while (!folder->children().empty()) {
    Remove(0, folder, 0);
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsFolderWithTitleAndChildren(kGenericFolderName, IsEmpty())));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelEmptyBMFoldEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_NE(nullptr, AddFolder(0, kGenericFolderName));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitle(kGenericFolderName)));

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelEmptyBMFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  ASSERT_NE(nullptr, AddFolder(0, kGenericFolderName));
  matchers.push_back(IsFolderWithTitle(kGenericFolderName));
  for (size_t i = 1; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
      matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
    } else {
      const std::string title = IndexedFolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, i, title));
      matchers.push_back(IsFolderWithTitle(title));
    }
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  Remove(0, GetBookmarkBarNode(0), 0);
  matchers.erase(matchers.begin());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMFoldWithBMsNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  matchers.push_back(
      IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL));

  const size_t kFolderIndex = 1;
  const BookmarkNode* folder = AddFolder(0, kFolderIndex, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  // |folder| will be added to matchers later when all its children will be
  // added.
  for (size_t i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
      matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
    } else {
      const std::string title = IndexedFolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, i, title));
      matchers.push_back(IsFolderWithTitle(title));
    }
  }

  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  for (size_t i = 0; i < 15; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  matchers.insert(
      matchers.begin() + kFolderIndex,
      IsFolderWithTitleAndChildren(kGenericFolderName,
                                   ElementsAreArray(matchers_in_folder)));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  Remove(0, GetBookmarkBarNode(0), 1);
  matchers.erase(matchers.begin() + kFolderIndex);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMFoldWithBMsAndBMFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  matchers.push_back(
      IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL));

  const size_t kFolderIndex = 1;
  const BookmarkNode* folder = AddFolder(0, kFolderIndex, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  // // |folder| will be added to matchers later when all its children will be
  // added.
  for (size_t i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
      matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
    } else {
      const std::string title = IndexedFolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, i, title));
      matchers.push_back(IsFolderWithTitle(title));
    }
  }

  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  for (size_t i = 0; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
      matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
    } else {
      const std::string title = IndexedSubfolderName(i);
      const BookmarkNode* subfolder = AddFolder(0, folder, i, title);
      ASSERT_NE(nullptr, subfolder);
      std::vector<BookmarkNodeMatcher> matchers_in_subfolder;
      if (base::RandDouble() > 0.3) {
        for (size_t j = 0; j < 10; ++j) {
          if (base::RandDouble() > 0.6) {
            const std::string url_title = IndexedURLTitle(j);
            const GURL url = GURL(IndexedURL(j));
            ASSERT_NE(nullptr, AddURL(0, subfolder, j, url_title, url));
            matchers_in_subfolder.push_back(
                IsUrlBookmarkWithTitleAndUrl(url_title, url));
          } else {
            const std::string subfolder_title = IndexedSubsubfolderName(j);
            ASSERT_NE(nullptr, AddFolder(0, subfolder, j, subfolder_title));
            matchers_in_subfolder.push_back(IsFolderWithTitle(subfolder_title));
          }
        }
      }
      matchers_in_folder.push_back(IsFolderWithTitleAndChildren(
          title, ElementsAreArray(std::move(matchers_in_subfolder))));
    }
  }
  matchers.insert(
      matchers.begin() + kFolderIndex,
      IsFolderWithTitleAndChildren(
          kGenericFolderName, ElementsAreArray(std::move(matchers_in_folder))));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  Remove(0, GetBookmarkBarNode(0), kFolderIndex);
  matchers.erase(matchers.begin() + kFolderIndex);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_DelBMFoldWithParentAndChildrenBMsAndBMFolds) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  // |folder| will be added to matchers later when all its children will be
  // added.
  for (size_t i = 1; i < 11; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }

  std::vector<BookmarkNodeMatcher> matchers_in_subfolder;
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 0, kGenericSubfolderName);
  ASSERT_NE(nullptr, subfolder);
  for (size_t i = 0; i < 30; ++i) {
    if (base::RandDouble() > 0.2) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
      matchers_in_subfolder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
    } else {
      const std::string title = IndexedSubsubfolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, subfolder, i, title));
      matchers_in_subfolder.push_back(IsFolderWithTitle(title));
    }
  }

  matchers.insert(matchers.begin(),
                  IsFolderWithTitleAndChildrenAre(
                      kGenericFolderName,
                      IsFolderWithTitleAndChildren(
                          kGenericSubfolderName,
                          ElementsAreArray(std::move(matchers_in_subfolder)))));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  Remove(0, folder, 0);
  matchers.front() =
      IsFolderWithTitleAndChildren(kGenericFolderName, IsEmpty());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_ReverseTheOrderOfTwoBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL url0 = GURL(IndexedURL(0));
  const GURL url1 = GURL(IndexedURL(1));
  const std::string title0 = IndexedURLTitle(0);
  const std::string title1 = IndexedURLTitle(1);
  const BookmarkNode* bookmark0 = AddURL(0, 0, title0, url0);
  const BookmarkNode* bookmark1 = AddURL(0, 1, title1, url1);
  ASSERT_NE(nullptr, bookmark0);
  ASSERT_NE(nullptr, bookmark1);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title0, url0),
                          IsUrlBookmarkWithTitleAndUrl(title1, url1)));

  Move(0, bookmark0, GetBookmarkBarNode(0), 2);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(title1, url1),
                          IsUrlBookmarkWithTitleAndUrl(title0, url0)));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_ReverseTheOrderOf10BMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers.rbegin(), matchers.rend()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromBMBarToBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  matchers.push_back(
      IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL));

  const size_t kFolderIndex = 1;
  const BookmarkNode* folder = AddFolder(0, kFolderIndex, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  matchers.push_back(IsFolderWithTitle(kGenericFolderName));
  for (size_t i = 2; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    matchers.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  const size_t num_bookmarks_to_move =
      GetBookmarkBarNode(0)->children().size() - 2;
  for (size_t i = 0; i < num_bookmarks_to_move; ++i) {
    Move(0, GetBookmarkBarNode(0)->children()[2].get(), folder, i);
    matchers_in_folder.push_back(matchers[2]);
    matchers.erase(matchers.begin() + 2);

    ASSERT_TRUE(BookmarksMatchChecker().Wait());

    EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
    ASSERT_LE(kFolderIndex, GetBookmarkBarNode(1)->children().size());
    const BookmarkNode* remote_folder =
        GetBookmarkBarNode(1)->children()[kFolderIndex].get();
    EXPECT_THAT(remote_folder->children(),
                ElementsAreArray(matchers_in_folder));
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromBMFoldToBMBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  matchers.push_back(
      IsUrlBookmarkWithTitleAndUrl(kGenericURLTitle, kGenericURL));

  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  matchers.push_back(IsFolderWithTitleAndChildren(
      kGenericFolderName, ElementsAreArray(matchers_in_folder)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  const size_t num_bookmarks_to_move = folder->children().size() - 2;
  for (size_t i = 0; i < num_bookmarks_to_move; ++i) {
    Move(0, folder->children().front().get(), GetBookmarkBarNode(0), i);
    matchers.insert(matchers.begin() + i, matchers_in_folder.front());
    matchers_in_folder.erase(matchers_in_folder.begin());
    // Update matchers for the |folder|.
    matchers.back() = IsFolderWithTitleAndChildren(
        kGenericFolderName, ElementsAreArray(matchers_in_folder));

    ASSERT_TRUE(BookmarksMatchChecker().Wait());
    EXPECT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromParentBMFoldToChildBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (size_t i = 0; i < 3; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }

  std::vector<BookmarkNodeMatcher> matchers_in_subfolder;
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_NE(nullptr, subfolder);
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i + 3);
    const GURL url = GURL(IndexedURL(i + 3));
    ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
    matchers_in_subfolder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  matchers_in_folder.push_back(IsFolderWithTitleAndChildren(
      kGenericSubfolderName, ElementsAreArray(matchers_in_subfolder)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers_in_folder))));

  for (size_t i = 0; i < 3; ++i) {
    const GURL url = GURL(IndexedURL(i));
    Move(0, GetUniqueNodeByURL(0, url), subfolder, i + 10);
    matchers_in_subfolder.push_back(matchers_in_folder[i]);
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Do not use |matchers_in_folder| as it contains moved nodes' matchers.
  EXPECT_THAT(
      GetBookmarkBarNode(1)->children(),
      ElementsAre(IsFolderWithTitleAndChildrenAre(
          kGenericFolderName, IsFolderWithTitleAndChildren(
                                  kGenericSubfolderName,
                                  ElementsAreArray(matchers_in_subfolder)))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_MovingBMsFromChildBMFoldToParentBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (size_t i = 0; i < 3; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }

  std::vector<BookmarkNodeMatcher> matchers_in_subfolder;
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_NE(nullptr, subfolder);
  for (size_t i = 0; i < 5; ++i) {
    const std::string title = IndexedURLTitle(i + 3);
    const GURL url = GURL(IndexedURL(i + 3));
    ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
    matchers_in_subfolder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }

  const size_t subfolder_index = matchers_in_folder.size();
  matchers_in_folder.push_back(IsFolderWithTitleAndChildren(
      kGenericSubfolderName, ElementsAreArray(matchers_in_subfolder)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers_in_folder))));

  for (size_t i = 0; i < 3; ++i) {
    const GURL url = GURL(IndexedURL(i + 3));
    Move(0, GetUniqueNodeByURL(0, url), folder, i + 4);
    matchers_in_folder.push_back(matchers_in_subfolder.front());
    matchers_in_subfolder.erase(matchers_in_subfolder.begin());
  }
  matchers_in_folder[subfolder_index] = IsFolderWithTitleAndChildren(
      kGenericSubfolderName, ElementsAreArray(matchers_in_subfolder));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAre(IsFolderWithTitleAndChildren(
                  kGenericFolderName, ElementsAreArray(matchers_in_folder))));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_HoistBMs10LevelUp) {
  const size_t kNumLevels = 15;

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an extra level to represent an empty folder for the last real level.
  std::vector<std::vector<BookmarkNodeMatcher>> matchers_by_level(kNumLevels +
                                                                  1);

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = nullptr;
  const BookmarkNode* folder_L10 = nullptr;

  for (size_t level = 0; level < kNumLevels; ++level) {
    const size_t num_bookmarks = base::RandInt(0, 9);
    for (size_t i = 0; i < num_bookmarks; ++i) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
      matchers_by_level[level].push_back(
          IsUrlBookmarkWithTitleAndUrl(title, url));
    }
    const std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 0) {
      folder_L0 = folder;
    }
    if (level == 10) {
      folder_L10 = folder;
    }
  }

  std::vector<BookmarkNodeMatcher>& matchers_L11 = matchers_by_level[11];
  for (size_t i = 0; i < 3; ++i) {
    const std::string title = IndexedURLTitle(i + 10);
    const GURL url = GURL(IndexedURL(i + 10));
    ASSERT_NE(nullptr, AddURL(0, folder_L10, i, title, url));

    matchers_L11.insert(matchers_L11.begin() + i,
                        IsUrlBookmarkWithTitleAndUrl(title, url));
  }

  // Add all folders to matchers from all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    matchers_by_level[level].push_back(IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i])));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));

  // Move first 3 bookmarks from |folder_L10| to |folder_L0| which are 11 and 1
  // levels respectively.
  std::vector<BookmarkNodeMatcher>& matchers_L01 = matchers_by_level[1];
  const GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L0,
       folder_L0->children().size());
  matchers_L01.push_back(matchers_L11.front());
  matchers_L11.erase(matchers_L11.begin());

  const GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L0, 0);
  matchers_L01.insert(matchers_L01.begin(), matchers_L11.front());
  matchers_L11.erase(matchers_L11.begin());

  const GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L0, 1);
  matchers_L01.insert(matchers_L01.begin() + 1, matchers_L11.front());
  matchers_L11.erase(matchers_L11.begin());

  // Update all folders to matchers for all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    size_t folder_index = matchers_by_level[level].size() - 1;
    // All folders were added to the end of each level. However for the
    // |folder_L0| one more URL was added in the end.
    if (level == 1) {
      folder_index--;
    }
    ASSERT_LT(folder_index, matchers_by_level[level].size());
    matchers_by_level[level][folder_index] = IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i]));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_SinkBMs10LevelDown) {
  const size_t kNumLevels = 15;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an extra level to represent an empty folder for the last real level.
  std::vector<std::vector<BookmarkNodeMatcher>> matchers_by_level(kNumLevels +
                                                                  1);

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = nullptr;
  const BookmarkNode* folder_L10 = nullptr;
  for (size_t level = 0; level < kNumLevels; ++level) {
    size_t num_bookmarks = base::RandInt(0, 9);
    for (size_t i = 0; i < num_bookmarks; ++i) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
      matchers_by_level[level].push_back(
          IsUrlBookmarkWithTitleAndUrl(title, url));
    }
    const std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 0) {
      folder_L0 = folder;
    }
    if (level == 10) {
      folder_L10 = folder;
    }
  }

  std::vector<BookmarkNodeMatcher>& matchers_L01 = matchers_by_level[1];
  for (size_t i = 0; i < 3; ++i) {
    const std::string title = IndexedURLTitle(i + 10);
    const GURL url = GURL(IndexedURL(i + 10));
    ASSERT_NE(nullptr, AddURL(0, folder_L0, i, title, url));

    matchers_L01.insert(matchers_L01.begin() + i,
                        IsUrlBookmarkWithTitleAndUrl(title, url));
  }

  // Add all folders to matchers from all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    matchers_by_level[level].push_back(IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i])));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));

  // Move first 3 bookmarks from |folder_L0| to |folder_L10| which are 1 and 11
  // levels respectively.
  std::vector<BookmarkNodeMatcher>& matchers_L11 = matchers_by_level[11];
  const GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L10,
       folder_L10->children().size());
  matchers_L11.push_back(matchers_L01.front());
  matchers_L01.erase(matchers_L01.begin());

  const GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L10, 0);
  matchers_L11.insert(matchers_L11.begin(), matchers_L01.front());
  matchers_L01.erase(matchers_L01.begin());

  const GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L10, 1);
  matchers_L11.insert(matchers_L11.begin() + 1, matchers_L01.front());
  matchers_L01.erase(matchers_L01.begin());

  // Update all folders to matchers for all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    size_t folder_index = matchers_by_level[level].size() - 1;
    // All folders were added to the end of each level. However for the
    // |folder_L10| one more URL was added in the end.
    if (level == 11) {
      folder_index--;
    }
    ASSERT_LT(folder_index, matchers_by_level[level].size());
    matchers_by_level[level][folder_index] = IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i]));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_SinkEmptyBMFold5LevelsDown) {
  const size_t kNumLevels = 15;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an extra level to represent an empty folder for the last real level.
  std::vector<std::vector<BookmarkNodeMatcher>> matchers_by_level(kNumLevels +
                                                                  1);

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = nullptr;
  for (size_t level = 0; level < 15; ++level) {
    size_t num_bookmarks = base::RandInt(0, 9);
    for (size_t i = 0; i < num_bookmarks; ++i) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
      matchers_by_level[level].push_back(
          IsUrlBookmarkWithTitleAndUrl(title, url));
    }
    const std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 5) {
      folder_L5 = folder;
    }
  }

  // Add all folders to matchers from all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    matchers_by_level[level].push_back(IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i])));
  }

  folder = AddFolder(0, GetBookmarkBarNode(0)->children().size(),
                     kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  matchers_by_level.front().push_back(IsFolderWithTitle(kGenericFolderName));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));

  Move(0, folder, folder_L5, folder_L5->children().size());
  matchers_by_level[6].push_back(matchers_by_level.front().back());
  matchers_by_level.front().pop_back();

  // Update all folders to matchers for all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    size_t folder_index = matchers_by_level[level].size() - 1;
    // All folders were added to the end of each level. However for the
    // |folder_L5| one more folder was added in the end.
    if (level == 6) {
      folder_index--;
    }
    ASSERT_LT(folder_index, matchers_by_level[level].size());
    matchers_by_level[level][folder_index] = IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i]));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_SinkNonEmptyBMFold5LevelsDown) {
  const size_t kNumLevels = 6;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an extra level to represent an empty folder for the last real level.
  std::vector<std::vector<BookmarkNodeMatcher>> matchers_by_level(kNumLevels +
                                                                  1);

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = nullptr;
  for (size_t level = 0; level < kNumLevels; ++level) {
    const size_t num_bookmarks = base::RandInt(0, 9);
    for (size_t i = 0; i < num_bookmarks; ++i) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
      matchers_by_level[level].push_back(
          IsUrlBookmarkWithTitleAndUrl(title, url));
    }
    const std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 5) {
      folder_L5 = folder;
    }
  }

  // Add all folders to matchers from all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    matchers_by_level[level].push_back(IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i])));
  }

  folder = AddFolder(0, GetBookmarkBarNode(0)->children().size(),
                     kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  matchers_by_level.front().push_back(IsFolderWithTitleAndChildren(
      kGenericFolderName, ElementsAreArray(matchers_in_folder)));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));

  Move(0, folder, folder_L5, folder_L5->children().size());
  matchers_by_level[6].push_back(matchers_by_level.front().back());
  matchers_by_level.front().pop_back();

  // Update all folders to matchers for all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    size_t folder_index = matchers_by_level[level].size() - 1;
    // All folders were added to the end of each level. However for the
    // |folder_L5| one more folder was added in the end.
    if (level == 6) {
      folder_index--;
    }
    ASSERT_LT(folder_index, matchers_by_level[level].size());
    matchers_by_level[level][folder_index] = IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i]));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, SC_HoistFolder5LevelsUp) {
  const size_t kNumLevels = 6;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an extra level to represent an empty folder for the last real level.
  std::vector<std::vector<BookmarkNodeMatcher>> matchers_by_level(kNumLevels +
                                                                  1);

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = nullptr;
  for (size_t level = 0; level < kNumLevels; ++level) {
    size_t num_bookmarks = base::RandInt(0, 9);
    for (size_t i = 0; i < num_bookmarks; ++i) {
      const std::string title = IndexedURLTitle(i);
      const GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
      matchers_by_level[level].push_back(
          IsUrlBookmarkWithTitleAndUrl(title, url));
    }
    const std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 5) {
      folder_L5 = folder;
    }
  }

  folder =
      AddFolder(0, folder_L5, folder_L5->children().size(), kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  std::vector<BookmarkNodeMatcher> matchers_in_folder;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    matchers_in_folder.push_back(IsUrlBookmarkWithTitleAndUrl(title, url));
  }
  matchers_by_level[6].push_back(IsFolderWithTitleAndChildren(
      kGenericFolderName, ElementsAreArray(matchers_in_folder)));

  // Add all folders to matchers from all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    matchers_by_level[level].push_back(IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i])));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));

  Move(0, folder, GetBookmarkBarNode(0),
       GetBookmarkBarNode(0)->children().size());
  matchers_by_level.front().push_back(matchers_by_level[6].back());
  matchers_by_level[6].erase(matchers_by_level[6].end() - 1);

  // Update all folders to matchers for all levels.
  for (size_t i = kNumLevels; i > 0; --i) {
    const size_t level = i - 1;
    DCHECK_LT(i, matchers_by_level.size());

    const std::string folder_title = IndexedFolderName(level);
    size_t folder_index = matchers_by_level[level].size() - 1;
    // All folders were added to the end of each level. However for the
    // bookmark bar node one more folder was added in the end.
    if (level == 0) {
      folder_index--;
    }
    ASSERT_LT(folder_index, matchers_by_level[level].size());
    matchers_by_level[level][folder_index] = IsFolderWithTitleAndChildren(
        folder_title, ElementsAreArray(matchers_by_level[i]));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers_by_level.front()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_ReverseTheOrderOfTwoBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 2; ++i) {
    const std::string title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    std::vector<BookmarkNodeMatcher> matchers_in_folder;
    ASSERT_NE(nullptr, folder);
    for (size_t j = 0; j < 10; ++j) {
      const std::string url_title = IndexedURLTitle(j);
      const GURL url = GURL(IndexedURL(j));
      ASSERT_NE(nullptr, AddURL(0, folder, j, url_title, url));
      matchers_in_folder.push_back(
          IsUrlBookmarkWithTitleAndUrl(url_title, url));
    }
    matchers.push_back(IsFolderWithTitleAndChildren(
        title, ElementsAreArray(std::move(matchers_in_folder))));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers.rbegin(), matchers.rend()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       SC_ReverseTheOrderOfTenBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < 10; ++i) {
    const std::string title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_NE(nullptr, folder);
    std::vector<BookmarkNodeMatcher> matchers_in_folder;
    for (size_t j = 0; j < 10; ++j) {
      const std::string url_title = IndexedURLTitle(1000 * i + j);
      const GURL url = GURL(IndexedURL(j));
      ASSERT_NE(nullptr, AddURL(0, folder, j, url_title, url));
      matchers_in_folder.push_back(
          IsUrlBookmarkWithTitleAndUrl(url_title, url));
    }
    matchers.push_back(IsFolderWithTitleAndChildren(
        title, ElementsAreArray(std::move(matchers_in_folder))));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(1)->children(), ElementsAreArray(matchers));

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(1)->children(),
              ElementsAreArray(matchers.rbegin(), matchers.rend()));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BiDirectionalPushAddingBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  for (size_t i = 0; i < 2; ++i) {
    std::string title0 = IndexedURLTitle(2 * i);
    GURL url0 = GURL(IndexedURL(2 * i));
    ASSERT_NE(nullptr, AddURL(0, title0, url0));
    std::string title1 = IndexedURLTitle(2 * i + 1);
    GURL url1 = GURL(IndexedURL(2 * i + 1));
    ASSERT_NE(nullptr, AddURL(1, title1, url1));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BiDirectionalPush_AddingSameBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Note: When a racy commit is done with identical bookmarks, it is possible
  // for duplicates to exist after sync completes. See http://crbug.com/19769.
  for (size_t i = 0; i < 2; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, title, url));
    ASSERT_NE(nullptr, AddURL(1, title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_CaseInsensitivity_InNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* folder0 = AddFolder(0, "Folder");
  ASSERT_NE(nullptr, folder0);
  ASSERT_NE(nullptr, AddURL(0, folder0, 0, "Bookmark 0", GURL(kGenericURL)));
  ASSERT_NE(nullptr, AddURL(0, folder0, 1, "Bookmark 1", GURL(kGenericURL)));
  ASSERT_NE(nullptr, AddURL(0, folder0, 2, "Bookmark 2", GURL(kGenericURL)));

  const BookmarkNode* folder1 = AddFolder(1, "fOlDeR");
  ASSERT_NE(nullptr, folder1);
  ASSERT_NE(nullptr, AddURL(1, folder1, 0, "bOoKmArK 0", GURL(kGenericURL)));
  ASSERT_NE(nullptr, AddURL(1, folder1, 1, "BooKMarK 1", GURL(kGenericURL)));
  ASSERT_NE(nullptr, AddURL(1, folder1, 2, "bOOKMARK 2", GURL(kGenericURL)));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_SimpleMergeOfDifferentBMModels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  for (size_t i = 3; i < 10; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title0, url0));
    std::string title1 = IndexedURLTitle(i + 7);
    GURL url1 = GURL(IndexedURL(i + 7));
    ASSERT_NE(nullptr, AddURL(1, i, title1, url1));
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchyUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  for (size_t i = 3; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchyEqualSetsUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge bookmark folders with different bookmarks.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeBMFoldersWithDifferentBMs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  const BookmarkNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_NE(nullptr, folder1);
  for (size_t i = 0; i < 2; ++i) {
    std::string title0 = IndexedURLTitle(2 * i);
    GURL url0 = GURL(IndexedURL(2 * i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title0, url0));
    std::string title1 = IndexedURLTitle(2 * i + 1);
    GURL url1 = GURL(IndexedURL(2 * i + 1));
    ASSERT_NE(nullptr, AddURL(1, folder1, i, title1, url1));
  }
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge moderately complex bookmark models.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeDifferentBMModelsModeratelyComplex) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (size_t i = 0; i < 25; ++i) {
    const std::string title0 = IndexedURLTitle(i);
    const GURL url0 = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title0, url0));

    const std::string title1 = IndexedURLTitle(i + 50);
    const GURL url1 = GURL(IndexedURL(i + 50));
    ASSERT_NE(nullptr, AddURL(1, i, title1, url1));
  }
  for (size_t i = 25; i < 30; ++i) {
    std::string title0 = IndexedFolderName(i);
    const BookmarkNode* folder0 = AddFolder(0, i, title0);
    ASSERT_NE(nullptr, folder0);

    std::string title1 = IndexedFolderName(i + 50);
    const BookmarkNode* folder1 = AddFolder(1, i, title1);
    ASSERT_NE(nullptr, folder1);
    for (size_t j = 0; j < 5; ++j) {
      title0 = IndexedURLTitle(i + 5 * j);
      const GURL url0 = GURL(IndexedURL(i + 5 * j));
      ASSERT_NE(nullptr, AddURL(0, folder0, j, title0, url0));

      title1 = IndexedURLTitle(i + 5 * j + 50);
      const GURL url1 = GURL(IndexedURL(i + 5 * j + 50));
      ASSERT_NE(nullptr, AddURL(1, folder1, j, title1, url1));
    }
  }

  // Generate several duplicate URLs which should match during bookmarks model
  // merge.
  for (size_t i = 100; i < 125; ++i) {
    const std::string title = IndexedURLTitle(i);
    const GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, title, url));
    ASSERT_NE(nullptr, AddURL(1, title, url));
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge simple bookmark subset under bookmark folder.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchySubsetUnderBMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 2; ++i) {
    const BookmarkNode* folder = AddFolder(i, kGenericFolderName);
    ASSERT_NE(nullptr, folder);
    for (size_t j = 0; j < 4; ++j) {
      if (base::RandDouble() < 0.5) {
        std::string title = IndexedURLTitle(j);
        GURL url = GURL(IndexedURL(j));
        ASSERT_NE(nullptr, AddURL(i, folder, j, title, url));
      } else {
        std::string title = IndexedFolderName(j);
        ASSERT_NE(nullptr, AddFolder(i, folder, j, title));
      }
    }
  }
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge subsets of bookmark under bookmark bar.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_MergeSimpleBMHierarchySubsetUnderBookmarkBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (size_t i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }

  for (size_t j = 0; j < 2; ++j) {
    std::string title = IndexedURLTitle(j);
    GURL url = GURL(IndexedURL(j));
    ASSERT_NE(nullptr, AddURL(1, j, title, url));
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
  ASSERT_FALSE(ContainsDuplicateBookmarks(1));
}

// Merge simple bookmark hierarchy under bookmark folder.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_SimpleBMHierarchy_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  ASSERT_NE(nullptr,
            AddURL(0, folder0, 0, IndexedURLTitle(1), GURL(IndexedURL(1))));
  ASSERT_NE(nullptr, AddFolder(0, folder0, 1, IndexedSubfolderName(2)));
  ASSERT_NE(nullptr,
            AddURL(0, folder0, 2, IndexedURLTitle(3), GURL(IndexedURL(3))));
  ASSERT_NE(nullptr, AddFolder(0, folder0, 3, IndexedSubfolderName(4)));

  const BookmarkNode* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_NE(nullptr, folder1);
  ASSERT_NE(nullptr, AddFolder(1, folder1, 0, IndexedSubfolderName(0)));
  ASSERT_NE(nullptr, AddFolder(1, folder1, 1, IndexedSubfolderName(2)));
  ASSERT_NE(nullptr,
            AddURL(1, folder1, 2, IndexedURLTitle(3), GURL(IndexedURL(3))));
  ASSERT_NE(nullptr, AddFolder(1, folder1, 3, IndexedSubfolderName(5)));
  ASSERT_NE(nullptr,
            AddURL(1, folder1, 4, IndexedURLTitle(1), GURL(IndexedURL(1))));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge disjoint sets of bookmark hierarchy under bookmark
// folder.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  ASSERT_NE(nullptr,
            AddURL(0, folder0, 0, IndexedURLTitle(1), GURL(IndexedURL(1))));
  ASSERT_NE(nullptr, AddFolder(0, folder0, 1, IndexedSubfolderName(2)));
  ASSERT_NE(nullptr,
            AddURL(0, folder0, 2, IndexedURLTitle(3), GURL(IndexedURL(3))));
  ASSERT_NE(nullptr, AddFolder(0, folder0, 3, IndexedSubfolderName(4)));

  const BookmarkNode* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_NE(nullptr, folder1);
  ASSERT_NE(nullptr, AddFolder(1, folder1, 0, IndexedSubfolderName(5)));
  ASSERT_NE(nullptr, AddFolder(1, folder1, 1, IndexedSubfolderName(6)));
  ASSERT_NE(nullptr,
            AddURL(1, folder1, 2, IndexedURLTitle(7), GURL(IndexedURL(7))));
  ASSERT_NE(nullptr,
            AddURL(1, folder1, 3, IndexedURLTitle(8), GURL(IndexedURL(8))));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge disjoint sets of bookmark hierarchy under bookmark bar.
IN_PROC_BROWSER_TEST_F(
    TwoClientBookmarksSyncTest,
    MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BookmarkBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 1);
    GURL url = GURL(IndexedURL(i + 1));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }

  for (size_t j = 0; j < 3; ++j) {
    std::string title = IndexedURLTitle(j + 4);
    GURL url = GURL(IndexedURL(j + 4));
    ASSERT_NE(nullptr, AddURL(0, j, title, url));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge sets of duplicate bookmarks under bookmark bar.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_Merge_SimpleBMHierarchy_DuplicateBMs_Under_BMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Let's add duplicate set of bookmark {1,2,2,3,3,3,4,4,4,4} to client0.
  int node_index = 0;
  for (size_t i = 1; i < 5; ++i) {
    for (size_t j = 0; j < i; ++j) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, node_index, title, url));
      ++node_index;
    }
  }
  // Let's add a set of bookmarks {1,2,3,4} to client1.
  for (size_t i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i + 1);
    GURL url = GURL(IndexedURL(i + 1));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  for (size_t i = 1; i < 5; ++i) {
    ASSERT_EQ(i, CountBookmarksWithTitlesMatching(1, IndexedURLTitle(i)));
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, DisableBookmarks) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(
      GetClient(1)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));
  ASSERT_NE(nullptr, AddFolder(1, kGenericFolderName));
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(
      GetClient(1)->EnableSyncForType(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_NE(nullptr, AddFolder(0, IndexedFolderName(0)));
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(0, GetSyncService(0),
                                                    GetFakeServer())
                  .Wait());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_NE(nullptr, AddFolder(1, IndexedFolderName(1)));
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForRegisteredDatatypes());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

// Test adding duplicate folder - Both with different BMs underneath.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, MC_DuplicateFolders) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  const BookmarkNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_NE(nullptr, folder1);
  for (size_t i = 0; i < 5; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title0, url0));
    std::string title1 = IndexedURLTitle(i + 5);
    GURL url1 = GURL(IndexedURL(i + 5));
    ASSERT_NE(nullptr, AddURL(1, folder1, i, title1, url1));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, MC_DeleteBookmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(
      GetClient(1)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));

  const GURL bar_url("http://example.com/bar");
  const GURL other_url("http://example.com/other");

  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 0, "bar", bar_url));
  ASSERT_NE(nullptr, AddURL(0, GetOtherNode(0), 0, "other", other_url));

  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(0, GetSyncService(0),
                                                    GetFakeServer())
                  .Wait());

  ASSERT_TRUE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_FALSE(HasNodeWithURL(1, other_url));

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(0, GetSyncService(0),
                                                    GetFakeServer())
                  .Wait());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));

  ASSERT_TRUE(
      GetClient(1)->EnableSyncForType(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_TRUE(HasNodeWithURL(1, other_url));
}

// Test a scenario of updating the name of the same bookmark from two clients at
// the same time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BookmarkNameChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  GURL url(IndexedURL(0));
  SetTitle(0, GetUniqueNodeByURL(0, url), "Title++");
  SetTitle(1, GetUniqueNodeByURL(1, url), "Title--");

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test a scenario of updating the URL of the same bookmark from two clients at
// the same time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_BookmarkURLChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  GURL url(IndexedURL(0));
  ASSERT_TRUE(
      SetURL(0, GetUniqueNodeByURL(0, url), GURL("http://www.google.com/00")));
  ASSERT_TRUE(
      SetURL(1, GetUniqueNodeByURL(1, url), GURL("http://www.google.com/11")));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test a scenario of updating the BM Folder name from two clients at the same
// time.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       MC_FolderNameChangeConflict) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* folderA[2];
  const BookmarkNode* folderB[2];
  const BookmarkNode* folderC[2];

  // Create empty folder A on both clients.
  folderA[0] = AddFolder(0, IndexedFolderName(0));
  ASSERT_NE(nullptr, folderA[0]);
  folderA[1] = AddFolder(1, IndexedFolderName(0));
  ASSERT_NE(nullptr, folderA[1]);

  // Create folder B with bookmarks on both clients.
  folderB[0] = AddFolder(0, IndexedFolderName(1));
  ASSERT_NE(nullptr, folderB[0]);
  folderB[1] = AddFolder(1, IndexedFolderName(1));
  ASSERT_NE(nullptr, folderB[1]);
  for (size_t i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folderB[0], i, title, url));
  }

  // Create folder C with bookmarks and subfolders on both clients.
  folderC[0] = AddFolder(0, IndexedFolderName(2));
  ASSERT_NE(nullptr, folderC[0]);
  folderC[1] = AddFolder(1, IndexedFolderName(2));
  ASSERT_NE(nullptr, folderC[1]);
  for (size_t i = 0; i < 3; ++i) {
    std::string folder_name = IndexedSubfolderName(i);
    const BookmarkNode* subfolder = AddFolder(0, folderC[0], i, folder_name);
    ASSERT_NE(nullptr, subfolder);
    for (size_t j = 0; j < 3; ++j) {
      std::string title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_NE(nullptr, AddURL(0, subfolder, j, title, url));
    }
  }

  ASSERT_EQ(2u, GetBookmarkBarNode(1)->GetIndexOf(folderA[1]));
  ASSERT_EQ(1u, GetBookmarkBarNode(1)->GetIndexOf(folderB[1]));
  ASSERT_EQ(0u, GetBookmarkBarNode(1)->GetIndexOf(folderC[1]));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder A on both clients. We must retrieve the nodes
  // directly from the model as one of them will have been replaced during merge
  // for GUID reassignment.
  SetTitle(0, GetBookmarkBarNode(0)->children()[2].get(), "Folder A++");
  SetTitle(1, GetBookmarkBarNode(1)->children()[2].get(), "Folder A--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder B on both clients. We must retrieve the nodes
  // directly from the model as one of them will have been replaced during merge
  // for GUID reassignment.
  SetTitle(0, GetBookmarkBarNode(0)->children()[1].get(), "Folder B++");
  SetTitle(1, GetBookmarkBarNode(1)->children()[1].get(), "Folder B--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder C on both clients. We must retrieve the nodes
  // directly from the model as one of them will have been replaced during merge
  // for GUID reassignment.
  SetTitle(0, GetBookmarkBarNode(0)->children()[0].get(), "Folder C++");
  SetTitle(1, GetBookmarkBarNode(1)->children()[0].get(), "Folder C--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       FirstClientEnablesEncryptionWithPassSecondChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add initial bookmarks.
  ASSERT_NE(nullptr, AddURL(0, 0, IndexedURLTitle(0), GURL(IndexedURL(0))));
  ASSERT_NE(nullptr, AddURL(0, 1, IndexedURLTitle(1), GURL(IndexedURL(1))));
  ASSERT_NE(nullptr, AddURL(0, 2, IndexedURLTitle(2), GURL(IndexedURL(2))));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Set a passphrase and enable encryption on Client 0. Client 1 will not
  // understand the bookmark updates.
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(
      kValidPassphrase);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->IsPassphraseRequired());

  // Client 0 adds bookmarks between the first two and between the second two.
  ASSERT_NE(nullptr, AddURL(0, 1, IndexedURLTitle(3), GURL(IndexedURL(3))));
  ASSERT_NE(nullptr, AddURL(0, 3, IndexedURLTitle(4), GURL(IndexedURL(4))));
  EXPECT_FALSE(AllModelsMatch());

  // Set the passphrase. Everything should resolve.
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->SetDecryptionPassphrase(
      kValidPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Ensure everything is syncing normally by appending a final bookmark.
  ASSERT_NE(nullptr, AddURL(1, 5, IndexedURLTitle(5), GURL(IndexedURL(5))));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(0)->children(),
              Contains(IsUrlBookmarkWithTitleAndUrl(IndexedURLTitle(5),
                                                    GURL(IndexedURL(5)))));
}

// Deliberately racy rearranging of bookmarks to test that our conflict resolver
// code results in a consistent view across machines (no matter what the final
// order is).
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, RacyPositionChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add initial bookmarks.
  constexpr size_t kNumBookmarks = 5;
  std::vector<BookmarkNodeMatcher> matchers;
  for (size_t i = 0; i < kNumBookmarks; ++i) {
    ASSERT_NE(nullptr, AddURL(0, i, IndexedURLTitle(i), GURL(IndexedURL(i))));
    matchers.push_back(
        IsUrlBookmarkWithTitleAndUrl(IndexedURLTitle(i), GURL(IndexedURL(i))));
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Make changes on client 0.
  for (size_t i = 0; i < kNumBookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    size_t rand_pos =
        static_cast<size_t>(base::RandInt(0, int{kNumBookmarks} - 1));
    DVLOG(1) << "Moving client 0's bookmark " << i << " to position "
             << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  // Make changes on client 1.
  for (size_t i = 0; i < kNumBookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    size_t rand_pos =
        static_cast<size_t>(base::RandInt(0, int{kNumBookmarks} - 1));
    DVLOG(1) << "Moving client 1's bookmark " << i << " to position "
             << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Now make changes to client 1 first.
  for (size_t i = 0; i < kNumBookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    size_t rand_pos =
        static_cast<size_t>(base::RandInt(0, int{kNumBookmarks} - 1));
    DVLOG(1) << "Moving client 1's bookmark " << i << " to position "
             << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  // Make changes on client 0.
  for (size_t i = 0; i < kNumBookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    size_t rand_pos =
        static_cast<size_t>(base::RandInt(0, int{kNumBookmarks} - 1));
    DVLOG(1) << "Moving client 0's bookmark " << i << " to position "
             << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  EXPECT_TRUE(BookmarksMatchChecker().Wait());
  EXPECT_THAT(GetBookmarkBarNode(0)->children(),
              UnorderedElementsAreArray(matchers));
}

// Trigger the server side creation of Synced Bookmarks. Ensure both clients
// remain syncing afterwards. Add bookmarks to the synced bookmarks folder
// and ensure both clients receive the bookmark.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, CreateSyncedBookmarks) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  fake_server_->InjectEntity(syncer::PersistentPermanentEntity::CreateNew(
      syncer::BOOKMARKS, "synced_bookmarks", "Synced Bookmarks",
      "google_chrome_bookmarks"));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Add a bookmark on Client 0 and ensure it syncs over. This will also trigger
  // both clients downloading the new Synced Bookmarks folder.
  ASSERT_NE(nullptr, AddURL(0, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Now add a bookmark within the Synced Bookmarks folder and ensure it syncs
  // over.
  const BookmarkNode* synced_bookmarks = GetSyncedBookmarksNode(0);
  ASSERT_TRUE(synced_bookmarks);
  ASSERT_NE(nullptr, AddURL(0, synced_bookmarks, 0, "Google2",
                            GURL("http://www.google2.com")));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       BookmarkAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Starting state:
  // other_node
  //    -> folder0
  //      -> tier1_a
  //        -> http://mail.google.com
  //        -> http://www.google.com
  //      -> http://news.google.com
  //      -> http://yahoo.com
  //    -> http://www.cnn.com
  // bookmark_bar
  // -> empty_folder
  // -> folder1
  //    -> http://yahoo.com
  // -> http://gmail.com

  const BookmarkNode* folder0 = AddFolder(0, GetOtherNode(0), 0, "folder0");
  const BookmarkNode* tier1_a = AddFolder(0, folder0, 0, "tier1_a");
  ASSERT_NE(nullptr,
            AddURL(0, folder0, 1, "News", GURL("http://news.google.com")));
  ASSERT_NE(nullptr,
            AddURL(0, folder0, 2, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_NE(nullptr,
            AddURL(0, tier1_a, 0, "Gmai", GURL("http://mail.google.com")));
  ASSERT_NE(nullptr,
            AddURL(0, tier1_a, 1, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "CNN", GURL("http://www.cnn.com")));

  ASSERT_TRUE(AddFolder(0, GetBookmarkBarNode(0), 0, "empty_folder"));
  const BookmarkNode* folder1 =
      AddFolder(0, GetBookmarkBarNode(0), 1, "folder1");
  ASSERT_NE(nullptr,
            AddURL(0, folder1, 0, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(
      AddURL(0, GetBookmarkBarNode(0), 2, "Gmai", GURL("http://gmail.com")));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  GetBookmarkModel(0)->RemoveAllUserBookmarks(FROM_HERE);
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Verify other node has no children now.
  EXPECT_TRUE(GetOtherNode(1)->children().empty());
  EXPECT_TRUE(GetBookmarkBarNode(1)->children().empty());
}

// Verifies that managed bookmarks (installed by policy) don't get synced.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, ManagedBookmarks) {
  // Make sure the first Profile has an overridden policy provider.
  policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);

  // Set up sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Verify that there are no managed bookmarks at startup in either profile.
  // The Managed Bookmarks folder should not be visible at this stage.
  const BookmarkNode* managed_node0 = GetManagedNode(0);
  ASSERT_TRUE(managed_node0->children().empty());
  ASSERT_FALSE(managed_node0->IsVisible());
  const BookmarkNode* managed_node1 = GetManagedNode(1);
  ASSERT_TRUE(managed_node1->children().empty());
  ASSERT_FALSE(managed_node1->IsVisible());

  // Verify that the bookmark bar node is empty on both profiles too.
  const BookmarkNode* bar_node0 = GetBookmarkBarNode(0);
  ASSERT_TRUE(bar_node0->children().empty());
  ASSERT_TRUE(bar_node0->IsVisible());
  const BookmarkNode* bar_node1 = GetBookmarkBarNode(1);
  ASSERT_TRUE(bar_node1->children().empty());
  ASSERT_TRUE(bar_node1->IsVisible());

  // Verify that adding a bookmark is observed by the second Profile.
  const GURL google_url("http://www.google.com");
  ASSERT_NE(nullptr, AddURL(0, "Google", google_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_EQ(1u, bar_node0->children().size());
  ASSERT_EQ(1u, bar_node1->children().size());

  // Set the ManagedBookmarks policy for the first Profile,
  // which will add one new managed bookmark.
  base::Value::Dict bookmark;
  bookmark.Set("name", "Managed bookmark");
  bookmark.Set("url", "youtube.com");
  base::Value::List list;
  list.Append(std::move(bookmark));
  policy::PolicyMap policy;
  // TODO(crbug.com/40172729): Migrate PolicyMap::Set() to take a
  // base::Value::Dict.
  policy.Set(policy::key::kManagedBookmarks, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(list)), nullptr);
  policy_provider_.UpdateChromePolicy(policy);
  base::RunLoop().RunUntilIdle();

  // Now add another user bookmark and wait for it to sync.
  ASSERT_NE(nullptr, AddURL(0, "Google 2", google_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_THAT(GetBookmarkBarNode(0)->children(),
              Contains(IsUrlBookmarkWithTitleAndUrl("Google 2", google_url)));

  EXPECT_FALSE(GetSyncService(0)->HasUnrecoverableError());
  EXPECT_FALSE(GetSyncService(1)->HasUnrecoverableError());

  // Verify that the managed bookmark exists in the local model of the first
  // Profile, and has a child node.
  ASSERT_EQ(1u, managed_node0->children().size());
  ASSERT_TRUE(managed_node0->IsVisible());
  EXPECT_EQ(GURL("http://youtube.com/"),
            managed_node0->children().front()->url());

  // Verify that the second Profile didn't get this node.
  ASSERT_EQ(0u, managed_node1->children().size());
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, E2E_ONLY(SanitySetup)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       E2E_ONLY(OneClientAddsBookmark)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // All profiles should sync same bookmarks.
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";
  // For clean profiles, the bookmarks count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new bookmarks count.
  size_t init_bookmarks_count = CountAllBookmarks(0);

  // Add one new bookmark to the first profile.
  ASSERT_NE(nullptr,
            AddURL(0, "Google URL 0", GURL("http://www.google.com/0")));

  // Blocks and waits for bookmarks models in all profiles to match.
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  // Check that total number of bookmarks is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(CountAllBookmarks(i), init_bookmarks_count + 1)
        << "Total bookmark count is wrong.";
  }
}

// TODO(shadi): crbug.com/569213: Enable this as E2E test.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       OneClientAddsFolderAndBookmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // All profiles should sync same bookmarks.
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";

  // Add one new bookmark to the first profile.
  const BookmarkNode* new_folder = AddFolder(0, 0, "Folder 0");
  ASSERT_NE(nullptr, new_folder);
  ASSERT_NE(nullptr, AddURL(0, new_folder, 0, "Google URL 0",
                            GURL("http://www.google.com/0")));

  // Blocks and waits for bookmarks models in all profiles to match.
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  // Check that both profiles have the folder and the bookmark created above.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(1u, CountFoldersWithTitlesMatching(i, "Folder 0"))
        << "Failed to match the folder";
    ASSERT_EQ(
        1u, CountBookmarksWithUrlsMatching(i, GURL("http://www.google.com/0")))
        << "Failed to match the bookmark";
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       E2E_ONLY(TwoClientsAddBookmarks)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  // ALl profiles should sync same bookmarks.
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";
  // For clean profiles, the bookmarks count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new bookmarks count.
  size_t init_bookmarks_count = CountAllBookmarks(0);

  // Add one new bookmark per profile.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_NE(nullptr,
              AddURL(i, base::StringPrintf("Google URL %d", i),
                     GURL(base::StringPrintf("http://www.google.com/%d", i))));
  }

  // Blocks and waits for bookmarks models in all profiles to match.
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Check that total number of bookmarks is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(CountAllBookmarks(i), init_bookmarks_count + num_clients())
        << "Total bookmark count is wrong.";
  }
}

// Verify that a bookmark added on a client with bookmark syncing disabled gets
// synced to a second client once bookmark syncing is re-enabled.
IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest,
                       E2E_ENABLED(AddBookmarkWhileDisabled)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";
  const size_t initial_count = CountAllBookmarks(0);

  // Verify that we can sync. Add a bookmark on the first client and verify it's
  // synced to the second client.
  const std::string url_title = "a happy little url";
  const GURL url("https://example.com");
  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 0, url_title, url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_EQ(initial_count + 1, CountAllBookmarks(0));
  ASSERT_EQ(initial_count + 1, CountAllBookmarks(1));

  // Disable bookmark syncing on the first client, add another bookmark,
  // re-enable bookmark syncing and see that the second bookmark reaches the
  // second client.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));
  const std::string url_title_2 = "another happy little url";
  const GURL url_2("https://example.com/second");
  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 0, url_title_2, url_2));
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_EQ(initial_count + 2, CountAllBookmarks(0));
  ASSERT_EQ(initial_count + 2, CountAllBookmarks(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientBookmarksSyncTest, ReorderChildren) {
  const GURL google_url("http://www.google.com");
  const GURL yahoo_url("http://www.yahoo.com");

  ASSERT_TRUE(SetupClients());

  ASSERT_NE(nullptr, AddURL(/*profile=*/0, /*index=*/0, "Google", google_url));
  ASSERT_NE(nullptr, AddURL(/*profile=*/0, /*index=*/1, "Yahoo", yahoo_url));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_EQ(2U, GetBookmarkBarNode(0)->children().size());
  ASSERT_EQ(2U, GetBookmarkBarNode(1)->children().size());
  ASSERT_EQ(google_url, GetBookmarkBarNode(0)->children().front()->url());
  ASSERT_EQ(google_url, GetBookmarkBarNode(1)->children().front()->url());
  ASSERT_EQ(yahoo_url, GetBookmarkBarNode(0)->children().back()->url());
  ASSERT_EQ(yahoo_url, GetBookmarkBarNode(1)->children().back()->url());

  GetBookmarkModel(0)->ReorderChildren(
      GetBookmarkBarNode(0), {GetBookmarkBarNode(0)->children().back().get(),
                              GetBookmarkBarNode(0)->children().front().get()});
  EXPECT_TRUE(BookmarksMatchChecker().Wait());

  ASSERT_EQ(2U, GetBookmarkBarNode(0)->children().size());
  ASSERT_EQ(2U, GetBookmarkBarNode(1)->children().size());
  EXPECT_EQ(yahoo_url, GetBookmarkBarNode(0)->children().front()->url());
  EXPECT_EQ(yahoo_url, GetBookmarkBarNode(1)->children().front()->url());
  EXPECT_EQ(google_url, GetBookmarkBarNode(0)->children().back()->url());
  EXPECT_EQ(google_url, GetBookmarkBarNode(1)->children().back()->url());
}

}  // namespace
