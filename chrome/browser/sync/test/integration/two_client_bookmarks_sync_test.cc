// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine_impl/loopback_server/persistent_permanent_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::AllModelsMatch;
using bookmarks_helper::AllModelsMatchVerifier;
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
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::RemoveAll;
using bookmarks_helper::ReverseChildOrder;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using bookmarks_helper::SetURL;
using bookmarks_helper::SortChildren;

const char kGenericURL[] = "http://www.host.ext:1234/path/filename";
const char kGenericURLTitle[] = "URL Title";
const char kGenericFolderName[] = "Folder Name";
const char kGenericSubfolderName[] = "Subfolder Name";
const char kValidPassphrase[] = "passphrase!";

class TwoClientBookmarksSyncTest : public SyncTest {
 public:
  TwoClientBookmarksSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientBookmarksSyncTest() override {}

  void TearDownInProcessBrowserTestFixture() override {
    SyncTest::TearDownInProcessBrowserTestFixture();
    policy_provider_.Shutdown();
  }

 protected:
  // Needs to be deleted after all Profiles are deleted.
  policy::MockConfigurationPolicyProvider policy_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientBookmarksSyncTest);
};

// TODO(crbug.com/516866): Merge the two fixtures into one when all tests are
// passing for USS.
class TwoClientBookmarksSyncTestIncludingUssTests
    : public FeatureToggler,
      public TwoClientBookmarksSyncTest {
 public:
  TwoClientBookmarksSyncTestIncludingUssTests()
      : FeatureToggler(switches::kSyncUSSBookmarks) {}
  ~TwoClientBookmarksSyncTestIncludingUssTests() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientBookmarksSyncTestIncludingUssTests);
};

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL google_url("http://www.google.com");
  ASSERT_NE(nullptr, AddURL(0, "Google", google_url));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_NE(nullptr, AddURL(1, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  const BookmarkNode* new_folder = AddFolder(0, 2, "New Folder");
  Move(0, GetUniqueNodeByURL(0, google_url), new_folder, 0);
  SetTitle(0, GetBookmarkBarNode(0)->GetChild(0), "Yahoo!!");
  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 1, "CNN",
                            GURL("http://www.cnn.com")));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  ASSERT_NE(nullptr, AddURL(1, "Facebook", GURL("http://www.facebook.com")));

  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  SortChildren(1, GetBookmarkBarNode(1));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  DisableVerifier();
  SetTitle(0, GetUniqueNodeByURL(0, google_url), "Google++");
  SetTitle(1, GetUniqueNodeByURL(1, google_url), "Google--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL initial_url("http://www.google.com");
  GURL second_url("http://www.google.com/abc");
  GURL third_url("http://www.google.com/def");
  std::string title = "Google";

  ASSERT_NE(nullptr, AddURL(0, title, initial_url));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  DisableVerifier();
  ASSERT_NE(nullptr, SetURL(0, GetUniqueNodeByURL(0, initial_url), second_url));
  ASSERT_NE(nullptr, SetURL(1, GetUniqueNodeByURL(1, initial_url), third_url));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  SetTitle(0, GetBookmarkBarNode(0)->GetChild(0), "Google1");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddFirstFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddFolder(0, kGenericFolderName));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_Add3FoldersInShuffledOrder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddFolder(0, 0, IndexedFolderName(0)));
  ASSERT_NE(nullptr, AddFolder(0, 1, IndexedFolderName(2)));
  ASSERT_NE(nullptr, AddFolder(0, 1, IndexedFolderName(1)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddFirstBMWithoutFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddFirstBMWithFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());
  const GURL page_url(kGenericURL);
  const GURL icon_url("http://www.google.com/favicon.ico");

  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  SetFavicon(0, bookmark, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

// Test that the history service logic for not losing the hidpi versions of
// favicons as a result of sync does not result in dropping sync updates.
// In particular, the synced 16x16 favicon bitmap should overwrite 16x16
// favicon bitmaps on all clients. (Though non-16x16 favicon bitmaps
// are unchanged).
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_SetFaviconHiDPI) {
  // Set the supported scale factors to include 2x such that CreateFavicon()
  // creates a favicon with hidpi representations and that methods in the
  // FaviconService request hidpi favicons.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);

  const GURL page_url(kGenericURL);
  const GURL icon_url1("http://www.google.com/favicon1.ico");
  const GURL icon_url2("http://www.google.com/favicon2.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(nullptr, bookmark0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  SetFavicon(0, bookmark0, icon_url1, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  SetFavicon(1, bookmark1, icon_url1, CreateFavicon(SK_ColorBLUE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  SetFavicon(0, bookmark0, icon_url2, CreateFavicon(SK_ColorGREEN),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

// Test that if sync does not modify a favicon bitmap's data that it does not
// modify the favicon bitmap's "last updated time" either. This is important
// because the last updated time is used to determine whether a bookmark's
// favicon should be redownloaded when the web when the bookmark is visited.
// If sync prevents the "last updated time" from expiring, the favicon is
// never redownloaded from the web. (http://crbug.com/481414)
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_UpdatingTitleDoesNotUpdateFaviconLastUpdatedTime) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  ui::SetSupportedScaleFactors(supported_scale_factors);

  const GURL page_url(kGenericURL);
  const GURL icon_url("http://www.google.com/favicon.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(bookmark0, nullptr);
  gfx::Image favicon0 = CreateFavicon(SK_ColorBLUE);
  ASSERT_FALSE(favicon0.IsEmpty());
  SetFavicon(0, bookmark0, icon_url, favicon0, bookmarks_helper::FROM_UI);

  // Expire the favicon (e.g. as a result of the user "clearing browsing
  // history from the beginning of time")
  ExpireFavicon(0, bookmark0);
  CheckFaviconExpired(0, icon_url);

  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // Change the bookmark's title for profile 1. Changing the title will cause
  // the bookmark's favicon data to be synced from profile 1 to profile 0 even
  // though the favicon data did not change.
  const std::string kNewTitle = "New Title";
  ASSERT_NE(kNewTitle, kGenericURLTitle);
  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  SetTitle(1, bookmark1, kNewTitle);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // The favicon for profile 0 should still be expired.
  CheckFaviconExpired(0, icon_url);
}

// When two bookmarks use the same icon URL, both bookmarks use the same row
// in the favicons table in the Favicons database. Test that when the favicon
// is updated for one bookmark it is also updated for the other bookmark. This
// ensures that sync has the most up to date data and prevents sync from
// reverting the newly updated bookmark favicon back to the old favicon.
// crbug.com/485657
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_SetFaviconTwoBookmarksSameIconURL) {
  const GURL page_url1("http://www.google.com/a");
  const GURL page_url2("http://www.google.com/b");
  const GURL icon_url("http://www.google.com/favicon.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark01 = AddURL(0, kGenericURLTitle, page_url1);
  ASSERT_NE(nullptr, bookmark01);
  const BookmarkNode* bookmark02 = AddURL(0, kGenericURLTitle, page_url2);
  ASSERT_NE(nullptr, bookmark02);

  SetFavicon(0, bookmark01, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // Set |page_url2| with the new (blue) favicon at |icon_url|. The sync favicon
  // for both |page_url1| and |page_url2| should be updated to the blue one.
  SetFavicon(0, bookmark02, icon_url, CreateFavicon(SK_ColorBLUE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // Set the title for |page_url1|. This should not revert either of the
  // bookmark favicons back to white.
  const char kNewTitle[] = "New Title";
  ASSERT_STRNE(kGenericURLTitle, kNewTitle);
  const BookmarkNode* bookmark11 = GetUniqueNodeByURL(1, page_url1);
  SetTitle(1, bookmark11, std::string(kNewTitle));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DeleteFavicon) {
  const GURL page_url("http://www.google.com/a");
  const GURL icon_url("http://www.google.com/favicon.ico");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* bookmark0 = AddURL(0, kGenericURLTitle, page_url);
  ASSERT_NE(nullptr, bookmark0);

  SetFavicon(0, bookmark0, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  DeleteFaviconMappings(0, bookmark0, bookmarks_helper::FROM_UI);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // Set the title for |page_url|. This should not revert the deletion of
  // favicon mappings.
  const char kNewTitle[] = "New Title";
  ASSERT_STRNE(kGenericURLTitle, kNewTitle);
  const BookmarkNode* bookmark1 = GetUniqueNodeByURL(1, page_url);
  SetTitle(1, bookmark1, std::string(kNewTitle));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // |page_url| should still have no mapping.
  CheckHasNoFavicon(0, page_url);
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddNonHTTPBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr,
            AddURL(0, "FTP UR", GURL("ftp://user:password@host:1234/path")));
  ASSERT_NE(nullptr, AddURL(0, "File UR", GURL("file://host/path")));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddFirstBMUnderFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  ASSERT_NE(nullptr, AddURL(0, folder, 0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddSeveralBMsUnderBMBarAndOtherBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 20; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(0, GetOtherNode(0), i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_AddSeveralBMsAndFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
    } else {
      std::string title = IndexedFolderName(i);
      const BookmarkNode* folder = AddFolder(0, i, title);
      ASSERT_NE(nullptr, folder);
      if (base::RandDouble() > 0.4) {
        for (int i = 0; i < 20; ++i) {
          std::string title = IndexedURLTitle(i);
          GURL url = GURL(IndexedURL(i));
          ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
        }
      }
    }
  }
  for (int i = 0; i < 10; i++) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, GetOtherNode(0), i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DuplicateBMWithDifferentURLSameName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, url0));
  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, url1));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

// Add bookmarks with different name and same URL.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DuplicateBookmarksWithSameURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title0 = IndexedURLTitle(0);
  std::string title1 = IndexedURLTitle(1);
  ASSERT_NE(nullptr, AddURL(0, title0, GURL(kGenericURL)));
  ASSERT_NE(nullptr, AddURL(0, title1, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameBMName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  std::string new_title = IndexedURLTitle(2);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameBMURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_NE(nullptr, SetURL(0, bookmark, new_url));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

// Renaming the same bookmark name twice.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_TwiceRenamingBookmarkName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, GURL(kGenericURL));
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  std::string new_title = IndexedURLTitle(2);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  SetTitle(0, bookmark, title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

// Renaming the same bookmark URL twice.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_TwiceRenamingBookmarkURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  const BookmarkNode* bookmark = AddURL(0, kGenericURLTitle, url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_NE(nullptr, SetURL(0, bookmark, new_url));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  ASSERT_NE(nullptr, SetURL(0, bookmark, url));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_NE(nullptr, AddURL(0, folder, 0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameEmptyBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameBMFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const BookmarkNode* folder = AddFolder(0, title);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    } else {
      std::string title = IndexedSubfolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, folder, i, title));
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameBMFolderThatHasParentAndChildren) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 1; i < 15; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }
  std::string title = IndexedSubfolderName(1);
  const BookmarkNode* subfolder = AddFolder(0, folder, 0, title);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
    } else {
      std::string title = IndexedSubsubfolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, subfolder, i, title));
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  std::string new_title = IndexedSubfolderName(2);
  SetTitle(0, subfolder, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_RenameBMNameAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  std::string title = IndexedURLTitle(1);
  const BookmarkNode* bookmark = AddURL(0, title, url);
  ASSERT_NE(nullptr, bookmark);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  GURL new_url = GURL(IndexedURL(2));
  std::string new_title = IndexedURLTitle(2);
  bookmark = SetURL(0, bookmark, new_url);
  ASSERT_NE(nullptr, bookmark);
  SetTitle(0, bookmark, new_title);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DeleteBMEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelBMNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 20; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelFirstBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, folder, 0);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelLastBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, folder, folder->child_count() - 1);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelMiddleBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, folder, 4);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelBMsUnderBMFoldEmptyFolderAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  int child_count = folder->child_count();
  for (int i = 0; i < child_count; ++i) {
    Remove(0, folder, 0);
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelEmptyBMFoldEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddFolder(0, kGenericFolderName));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelEmptyBMFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddFolder(0, kGenericFolderName));
  for (int i = 1; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, i, title));
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelBMFoldWithBMsNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, i, title));
    }
  }
  for (int i = 0; i < 15; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, GetBookmarkBarNode(0), 1);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelBMFoldWithBMsAndBMFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, i, title, url));
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, i, title));
    }
  }
  for (int i = 0; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    } else {
      std::string title = IndexedSubfolderName(i);
      const BookmarkNode* subfolder =
          AddFolder(0, folder, i, title);
      ASSERT_NE(nullptr, subfolder);
      if (base::RandDouble() > 0.3) {
        for (int j = 0; j < 10; ++j) {
          if (base::RandDouble() > 0.6) {
            std::string title = IndexedURLTitle(j);
            GURL url = GURL(IndexedURL(j));
            ASSERT_NE(nullptr, AddURL(0, subfolder, j, title, url));
          } else {
            std::string title = IndexedSubsubfolderName(j);
            ASSERT_NE(nullptr, AddFolder(0, subfolder, j, title));
          }
        }
      }
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, GetBookmarkBarNode(0), 1);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_DelBMFoldWithParentAndChildrenBMsAndBMFolds) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 1; i < 11; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 0, kGenericSubfolderName);
  ASSERT_NE(nullptr, subfolder);
  for (int i = 0; i < 30; ++i) {
    if (base::RandDouble() > 0.2) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
    } else {
      std::string title = IndexedSubsubfolderName(i);
      ASSERT_NE(nullptr, AddFolder(0, subfolder, i, title));
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Remove(0, folder, 0);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_ReverseTheOrderOfTwoBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  std::string title0 = IndexedURLTitle(0);
  std::string title1 = IndexedURLTitle(1);
  const BookmarkNode* bookmark0 = AddURL(0, 0, title0, url0);
  const BookmarkNode* bookmark1 = AddURL(0, 1, title1, url1);
  ASSERT_NE(nullptr, bookmark0);
  ASSERT_NE(nullptr, bookmark1);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Move(0, bookmark0, GetBookmarkBarNode(0), 2);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_ReverseTheOrderOf10BMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_MovingBMsFromBMBarToBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 2; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  int num_bookmarks_to_move =
      GetBookmarkBarNode(0)->child_count() - 2;
  for (int i = 0; i < num_bookmarks_to_move; ++i) {
    Move(
        0, GetBookmarkBarNode(0)->GetChild(2), folder, i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatchVerifier());
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_MovingBMsFromBMFoldToBMBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, kGenericURLTitle, GURL(kGenericURL)));
  const BookmarkNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  int num_bookmarks_to_move = folder->child_count() - 2;
  for (int i = 0; i < num_bookmarks_to_move; ++i) {
    Move(0, folder->GetChild(0), GetBookmarkBarNode(0), i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatchVerifier());
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_MovingBMsFromParentBMFoldToChildBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_NE(nullptr, subfolder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i));
    Move(0, GetUniqueNodeByURL(0, url), subfolder, i + 10);
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_MovingBMsFromChildBMFoldToParentBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  const BookmarkNode* subfolder =
      AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_NE(nullptr, subfolder);
  for (int i = 0; i < 5; ++i) {
    std::string title = IndexedURLTitle(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_NE(nullptr, AddURL(0, subfolder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i + 3));
    Move(0, GetUniqueNodeByURL(0, url), folder, i + 4);
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_HoistBMs10LevelUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = nullptr;
  const BookmarkNode* folder_L10 = nullptr;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 0) folder_L0 = folder;
    if (level == 10) folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 10);
    GURL url = GURL(IndexedURL(i + 10));
    ASSERT_NE(nullptr, AddURL(0, folder_L10, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(
      0, url10), folder_L0, folder_L0->child_count());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L0, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L0, 1);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

// Flaky. http://crbug.com/107744.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_SinkBMs10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L0 = nullptr;
  const BookmarkNode* folder_L10 = nullptr;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 0) folder_L0 = folder;
    if (level == 10) folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 10);
    GURL url = GURL(IndexedURL(i + 10));
    ASSERT_NE(nullptr, AddURL(0, folder_L0, 0, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L10, folder_L10->child_count());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L10, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L10, 1);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_SinkEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = nullptr;
  for (int level = 0; level < 15; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(
        0, folder, folder->child_count(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, GetBookmarkBarNode(0)->child_count(), kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Move(0, folder, folder_L5, folder_L5->child_count());
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_SinkNonEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = nullptr;
  for (int level = 0; level < 6; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, GetBookmarkBarNode(0)->child_count(), kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Move(0, folder, folder_L5, folder_L5->child_count());
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_HoistFolder5LevelsUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const BookmarkNode* folder = GetBookmarkBarNode(0);
  const BookmarkNode* folder_L5 = nullptr;
  for (int level = 0; level < 6; ++level) {
    int num_bookmarks = base::RandInt(0, 9);
    for (int i = 0; i < num_bookmarks; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_NE(nullptr, folder);
    if (level == 5) folder_L5 = folder;
  }
  folder = AddFolder(
      0, folder_L5, folder_L5->child_count(), kGenericFolderName);
  ASSERT_NE(nullptr, folder);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  Move(0, folder, GetBookmarkBarNode(0), GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_ReverseTheOrderOfTwoBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 2; ++i) {
    std::string title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_NE(nullptr, folder);
    for (int j = 0; j < 10; ++j) {
      std::string title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_NE(nullptr, AddURL(0, folder, j, title, url));
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SC_ReverseTheOrderOfTenBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedFolderName(i);
    const BookmarkNode* folder = AddFolder(0, i, title);
    ASSERT_NE(nullptr, folder);
    for (int j = 0; j < 10; ++j) {
      std::string title = IndexedURLTitle(1000 * i + j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_NE(nullptr, AddURL(0, folder, j, title, url));
    }
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  ReverseChildOrder(0, GetBookmarkBarNode(0));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_BiDirectionalPushAddingBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  DisableVerifier();
  for (int i = 0; i < 2; ++i) {
    std::string title0 = IndexedURLTitle(2*i);
    GURL url0 = GURL(IndexedURL(2*i));
    ASSERT_NE(nullptr, AddURL(0, title0, url0));
    std::string title1 = IndexedURLTitle(2*i+1);
    GURL url1 = GURL(IndexedURL(2*i+1));
    ASSERT_NE(nullptr, AddURL(1, title1, url1));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_BiDirectionalPush_AddingSameBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Note: When a racy commit is done with identical bookmarks, it is possible
  // for duplicates to exist after sync completes. See http://crbug.com/19769.
  DisableVerifier();
  for (int i = 0; i < 2; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, title, url));
    ASSERT_NE(nullptr, AddURL(1, title, url));
  }
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_BootStrapEmptyStateEverywhere) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_Merge_CaseInsensitivity_InNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

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

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_SimpleMergeOfDifferentBMModels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  for (int i = 3; i < 10; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title0, url0));
    std::string title1 = IndexedURLTitle(i+7);
    GURL url1 = GURL(IndexedURL(i+7));
    ASSERT_NE(nullptr, AddURL(1, i, title1, url1));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_MergeSimpleBMHierarchyUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  for (int i = 3; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_MergeSimpleBMHierarchyEqualSetsUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge bookmark folders with different bookmarks.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_MergeBMFoldersWithDifferentBMs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  const BookmarkNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_NE(nullptr, folder1);
  for (int i = 0; i < 2; ++i) {
    std::string title0 = IndexedURLTitle(2*i);
    GURL url0 = GURL(IndexedURL(2*i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title0, url0));
    std::string title1 = IndexedURLTitle(2*i+1);
    GURL url1 = GURL(IndexedURL(2*i+1));
    ASSERT_NE(nullptr, AddURL(1, folder1, i, title1, url1));
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge moderately complex bookmark models.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_MergeDifferentBMModelsModeratelyComplex) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 25; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title0, url0));
    std::string title1 = IndexedURLTitle(i+50);
    GURL url1 = GURL(IndexedURL(i+50));
    ASSERT_NE(nullptr, AddURL(1, i, title1, url1));
  }
  for (int i = 25; i < 30; ++i) {
    std::string title0 = IndexedFolderName(i);
    const BookmarkNode* folder0 = AddFolder(0, i, title0);
    ASSERT_NE(nullptr, folder0);
    std::string title1 = IndexedFolderName(i+50);
    const BookmarkNode* folder1 = AddFolder(1, i, title1);
    ASSERT_NE(nullptr, folder1);
    for (int j = 0; j < 5; ++j) {
      std::string title0 = IndexedURLTitle(i+5*j);
      GURL url0 = GURL(IndexedURL(i+5*j));
      ASSERT_NE(nullptr, AddURL(0, folder0, j, title0, url0));
      std::string title1 = IndexedURLTitle(i+5*j+50);
      GURL url1 = GURL(IndexedURL(i+5*j+50));
      ASSERT_NE(nullptr, AddURL(1, folder1, j, title1, url1));
    }
  }
  for (int i = 100; i < 125; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, title, url));
    ASSERT_NE(nullptr, AddURL(1, title, url));
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge simple bookmark subset under bookmark folder.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_MergeSimpleBMHierarchySubsetUnderBMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 2; ++i) {
    const BookmarkNode* folder = AddFolder(i, kGenericFolderName);
    ASSERT_NE(nullptr, folder);
    for (int j = 0; j < 4; ++j) {
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
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge subsets of bookmark under bookmark bar.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_MergeSimpleBMHierarchySubsetUnderBookmarkBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }

  for (int j = 0; j < 2; ++j) {
    std::string title = IndexedURLTitle(j);
    GURL url = GURL(IndexedURL(j));
    ASSERT_NE(nullptr, AddURL(1, j, title, url));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
  ASSERT_FALSE(ContainsDuplicateBookmarks(1));
}

// Merge simple bookmark hierarchy under bookmark folder.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_Merge_SimpleBMHierarchy_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

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

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge disjoint sets of bookmark hierarchy under bookmark
// folder.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

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

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge disjoint sets of bookmark hierarchy under bookmark bar.
IN_PROC_BROWSER_TEST_P(
    TwoClientBookmarksSyncTestIncludingUssTests,
    MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BookmarkBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i+1);
    GURL url = GURL(IndexedURL(i+1));
    ASSERT_NE(nullptr, AddURL(0, i, title, url));
  }

  for (int j = 0; j < 3; ++j) {
    std::string title = IndexedURLTitle(j+4);
    GURL url = GURL(IndexedURL(j+4));
    ASSERT_NE(nullptr, AddURL(0, j, title, url));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Merge sets of duplicate bookmarks under bookmark bar.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_Merge_SimpleBMHierarchy_DuplicateBMs_Under_BMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  // Let's add duplicate set of bookmark {1,2,2,3,3,3,4,4,4,4} to client0.
  int node_index = 0;
  for (int i = 1; i < 5 ; ++i) {
    for (int j = 0; j < i; ++j) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_NE(nullptr, AddURL(0, node_index, title, url));
      ++node_index;
    }
  }
  // Let's add a set of bookmarks {1,2,3,4} to client1.
  for (int i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i+1);
    GURL url = GURL(IndexedURL(i+1));
    ASSERT_NE(nullptr, AddURL(1, i, title, url));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  for (int i = 1; i < 5 ; ++i) {
    ASSERT_EQ(i, CountBookmarksWithTitlesMatching(1, IndexedURLTitle(i)));
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       DisableBookmarks) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_NE(nullptr, AddFolder(1, kGenericFolderName));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_NE(nullptr, AddFolder(0, IndexedFolderName(0)));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_NE(nullptr, AddFolder(1, IndexedFolderName(1)));
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

// Test adding duplicate folder - Both with different BMs underneath.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_DuplicateFolders) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  const BookmarkNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_NE(nullptr, folder1);
  for (int i = 0; i < 5; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title0, url0));
    std::string title1 = IndexedURLTitle(i+5);
    GURL url1 = GURL(IndexedURL(i+5));
    ASSERT_NE(nullptr, AddURL(1, folder1, i, title1, url1));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_DeleteBookmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::BOOKMARKS));

  const GURL bar_url("http://example.com/bar");
  const GURL other_url("http://example.com/other");

  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 0, "bar", bar_url));
  ASSERT_NE(nullptr, AddURL(0, GetOtherNode(0), 0, "other", other_url));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  ASSERT_TRUE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_FALSE(HasNodeWithURL(1, other_url));

  Remove(0, GetBookmarkBarNode(0), 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_TRUE(HasNodeWithURL(1, other_url));
}

// Test a scenario of updating the name of the same bookmark from two clients at
// the same time.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_BookmarkNameChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  SetTitle(0, GetUniqueNodeByURL(0, url), "Title++");
  SetTitle(1, GetUniqueNodeByURL(1, url), "Title--");

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test a scenario of updating the URL of the same bookmark from two clients at
// the same time.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_BookmarkURLChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_NE(nullptr, folder0);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folder0, i, title, url));
  }
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  ASSERT_TRUE(SetURL(
      0, GetUniqueNodeByURL(0, url), GURL("http://www.google.com/00")));
  ASSERT_TRUE(SetURL(
      1, GetUniqueNodeByURL(1, url), GURL("http://www.google.com/11")));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

// Test a scenario of updating the BM Folder name from two clients at the same
// time.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       MC_FolderNameChangeConflict) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

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
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_NE(nullptr, AddURL(0, folderB[0], i, title, url));
  }

  // Create folder C with bookmarks and subfolders on both clients.
  folderC[0] = AddFolder(0, IndexedFolderName(2));
  ASSERT_NE(nullptr, folderC[0]);
  folderC[1] = AddFolder(1, IndexedFolderName(2));
  ASSERT_NE(nullptr, folderC[1]);
  for (int i = 0; i < 3; ++i) {
    std::string folder_name = IndexedSubfolderName(i);
    const BookmarkNode* subfolder = AddFolder(0, folderC[0], i, folder_name);
    ASSERT_NE(nullptr, subfolder);
    for (int j = 0; j < 3; ++j) {
      std::string title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_NE(nullptr, AddURL(0, subfolder, j, title, url));
    }
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder A on both clients.
  SetTitle(0, folderA[0], "Folder A++");
  SetTitle(1, folderA[1], "Folder A--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder B on both clients.
  SetTitle(0, folderB[0], "Folder B++");
  SetTitle(1, folderB[1], "Folder B--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));

  // Simultaneously rename folder C on both clients.
  SetTitle(0, folderC[0], "Folder C++");
  SetTitle(1, folderC[1], "Folder C--");
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_FALSE(ContainsDuplicateBookmarks(0));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_NE(nullptr, AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SingleClientEnabledEncryptionBothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_NE(nullptr, AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))));
  ASSERT_NE(nullptr, AddURL(0, IndexedURLTitle(1), GURL(IndexedURL(1))));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       SingleClientEnabledEncryptionAndChangedMultipleTimes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))));
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_NE(nullptr, AddURL(0, IndexedURLTitle(1), GURL(IndexedURL(1))));
  ASSERT_NE(nullptr, AddFolder(0, IndexedFolderName(0)));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       FirstClientEnablesEncryptionWithPassSecondChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Add initial bookmarks.
  ASSERT_NE(nullptr, AddURL(0, 0, IndexedURLTitle(0), GURL(IndexedURL(0))));
  ASSERT_NE(nullptr, AddURL(0, 1, IndexedURLTitle(1), GURL(IndexedURL(1))));
  ASSERT_NE(nullptr, AddURL(0, 2, IndexedURLTitle(2), GURL(IndexedURL(2))));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  // Set a passphrase and enable encryption on Client 0. Client 1 will not
  // understand the bookmark updates.
  GetSyncService(0)->SetEncryptionPassphrase(kValidPassphrase);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(GetSyncService(1)->IsPassphraseRequired());

  // Client 0 adds bookmarks between the first two and between the second two.
  ASSERT_NE(nullptr, AddURL(0, 1, IndexedURLTitle(3), GURL(IndexedURL(3))));
  ASSERT_NE(nullptr, AddURL(0, 3, IndexedURLTitle(4), GURL(IndexedURL(4))));
  EXPECT_FALSE(AllModelsMatchVerifier());
  EXPECT_FALSE(AllModelsMatch());

  // Set the passphrase. Everything should resolve.
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(GetSyncService(1)->SetDecryptionPassphrase(kValidPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_EQ(0, GetClient(1)->GetLastCycleSnapshot().num_encryption_conflicts());

  // Ensure everything is syncing normally by appending a final bookmark.
  ASSERT_NE(nullptr, AddURL(1, 5, IndexedURLTitle(5), GURL(IndexedURL(5))));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_EQ(0, GetClient(1)->GetLastCycleSnapshot().num_encryption_conflicts());
}

// Deliberately racy rearranging of bookmarks to test that our conflict resolver
// code results in a consistent view across machines (no matter what the final
// order is).
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       RacyPositionChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Add initial bookmarks.
  size_t num_bookmarks = 5;
  for (size_t i = 0; i < num_bookmarks; ++i) {
    ASSERT_NE(nullptr, AddURL(0, i, IndexedURLTitle(i), GURL(IndexedURL(i))));
  }

  // Once we make diverging changes the verifer is helpless.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  DisableVerifier();

  // Make changes on client 0.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 0's bookmark " << i << " to position "
             << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  // Make changes on client 1.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 1's bookmark " << i << " to position "
             << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Now make changes to client 1 first.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 1's bookmark " << i << " to position "
             << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  // Make changes on client 0.
  for (size_t i = 0; i < num_bookmarks; ++i) {
    const BookmarkNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_bookmarks-1);
    DVLOG(1) << "Moving client 0's bookmark " << i << " to position "
             << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(BookmarksMatchChecker().Wait());
}

// Trigger the server side creation of Synced Bookmarks. Ensure both clients
// remain syncing afterwards. Add bookmarks to the synced bookmarks folder
// and ensure both clients receive the bookmark.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       CreateSyncedBookmarks) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

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

// Enable enccryption and then trigger the server side creation of Synced
// Bookmarks. Ensure both clients remain syncing afterwards. Add bookmarks to
// the synced bookmarks folder and ensure both clients receive the bookmark.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       CreateSyncedBookmarksWithSingleClientEnableEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Enable the encryption on Client 0.
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
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

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       BookmarkAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

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
  ASSERT_TRUE(
      AddURL(0, GetOtherNode(0), 1, "CNN", GURL("http://www.cnn.com")));

  ASSERT_TRUE(AddFolder(0, GetBookmarkBarNode(0), 0, "empty_folder"));
  const BookmarkNode* folder1 =
      AddFolder(0, GetBookmarkBarNode(0), 1, "folder1");
  ASSERT_NE(nullptr,
            AddURL(0, folder1, 0, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(
      AddURL(0, GetBookmarkBarNode(0), 2, "Gmai", GURL("http://gmail.com")));

  ASSERT_TRUE(BookmarksMatchChecker().Wait());

  // Remove all
  RemoveAll(0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  // Verify other node has no children now.
  EXPECT_EQ(0, GetOtherNode(0)->child_count());
  EXPECT_EQ(0, GetBookmarkBarNode(0)->child_count());
  ASSERT_TRUE(AllModelsMatch());
}

// Verifies that managed bookmarks (installed by policy) don't get synced.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       ManagedBookmarks) {
  // Make sure the first Profile has an overridden policy provider.
  EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  policy::ProfilePolicyConnectorFactory::GetInstance()->PushProviderForTesting(
      &policy_provider_);

  // Set up sync.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Verify that there are no managed bookmarks at startup in either profile.
  // The Managed Bookmarks folder should not be visible at this stage.
  const BookmarkNode* managed_node0 = GetManagedNode(0);
  ASSERT_TRUE(managed_node0->empty());
  ASSERT_FALSE(managed_node0->IsVisible());
  const BookmarkNode* managed_node1 = GetManagedNode(1);
  ASSERT_TRUE(managed_node1->empty());
  ASSERT_FALSE(managed_node1->IsVisible());

  // Verify that the bookmark bar node is empty on both profiles too.
  const BookmarkNode* bar_node0 = GetBookmarkBarNode(0);
  ASSERT_TRUE(bar_node0->empty());
  ASSERT_TRUE(bar_node0->IsVisible());
  const BookmarkNode* bar_node1 = GetBookmarkBarNode(1);
  ASSERT_TRUE(bar_node1->empty());
  ASSERT_TRUE(bar_node1->IsVisible());

  // Verify that adding a bookmark is observed by the second Profile.
  GURL google_url("http://www.google.com");
  ASSERT_NE(nullptr, AddURL(0, "Google", google_url));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());
  ASSERT_EQ(1, bar_node0->child_count());
  ASSERT_EQ(1, bar_node1->child_count());

  // Set the ManagedBookmarks policy for the first Profile,
  // which will add one new managed bookmark.
  std::unique_ptr<base::DictionaryValue> bookmark(new base::DictionaryValue());
  bookmark->SetString("name", "Managed bookmark");
  bookmark->SetString("url", "youtube.com");
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  list->Append(std::move(bookmark));
  policy::PolicyMap policy;
  policy.Set(policy::key::kManagedBookmarks, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::move(list), nullptr);
  policy_provider_.UpdateChromePolicy(policy);
  base::RunLoop().RunUntilIdle();

  // Now add another user bookmark and wait for it to sync.
  ASSERT_NE(nullptr, AddURL(0, "Google 2", google_url));
  ASSERT_TRUE(BookmarksMatchVerifierChecker().Wait());

  EXPECT_FALSE(GetSyncService(0)->HasUnrecoverableError());
  EXPECT_FALSE(GetSyncService(1)->HasUnrecoverableError());

  // Verify that the managed bookmark exists in the local model of the first
  // Profile, and has a child node.
  ASSERT_EQ(1, managed_node0->child_count());
  ASSERT_TRUE(managed_node0->IsVisible());
  EXPECT_EQ(GURL("http://youtube.com/"), managed_node0->GetChild(0)->url());

  // Verify that the second Profile didn't get this node.
  ASSERT_EQ(0, managed_node1->child_count());
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       E2E_ONLY(SanitySetup)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       E2E_ONLY(OneClientAddsBookmark)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // All profiles should sync same bookmarks.
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";
  // For clean profiles, the bookmarks count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new bookmarks count.
  int init_bookmarks_count = CountAllBookmarks(0);

  // Add one new bookmark to the first profile.
  ASSERT_NE(nullptr,
            AddURL(0, "Google URL 0", GURL("http://www.google.com/0")));

  // Blocks and waits for bookmarks models in all profiles to match.
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  // Check that total number of bookmarks is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(CountAllBookmarks(i), init_bookmarks_count + 1) <<
        "Total bookmark count is wrong.";
  }
}

// TODO(shadi): crbug.com/569213: Enable this as E2E test.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
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
    ASSERT_EQ(1, CountFoldersWithTitlesMatching(i, "Folder 0"))
        << "Failed to match the folder";
    ASSERT_EQ(
        1, CountBookmarksWithUrlsMatching(i, GURL("http://www.google.com/0")))
        << "Failed to match the bookmark";
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       E2E_ONLY(TwoClientsAddBookmarks)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // ALl profiles should sync same bookmarks.
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";
  // For clean profiles, the bookmarks count should be zero. We are not
  // enforcing this, we only check that the final count is equal to initial
  // count plus new bookmarks count.
  int init_bookmarks_count = CountAllBookmarks(0);

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
    ASSERT_EQ(CountAllBookmarks(i), init_bookmarks_count + num_clients()) <<
        "Total bookmark count is wrong.";
  }
}

// Verify that a bookmark added on a client with bookmark syncing disabled gets
// synced to a second client once bookmark syncing is re-enabled.
IN_PROC_BROWSER_TEST_P(TwoClientBookmarksSyncTestIncludingUssTests,
                       E2E_ENABLED(AddBookmarkWhileDisabled)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarksMatchChecker().Wait())
      << "Initial bookmark models did not match for all profiles";
  const int initial_count = CountAllBookmarks(0);

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
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::BOOKMARKS));
  const std::string url_title_2 = "another happy little url";
  const GURL url_2("https://example.com/second");
  ASSERT_NE(nullptr, AddURL(0, GetBookmarkBarNode(0), 0, url_title_2, url_2));
  ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(syncer::BOOKMARKS));
  ASSERT_TRUE(BookmarksMatchChecker().Wait());
  ASSERT_EQ(initial_count + 2, CountAllBookmarks(0));
  ASSERT_EQ(initial_count + 2, CountAllBookmarks(1));
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientBookmarksSyncTestIncludingUssTests,
                        ::testing::Values(false, true));

}  // namespace
