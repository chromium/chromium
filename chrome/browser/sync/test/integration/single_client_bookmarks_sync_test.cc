// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/engine/bookmark_update_preprocessing.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/bookmark_entity_builder.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "components/sync/test/fake_server/fake_server_verifier.h"
#include "components/sync_bookmarks/switches.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::UrlAndTitle;
using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::BookmarkFaviconLoadedChecker;
using bookmarks_helper::BookmarkModelMatchesFakeServerChecker;
using bookmarks_helper::BookmarksGUIDChecker;
using bookmarks_helper::BookmarksTitleChecker;
using bookmarks_helper::BookmarksUrlChecker;
using bookmarks_helper::CheckHasNoFavicon;
using bookmarks_helper::ContainsBookmarkNodeWithGUID;
using bookmarks_helper::CountBookmarksWithTitlesMatching;
using bookmarks_helper::CountBookmarksWithUrlsMatching;
using bookmarks_helper::CountFoldersWithTitlesMatching;
using bookmarks_helper::Create1xFaviconFromPNGFile;
using bookmarks_helper::CreateFavicon;
using bookmarks_helper::GetBookmarkBarNode;
using bookmarks_helper::GetBookmarkModel;
using bookmarks_helper::GetBookmarkUndoService;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::GetUniqueNodeByURL;
using bookmarks_helper::IsFolderWithTitle;
using bookmarks_helper::IsFolderWithTitleAndChildrenAre;
using bookmarks_helper::IsUrlBookmarkWithTitleAndUrl;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::RemoveAll;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using testing::ElementsAre;
using testing::Eq;
using testing::NotNull;
using testing::Pointee;
using testing::SizeIs;

// All tests in this file utilize a single profile.
// TODO(pvalenzuela): Standardize this pattern by moving this constant to
// SyncTest and using it in all single client tests.
const int kSingleProfileIndex = 0;

// An arbitrary GUID, to be used for injecting the same bookmark entity to the
// fake server across PRE_MyTest and MyTest.
const char kBookmarkGuid[] = "e397ed62-9532-4dbf-ae55-200236eba15c";

// A title and a URL which are used across PRE_MyTest and MyTest.
const char kBookmarkTitle[] = "Title";
const char kBookmarkPageUrl[] = "http://www.foo.com/";

class SingleClientBookmarksSyncTest : public SyncTest {
 public:
  SingleClientBookmarksSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientBookmarksSyncTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientBookmarksSyncTest);
};

class SingleClientBookmarksSyncTestWithEnabledReuploadRemoteBookmarks
    : public SingleClientBookmarksSyncTest {
 public:
  SingleClientBookmarksSyncTestWithEnabledReuploadRemoteBookmarks() {
    features_override_.InitAndEnableFeature(
        switches::kSyncReuploadBookmarkFullTitles);
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

class SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks
    : public SyncTest {
 public:
  SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks()
      : SyncTest(SINGLE_CLIENT) {
    features_override_.InitAndDisableFeature(
        switches::kSyncReuploadBookmarkFullTitles);
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

class SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks
    : public SingleClientBookmarksSyncTest {
 public:
  SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks() {
    features_override_.InitWithFeatureState(
        switches::kSyncReuploadBookmarkFullTitles, !content::IsPreTest());
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

class SingleClientBookmarksSyncTestWithEnabledClientTags : public SyncTest {
 public:
  SingleClientBookmarksSyncTestWithEnabledClientTags()
      : SyncTest(SINGLE_CLIENT) {
    features_override_.InitAndEnableFeature(
        switches::kSyncUseClientTagForBookmarkCommits);
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //        -> http://www.pandora.com  "tier1_a_url1"
  //        -> http://www.facebook.com "tier1_a_url2"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* other_node = GetOtherNode(kSingleProfileIndex);
  const BookmarkNode* top =
      AddFolder(kSingleProfileIndex, other_node, 0, "top");
  const BookmarkNode* tier1_a = AddFolder(
      kSingleProfileIndex, top, 0, "tier1_a");
  const BookmarkNode* tier1_b = AddFolder(
      kSingleProfileIndex, top, 1, "tier1_b");
  const BookmarkNode* tier1_a_url0 = AddURL(
      kSingleProfileIndex, tier1_a, 0, "tier1_a_url0",
      GURL("http://mail.google.com"));
  const BookmarkNode* tier1_a_url1 = AddURL(
      kSingleProfileIndex, tier1_a, 1, "tier1_a_url1",
      GURL("http://www.pandora.com"));
  const BookmarkNode* tier1_a_url2 = AddURL(
      kSingleProfileIndex, tier1_a, 2, "tier1_a_url2",
      GURL("http://www.facebook.com"));
  const BookmarkNode* tier1_b_url0 = AddURL(
      kSingleProfileIndex, tier1_b, 0, "tier1_b_url0",
      GURL("http://www.nhl.com"));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolderWithTitleAndChildrenAre(
          "top",
          IsFolderWithTitleAndChildrenAre(
              "tier1_a",
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url0",
                                           GURL("http://mail.google.com")),
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url1",
                                           GURL("http://www.pandora.com")),
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url2",
                                           GURL("http://www.facebook.com"))),
          IsFolderWithTitleAndChildrenAre(
              "tier1_b", IsUrlBookmarkWithTitleAndUrl(
                             "tier1_b_url0", GURL("http://www.nhl.com"))))));

  //  Ultimately we want to end up with the following model; but this test is
  //  more about the journey than the destination.
  //
  //  bookmark_bar
  //    -> CNN (www.cnn.com)
  //    -> tier1_a
  //      -> tier1_a_url2 (www.facebook.com)
  //      -> tier1_a_url1 (www.pandora.com)
  //    -> Porsche (www.porsche.com)
  //    -> Bank of America (www.bankofamerica.com)
  //    -> Seattle Bubble
  //  other_node
  //    -> top
  //      -> tier1_b
  //        -> Wired News (www.wired.com)
  //        -> tier2_b
  //          -> tier1_b_url0
  //          -> tier3_b
  //            -> Toronto Maple Leafs (mapleleafs.nhl.com)
  //            -> Wynn (www.wynnlasvegas.com)
  //      -> tier1_a_url0
  const BookmarkNode* bar = GetBookmarkBarNode(kSingleProfileIndex);
  const BookmarkNode* cnn = AddURL(
      kSingleProfileIndex, bar, 0, "CNN", GURL("http://www.cnn.com"));
  ASSERT_NE(nullptr, cnn);
  Move(kSingleProfileIndex, tier1_a, bar, 1);

  // Wait for the bookmark position change to sync.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_THAT(bar->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(
                              "CNN", GURL("http://www.cnn.com")),
                          IsFolderWithTitle("tier1_a")));
  EXPECT_THAT(top->children(), ElementsAre(IsFolderWithTitle("tier1_b")));

  const BookmarkNode* porsche = AddURL(
      kSingleProfileIndex, bar, 2, "Porsche", GURL("http://www.porsche.com"));
  // Rearrange stuff in tier1_a.
  ASSERT_EQ(tier1_a, tier1_a_url2->parent());
  ASSERT_EQ(tier1_a, tier1_a_url1->parent());
  Move(kSingleProfileIndex, tier1_a_url2, tier1_a, 0);
  Move(kSingleProfileIndex, tier1_a_url1, tier1_a, 2);

  // Wait for the rearranged hierarchy to sync.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_THAT(
      bar->children(),
      ElementsAre(
          IsUrlBookmarkWithTitleAndUrl("CNN", GURL("http://www.cnn.com")),
          IsFolderWithTitleAndChildrenAre(
              "tier1_a",
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url2",
                                           GURL("http://www.facebook.com")),
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url0",
                                           GURL("http://mail.google.com")),
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url1",
                                           GURL("http://www.pandora.com"))),
          IsUrlBookmarkWithTitleAndUrl("Porsche",
                                       GURL("http://www.porsche.com"))));

  ASSERT_EQ(1, tier1_a_url0->parent()->GetIndexOf(tier1_a_url0));
  Move(kSingleProfileIndex, tier1_a_url0, bar, bar->children().size());
  const BookmarkNode* boa =
      AddURL(kSingleProfileIndex, bar, bar->children().size(),
             "Bank of America", GURL("https://www.bankofamerica.com"));
  ASSERT_NE(nullptr, boa);
  Move(kSingleProfileIndex, tier1_a_url0, top, top->children().size());
  const BookmarkNode* bubble =
      AddURL(kSingleProfileIndex, bar, bar->children().size(), "Seattle Bubble",
             GURL("http://seattlebubble.com"));
  ASSERT_NE(nullptr, bubble);
  const BookmarkNode* wired = AddURL(
      kSingleProfileIndex, bar, 2, "Wired News", GURL("http://www.wired.com"));
  const BookmarkNode* tier2_b = AddFolder(
      kSingleProfileIndex, tier1_b, 0, "tier2_b");
  Move(kSingleProfileIndex, tier1_b_url0, tier2_b, 0);
  Move(kSingleProfileIndex, porsche, bar, 0);
  SetTitle(kSingleProfileIndex, wired, "News Wired");
  SetTitle(kSingleProfileIndex, porsche, "ICanHazPorsche?");

  // Wait for the title change to sync.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_THAT(
      bar->children(),
      ElementsAre(
          IsUrlBookmarkWithTitleAndUrl("ICanHazPorsche?",
                                       GURL("http://www.porsche.com")),
          IsUrlBookmarkWithTitleAndUrl("CNN", GURL("http://www.cnn.com")),
          IsFolderWithTitle("tier1_a"),
          IsUrlBookmarkWithTitleAndUrl("News Wired",
                                       GURL("http://www.wired.com")),
          IsUrlBookmarkWithTitleAndUrl("Bank of America",
                                       GURL("https://www.bankofamerica.com")),
          IsUrlBookmarkWithTitleAndUrl("Seattle Bubble",
                                       GURL("http://seattlebubble.com"))));
  EXPECT_THAT(tier1_a->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl(
                              "tier1_a_url2", GURL("http://www.facebook.com")),
                          IsUrlBookmarkWithTitleAndUrl(
                              "tier1_a_url1", GURL("http://www.pandora.com"))));
  EXPECT_THAT(
      top->children(),
      ElementsAre(
          IsFolderWithTitleAndChildrenAre(
              "tier1_b",
              IsFolderWithTitleAndChildrenAre(
                  "tier2_b", IsUrlBookmarkWithTitleAndUrl(
                                 "tier1_b_url0", GURL("http://www.nhl.com")))),
          IsUrlBookmarkWithTitleAndUrl("tier1_a_url0",
                                       GURL("http://mail.google.com"))));

  ASSERT_EQ(tier1_a_url0->id(), top->children().back()->id());
  Remove(kSingleProfileIndex, top, top->children().size() - 1);
  Move(kSingleProfileIndex, wired, tier1_b, 0);
  Move(kSingleProfileIndex, porsche, bar, 3);
  const BookmarkNode* tier3_b = AddFolder(
      kSingleProfileIndex, tier2_b, 1, "tier3_b");
  const BookmarkNode* leafs = AddURL(
      kSingleProfileIndex, tier1_a, 0, "Toronto Maple Leafs",
      GURL("http://mapleleafs.nhl.com"));
  const BookmarkNode* wynn = AddURL(
      kSingleProfileIndex, bar, 1, "Wynn", GURL("http://www.wynnlasvegas.com"));

  Move(kSingleProfileIndex, wynn, tier3_b, 0);
  Move(kSingleProfileIndex, leafs, tier3_b, 0);

  // Wait for newly added bookmarks to sync.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  EXPECT_THAT(
      bar->children(),
      ElementsAre(
          IsUrlBookmarkWithTitleAndUrl("CNN", GURL("http://www.cnn.com")),
          IsFolderWithTitleAndChildrenAre(
              "tier1_a",
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url2",
                                           GURL("http://www.facebook.com")),
              IsUrlBookmarkWithTitleAndUrl("tier1_a_url1",
                                           GURL("http://www.pandora.com"))),
          IsUrlBookmarkWithTitleAndUrl("ICanHazPorsche?",
                                       GURL("http://www.porsche.com")),
          IsUrlBookmarkWithTitleAndUrl("Bank of America",
                                       GURL("https://www.bankofamerica.com")),
          IsUrlBookmarkWithTitleAndUrl("Seattle Bubble",
                                       GURL("http://seattlebubble.com"))));
  EXPECT_THAT(
      top->children(),
      ElementsAre(IsFolderWithTitleAndChildrenAre(
          "tier1_b",
          IsUrlBookmarkWithTitleAndUrl("News Wired",
                                       GURL("http://www.wired.com")),
          IsFolderWithTitleAndChildrenAre(
              "tier2_b",
              IsUrlBookmarkWithTitleAndUrl("tier1_b_url0",
                                           GURL("http://www.nhl.com")),
              IsFolderWithTitleAndChildrenAre(
                  "tier3_b",
                  IsUrlBookmarkWithTitleAndUrl(
                      "Toronto Maple Leafs", GURL("http://mapleleafs.nhl.com")),
                  IsUrlBookmarkWithTitleAndUrl(
                      "Wynn", GURL("http://www.wynnlasvegas.com")))))));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, CommitLocalCreations) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //        -> http://www.pandora.com  "tier1_a_url1"
  //        -> http://www.facebook.com "tier1_a_url2"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* other_node = GetOtherNode(kSingleProfileIndex);
  const BookmarkNode* top =
      AddFolder(kSingleProfileIndex, other_node, 0, "top");
  const BookmarkNode* tier1_a =
      AddFolder(kSingleProfileIndex, top, 0, "tier1_a");
  const BookmarkNode* tier1_b =
      AddFolder(kSingleProfileIndex, top, 1, "tier1_b");
  const BookmarkNode* tier1_a_url0 =
      AddURL(kSingleProfileIndex, tier1_a, 0, "tier1_a_url0",
             GURL("http://mail.google.com"));
  const BookmarkNode* tier1_a_url1 =
      AddURL(kSingleProfileIndex, tier1_a, 1, "tier1_a_url1",
             GURL("http://www.pandora.com"));
  const BookmarkNode* tier1_a_url2 =
      AddURL(kSingleProfileIndex, tier1_a, 2, "tier1_a_url2",
             GURL("http://www.facebook.com"));
  const BookmarkNode* tier1_b_url0 =
      AddURL(kSingleProfileIndex, tier1_b, 0, "tier1_b_url0",
             GURL("http://www.nhl.com"));
  EXPECT_TRUE(tier1_a_url0);
  EXPECT_TRUE(tier1_a_url1);
  EXPECT_TRUE(tier1_a_url2);
  EXPECT_TRUE(tier1_b_url0);
  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_THAT(other_node->children(),
              ElementsAre(IsFolderWithTitleAndChildrenAre(
                  "top",
                  IsFolderWithTitleAndChildrenAre(
                      "tier1_a",
                      IsUrlBookmarkWithTitleAndUrl("tier1_a_url0",
                                                   "http://mail.google.com"),
                      IsUrlBookmarkWithTitleAndUrl("tier1_a_url1",
                                                   "http://www.pandora.com"),
                      IsUrlBookmarkWithTitleAndUrl("tier1_a_url2",
                                                   "http://www.facebook.com")),
                  IsFolderWithTitleAndChildrenAre(
                      "tier1_b", IsUrlBookmarkWithTitleAndUrl(
                                     "tier1_b_url0", "http://www.nhl.com")))));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, InjectedBookmark) {
  std::string title = "Montreal Canadiens";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(
      GURL("http://canadiens.nhl.com")));

  ASSERT_TRUE(SetupSync());

  EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadTwoPre2015BookmarksWithSameItemId) {
  const std::string title1 = "Title1";
  const std::string title2 = "Title2";

  // Mimic the creation of two bookmarks from two different devices, with the
  // same client item ID.
  fake_server::BookmarkEntityBuilder bookmark_builder1(
      title1, /*originator_cache_guid=*/base::GenerateGUID(),
      /*originator_client_item_id=*/"1");
  fake_server::BookmarkEntityBuilder bookmark_builder2(
      title2, /*originator_cache_guid=*/base::GenerateGUID(),
      /*originator_client_item_id=*/"1");

  fake_server_->InjectEntity(bookmark_builder1.BuildBookmark(
      GURL("http://page1.com"), /*is_legacy=*/true));
  fake_server_->InjectEntity(bookmark_builder2.BuildBookmark(
      GURL("http://page2.com"), /*is_legacy=*/true));

  ASSERT_TRUE(SetupSync());

  EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title1));
  EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title2));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadLegacyUppercaseGuid2016BookmarksAndCommit) {
  const base::GUID guid = base::GUID::GenerateRandomV4();
  const std::string uppercase_guid_str =
      base::ToUpperASCII(guid.AsLowercaseString());
  const std::string title1 = "Title1";
  const std::string title2 = "Title2";

  // Bookmarks created around 2016, between [M44..M52) use an uppercase GUID as
  // originator client item ID.
  fake_server::BookmarkEntityBuilder bookmark_builder(
      title1, /*originator_cache_guid=*/base::GenerateGUID(),
      /*originator_client_item_id=*/uppercase_guid_str);

  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(
      GURL("http://page1.com"), /*is_legacy=*/true));

  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title1));

  // The GUID should have been canonicalized (lowercased) in BookmarkModel.
  EXPECT_TRUE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, guid));

  // Changing the title should populate the server-side GUID in specifics in
  // lowercase form.
  ASSERT_EQ(1u, GetBookmarkBarNode(0)->children().size());
  ASSERT_EQ(base::ASCIIToUTF16(title1),
            GetBookmarkBarNode(0)->children()[0]->GetTitle());
  SetTitle(kSingleProfileIndex, GetBookmarkBarNode(0)->children().front().get(),
           title2);
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  // Verify the GUID that was committed to the server.
  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks.size());
  ASSERT_EQ(
      title2,
      server_bookmarks[0].specifics().bookmark().legacy_canonicalized_title());
  EXPECT_EQ(guid.AsLowercaseString(),
            server_bookmarks[0].specifics().bookmark().guid());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadModernBookmarkCollidingPre2015BookmarkId) {
  const std::string title1 = "Title1";
  const std::string title2 = "Title2";

  const std::string kOriginalOriginatorCacheGuid = base::GenerateGUID();
  const std::string kOriginalOriginatorClientItemId = "1";

  // One pre-2015 bookmark, nothing special here.
  fake_server::BookmarkEntityBuilder bookmark_builder1(
      title1, kOriginalOriginatorCacheGuid, kOriginalOriginatorClientItemId);

  // A second bookmark, possibly uploaded by a buggy client, happens to use an
  // originator client item ID that collides with the GUID that would have been
  // inferred for the original pre-2015 bookmark.
  fake_server::BookmarkEntityBuilder bookmark_builder2(
      title2, /*originator_cache_guid=*/base::GenerateGUID(),
      /*originator_client_item_id=*/
      syncer::InferGuidForLegacyBookmarkForTesting(
          kOriginalOriginatorCacheGuid, kOriginalOriginatorClientItemId));

  fake_server_->InjectEntity(bookmark_builder1.BuildBookmark(
      GURL("http://page1.com"), /*is_legacy=*/true));
  fake_server_->InjectEntity(bookmark_builder2.BuildBookmark(
      GURL("http://page2.com"), /*is_legacy=*/true));

  ASSERT_TRUE(SetupSync());

  const BookmarkNode* bookmark_bar_node =
      GetBookmarkBarNode(kSingleProfileIndex);
  // Check only number of bookmarks since any of them may be removed as
  // duplicate.
  EXPECT_EQ(1u, bookmark_bar_node->children().size());
}

// Test that a client doesn't mutate the favicon data in the process
// of storing the favicon data from sync to the database or in the process
// of requesting data from the database for sync.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       SetFaviconHiDPIDifferentCodec) {
  // Set the supported scale factors to 1x and 2x such that
  // BookmarkModel::GetFavicon() requests both 1x and 2x.
  // 1x -> for sync, 2x -> for the UI.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon.ico");
  const BookmarkNode* bookmark = AddURL(kSingleProfileIndex, "title", page_url);

  // Simulate receiving a favicon from sync encoded by a different PNG encoder
  // than the one native to the OS. This tests the PNG data is not decoded to
  // SkBitmap (or any other image format) then encoded back to PNG on the path
  // between sync and the database.
  gfx::Image original_favicon = Create1xFaviconFromPNGFile(
      "favicon_cocoa_png_codec.png");
  ASSERT_FALSE(original_favicon.IsEmpty());
  SetFavicon(kSingleProfileIndex, bookmark, icon_url, original_favicon,
             bookmarks_helper::FROM_SYNC);

  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());

  scoped_refptr<base::RefCountedMemory> original_favicon_bytes =
      original_favicon.As1xPNGBytes();
  gfx::Image final_favicon =
      GetBookmarkModel(kSingleProfileIndex)->GetFavicon(bookmark);
  scoped_refptr<base::RefCountedMemory> final_favicon_bytes =
      final_favicon.As1xPNGBytes();

  // Check that the data was not mutated from the original.
  EXPECT_TRUE(original_favicon_bytes.get());
  EXPECT_TRUE(original_favicon_bytes->Equals(final_favicon_bytes));
}

// Test that a client deletes favicons from sync when they have been removed
// from the local database.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, DeleteFaviconFromSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon.ico");
  const BookmarkNode* bookmark = AddURL(kSingleProfileIndex, "title", page_url);
  SetFavicon(0, bookmark, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());

  // Simulate receiving a favicon deletion from sync.
  DeleteFaviconMappings(kSingleProfileIndex, bookmark,
                        bookmarks_helper::FROM_SYNC);

  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_THAT(GetBookmarkBarNode(kSingleProfileIndex)->children(),
              ElementsAre(IsUrlBookmarkWithTitleAndUrl("title", page_url)));

  CheckHasNoFavicon(kSingleProfileIndex, page_url);
  EXPECT_TRUE(
      GetBookmarkModel(kSingleProfileIndex)->GetFavicon(bookmark).IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       BookmarkAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
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

  const BookmarkNode* folder0 = AddFolder(
      kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, "folder0");
  const BookmarkNode* tier1_a =
      AddFolder(kSingleProfileIndex, folder0, 0, "tier1_a");
  ASSERT_TRUE(AddURL(kSingleProfileIndex, folder0, 1, "News",
                     GURL("http://news.google.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex, folder0, 2, "Yahoo",
                     GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex, tier1_a, 0, "Gmai",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex, tier1_a, 1, "Google",
                     GURL("http://www.google.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 1,
                     "CNN", GURL("http://www.cnn.com")));

  ASSERT_TRUE(AddFolder(kSingleProfileIndex,
                        GetBookmarkBarNode(kSingleProfileIndex), 0,
                        "empty_folder"));
  const BookmarkNode* folder1 =
      AddFolder(kSingleProfileIndex, GetBookmarkBarNode(kSingleProfileIndex), 1,
                "folder1");
  ASSERT_TRUE(AddURL(kSingleProfileIndex, folder1, 0, "Yahoo",
                     GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex, GetBookmarkBarNode(0), 2, "Gmai",
                     GURL("http://gmail.com")));

  // Set up sync, wait for its completion and verify that changes propagated.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(2u, GetOtherNode(kSingleProfileIndex)->children().size());
  ASSERT_EQ(3u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  // Remove all bookmarks and wait for sync completion.
  RemoveAll(kSingleProfileIndex);
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  // Verify other node has no children now.
  EXPECT_TRUE(GetOtherNode(kSingleProfileIndex)->children().empty());
  EXPECT_TRUE(GetBookmarkBarNode(kSingleProfileIndex)->children().empty());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, DownloadDeletedBookmark) {
  std::string title = "Patrick Star";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(
      GURL("http://en.wikipedia.org/wiki/Patrick_Star")));

  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title));

  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1ul, server_bookmarks.size());
  std::string entity_id = server_bookmarks[0].id_string();
  std::unique_ptr<syncer::LoopbackServerEntity> tombstone(
      syncer::PersistentTombstoneEntity::CreateNew(entity_id, std::string()));
  GetFakeServer()->InjectEntity(std::move(tombstone));

  const int kExpectedCountAfterDeletion = 0;
  ASSERT_TRUE(BookmarksTitleChecker(kSingleProfileIndex, title,
                                    kExpectedCountAfterDeletion)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadModifiedBookmark) {
  std::string title = "Syrup";
  GURL original_url = GURL("https://en.wikipedia.org/?title=Maple_syrup");
  GURL updated_url = GURL("https://en.wikipedia.org/wiki/Xylem");

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(original_url));

  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title));
  ASSERT_EQ(1u,
            CountBookmarksWithUrlsMatching(kSingleProfileIndex, original_url));
  ASSERT_EQ(0u,
            CountBookmarksWithUrlsMatching(kSingleProfileIndex, updated_url));

  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1ul, server_bookmarks.size());
  std::string entity_id = server_bookmarks[0].id_string();

  sync_pb::EntitySpecifics specifics = server_bookmarks[0].specifics();
  sync_pb::BookmarkSpecifics* bookmark_specifics = specifics.mutable_bookmark();
  bookmark_specifics->set_url(updated_url.spec());
  ASSERT_TRUE(GetFakeServer()->ModifyEntitySpecifics(entity_id, specifics));

  ASSERT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, updated_url, 1).Wait());
  ASSERT_EQ(0u,
            CountBookmarksWithUrlsMatching(kSingleProfileIndex, original_url));
  ASSERT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, DownloadBookmarkFolder) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadLegacyBookmarkFolder) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder(/*is_legacy=*/true));

  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));
}

// Legacy bookmark clients append a blank space to empty titles, ".", ".." tiles
// before committing them because historically they were illegal server titles.
// This test makes sure that this functionality is implemented for backward
// compatibility with legacy clients.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldCommitBookmarksWithIllegalServerNames) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::vector<std::string> illegal_titles = {"", ".", ".."};
  // Create 3 bookmarks under the bookmark bar with illegal titles.
  for (const std::string& illegal_title : illegal_titles) {
    ASSERT_TRUE(AddURL(kSingleProfileIndex, illegal_title,
                       GURL("http://www.google.com")));
  }

  // Wait till all entities are committed.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  // Collect the titles committed on the server.
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  std::vector<std::string> committed_titles;
  for (const sync_pb::SyncEntity& entity : entities) {
    committed_titles.push_back(
        entity.specifics().bookmark().legacy_canonicalized_title());
  }

  // A space should have been appended to each illegal title before committing.
  EXPECT_THAT(committed_titles,
              testing::UnorderedElementsAre(" ", ". ", ".. "));
}

// This test the opposite functionality in the test above. Legacy bookmark
// clients omit a blank space from blank space title, ". ", ".. " tiles upon
// receiving the remote updates. An extra space has been appended during a
// commit because historically they were considered illegal server titles. This
// test makes sure that this functionality is implemented for backward
// compatibility with legacy clients.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldCreateLocalBookmarksWithIllegalServerNames) {
  const std::vector<std::string> illegal_titles = {"", ".", ".."};

  // Create 3 bookmarks on the server under BookmarkBar with illegal server
  // titles with a blank space appended to simulate a commit from a legacy
  // client.
  fake_server::EntityBuilderFactory entity_builder_factory;
  for (const std::string& illegal_title : illegal_titles) {
    fake_server::BookmarkEntityBuilder bookmark_builder =
        entity_builder_factory.NewBookmarkEntityBuilder(illegal_title + " ");
    fake_server_->InjectEntity(bookmark_builder.BuildBookmarkWithoutFullTitle(
        GURL("http://www.google.com")));
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // There should be bookmark with illegal title (without the appended space).
  for (const std::string& illegal_title : illegal_titles) {
    EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex,
                                                   illegal_title));
  }
}

// Legacy bookmark clients append a blank space to empty titles. This tests that
// this is respected when merging local and remote hierarchies.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldTruncateBlanksWhenMatchingTitles) {
  const std::string remote_blank_title = " ";
  const std::string local_empty_title;

  // Create a folder on the server under BookmarkBar with a title with a blank
  // space.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(remote_blank_title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolderWithoutFullTitle());

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Create a folder on the client under BookmarkBar with an empty title.
  const BookmarkNode* node =
      AddFolder(kSingleProfileIndex, GetBookmarkBarNode(kSingleProfileIndex), 0,
                local_empty_title);
  ASSERT_TRUE(node);
  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex,
                                               local_empty_title));

  ASSERT_TRUE(SetupSync());
  // There should be only one bookmark on the client. The remote node should
  // have been merged with the local node and either the local or remote titles
  // is picked.
  EXPECT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex,
                                               local_empty_title) +
                    CountFoldersWithTitlesMatching(kSingleProfileIndex,
                                                   remote_blank_title));
}

// Legacy bookmark clients truncate long titles up to 255 bytes. This tests that
// this is respected when merging local and remote hierarchies.
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldTruncateLongTitles) {
  const std::string remote_truncated_title =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
      "uvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN"
      "OPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
      "ijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU";
  const std::string local_full_title =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
      "uvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN"
      "OPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
      "ijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzAB"
      "CDEFGHIJKLMNOPQRSTUVWXYZ";

  // Create a folder on the server under BookmarkBar with a truncated title.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(remote_truncated_title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Create a folder on the client under BookmarkBar with a long title.
  const BookmarkNode* node =
      AddFolder(kSingleProfileIndex, GetBookmarkBarNode(kSingleProfileIndex), 0,
                local_full_title);
  ASSERT_TRUE(node);
  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex,
                                               local_full_title));

  ASSERT_TRUE(SetupSync());
  // There should be only one bookmark on the client. The remote node should
  // have been merged with the local node and either the local or remote title
  // is picked.
  EXPECT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex,
                                               local_full_title) +
                    CountFoldersWithTitlesMatching(kSingleProfileIndex,
                                                   remote_truncated_title));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadBookmarkFoldersWithPositions) {
  const std::string title0 = "Folder left";
  const std::string title1 = "Folder middle";
  const std::string title2 = "Folder right";

  fake_server::EntityBuilderFactory entity_builder_factory;

  fake_server::BookmarkEntityBuilder bookmark0_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title0);
  bookmark0_builder.SetIndex(0);

  fake_server::BookmarkEntityBuilder bookmark1_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title1);
  bookmark1_builder.SetIndex(1);

  fake_server::BookmarkEntityBuilder bookmark2_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title2);
  bookmark2_builder.SetIndex(2);

  fake_server_->InjectEntity(bookmark0_builder.BuildFolder());
  fake_server_->InjectEntity(bookmark2_builder.BuildFolder());
  fake_server_->InjectEntity(bookmark1_builder.BuildFolder());

  ASSERT_TRUE(SetupSync());

  EXPECT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title0));
  EXPECT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title1));
  EXPECT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title2));

  const BookmarkNode* bar = GetBookmarkBarNode(kSingleProfileIndex);
  ASSERT_EQ(3u, bar->children().size());
  EXPECT_EQ(base::ASCIIToUTF16(title0), bar->children()[0]->GetTitle());
  EXPECT_EQ(base::ASCIIToUTF16(title1), bar->children()[1]->GetTitle());
  EXPECT_EQ(base::ASCIIToUTF16(title2), bar->children()[2]->GetTitle());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, E2E_ONLY(SanitySetup)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTest,
    RemoveRightAfterAddShouldNotSendCommitRequestsOrTombstones) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add a folder and directly remove it.
  ASSERT_NE(nullptr,
            AddFolder(kSingleProfileIndex,
                      /*parent=*/GetBookmarkBarNode(kSingleProfileIndex),
                      /*index=*/0, "folder name"));
  Remove(kSingleProfileIndex,
         /*parent=*/GetBookmarkBarNode(kSingleProfileIndex), 0);

  // Add another bookmark to make sure a full sync cycle completion.
  ASSERT_NE(nullptr, AddURL(kSingleProfileIndex,
                            /*parent=*/GetOtherNode(kSingleProfileIndex),
                            /*index=*/0, "title", GURL("http://www.url.com")));
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  // There should have been one creation and no deletions.
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_CREATION=*/1));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_DELETION=*/0));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title, kBookmarkGuid);
  bookmark_builder.SetId(
      syncer::LoopbackServerEntity::CreateId(syncer::BOOKMARKS, kBookmarkGuid));
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  EXPECT_NE(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       PersistProgressMarkerOnRestart) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title, kBookmarkGuid);
  bookmark_builder.SetId(
      syncer::LoopbackServerEntity::CreateId(syncer::BOOKMARKS, kBookmarkGuid));
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithValidGUID) {
  // Start syncing.
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  // Create a bookmark folder with a valid GUID.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Issue remote creation with a valid GUID.
  base::HistogramTester histogram_tester;
  std::unique_ptr<syncer::LoopbackServerEntity> folder =
      bookmark_builder.BuildFolder(/*is_legacy=*/false);
  const base::GUID guid = base::GUID::ParseCaseInsensitive(
      folder.get()->GetSpecifics().bookmark().guid());
  ASSERT_TRUE(guid.is_valid());
  ASSERT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, guid));
  fake_server_->InjectEntity(std::move(folder));

  // A folder should have been added with the corresponding GUID.
  EXPECT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, guid).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(
      guid,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid());
  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kSpecifics=*/0));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithoutValidGUID) {
  // Start syncing.
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  const base::GUID originator_client_item_id = base::GUID::GenerateRandomV4();
  ASSERT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex,
                                            originator_client_item_id));

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id.AsLowercaseString());

  // Issue remote creation without a valid GUID but with a valid
  // originator_client_item_id.
  base::HistogramTester histogram_tester;
  std::unique_ptr<syncer::LoopbackServerEntity> folder =
      bookmark_builder.BuildFolder(/*is_legacy=*/true);
  ASSERT_TRUE(folder.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(folder));

  // A bookmark folder should have been added with the originator_client_item_id
  // as the GUID.
  EXPECT_TRUE(
      BookmarksGUIDChecker(kSingleProfileIndex, originator_client_item_id)
          .Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(
      originator_client_item_id,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid());

  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kValidOCII=*/1));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithoutValidGUIDOrOCII) {
  // Start syncing.
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  GURL url = GURL("http://foo.com");
  const std::string originator_client_item_id = "INVALID OCII";

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id);

  // Issue remote creation without a valid GUID or a valid
  // originator_client_item_id.
  base::HistogramTester histogram_tester;
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/true);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // A bookmark should have been added with a newly assigned valid GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_FALSE(ContainsBookmarkNodeWithGUID(
      kSingleProfileIndex,
      base::GUID::ParseCaseInsensitive(originator_client_item_id)));

  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kInferred=*/3));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithValidGUID) {
  const GURL url = GURL("http://www.foo.com");
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Create bookmark in server with a valid GUID.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/false);
  const base::GUID guid = base::GUID::ParseCaseInsensitive(
      bookmark.get()->GetSpecifics().bookmark().guid());
  ASSERT_TRUE(guid.is_valid());
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with the corresponding GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, guid));

  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kSpecifics=*/0));
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kValidOCII=*/1));
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kLeftEmpty=*/2));
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kInferred=*/3));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldStartTrackingRestoredBookmark) {
  ASSERT_TRUE(SetupSync());

  BookmarkModel* bookmark_model = GetBookmarkModel(kSingleProfileIndex);
  const BookmarkNode* bookmark_bar_node =
      GetBookmarkBarNode(kSingleProfileIndex);

  // First add a new bookmark.
  const std::string title = "Title";
  const BookmarkNode* node = bookmark_model->AddFolder(
      bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(title));
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  const std::vector<sync_pb::SyncEntity> server_bookmarks_before =
      fake_server_->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks_before.size());

  // Remove the node and undo the action.
  bookmark_model->Remove(node);
  BookmarkUndoService* undo_service =
      GetBookmarkUndoService(kSingleProfileIndex);
  undo_service->undo_manager()->Undo();

  // Do not use BookmarkModelMatchesFakeServerChecker because the structure and
  // bookmarks actually don't change. The only change is the version of the
  // entity.
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());

  // Check that the bookmark was committed again.
  const std::vector<sync_pb::SyncEntity> server_bookmarks_after =
      fake_server_->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks_after.size());
  EXPECT_GT(server_bookmarks_after.front().version(),
            server_bookmarks_before.front().version());
  EXPECT_EQ(server_bookmarks_after.front().id_string(),
            server_bookmarks_before.front().id_string());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithoutValidGUID) {
  const GURL url = GURL("http://www.foo.com");
  const base::GUID originator_client_item_id = base::GUID::GenerateRandomV4();
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id.AsLowercaseString());

  // Create bookmark in server without a valid GUID but with a valid
  // originator_client_item_id.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/true);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with the originator_client_item_id as the
  // GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex,
                                           originator_client_item_id));

  // Do not check for kSpecifics bucket because it may be 0 or 1. It depends on
  // reupload of bookmark (legacy bookmark will be reuploaded and may return
  // from the server again with valid GUID in specifics).
  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kValidOCII=*/1));
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kLeftEmpty=*/2));
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kInferred=*/3));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithoutValidGUIDOrOCII) {
  const GURL url = GURL("http://www.foo.com");
  const std::string originator_client_item_id = "INVALID OCII";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id);

  // Create bookmark in server without a valid GUID and without a valid
  // originator_client_item_id.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/true);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with a newly assigned valid GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_FALSE(ContainsBookmarkNodeWithGUID(
      kSingleProfileIndex,
      base::GUID::ParseCaseInsensitive(originator_client_item_id)));

  // Do not check for kSpecifics bucket because it may be 0 or 1. It depends on
  // reupload of bookmark (legacy bookmark will be reuploaded and may return
  // from the server again with valid GUID in specifics).
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kValidOCII=*/1));
  EXPECT_EQ(0, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kLeftEmpty=*/2));
  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kInferred=*/3));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       MergeRemoteUpdateWithValidGUID) {
  ASSERT_TRUE(SetupClients());

  // Create a local bookmark folder.
  const std::string title = "Seattle Sounders FC";
  const BookmarkNode* local_folder = AddFolder(
      kSingleProfileIndex, GetBookmarkBarNode(kSingleProfileIndex), 0, title);
  const base::GUID old_guid = local_folder->guid();
  SCOPED_TRACE(std::string("old_guid=") + old_guid.AsLowercaseString());

  ASSERT_TRUE(local_folder);
  ASSERT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, old_guid).Wait());
  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  // Create an equivalent remote folder.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.BuildFolder(/*is_legacy=*/false);
  const base::GUID new_guid = base::GUID::ParseCaseInsensitive(
      remote_folder->GetSpecifics().bookmark().guid());
  fake_server_->InjectEntity(std::move(remote_folder));

  // Start syncing.
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // The folder GUID should have been updated with the corresponding value.
  EXPECT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, new_guid).Wait());
  EXPECT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, old_guid));
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks,
    PRE_ShouldNotReploadUponFaviconLoad) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Foo Title");

  // Create a legacy bookmark on the server (no GUID field populated). The fact
  // that it's a legacy bookmark means any locally-produced specifics would be
  // different for this bookmark (new fields like GUID would be populated).
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(GURL(kBookmarkPageUrl),
                                     /*is_legacy=*/true);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      BookmarksUrlChecker(kSingleProfileIndex, GURL(kBookmarkPageUrl), 1)
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       PRE_ShouldUploadUnsyncedEntityAfterRestart) {
  ASSERT_TRUE(SetupSync());

  const std::string title = "Title";
  const std::string new_title = "New Title";
  const GURL icon_url("http://www.google.com/favicon.ico");

  const BookmarkNode* bookmark =
      AddURL(kSingleProfileIndex, title, GURL(kBookmarkPageUrl));
  SetFavicon(0, bookmark, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);

  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(kSingleProfileIndex), GetFakeServer(),
                  {{title, GURL(kBookmarkPageUrl)}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // Stop Sync and update local entity to enter in unsynced state.
  GetClient(kSingleProfileIndex)->StopSyncServiceWithoutClearingData();

  SetTitle(kSingleProfileIndex, bookmark, new_title);
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldUploadUnsyncedEntityAfterRestart) {
  ASSERT_TRUE(SetupClients());

  const std::string title = "Title";
  const std::string new_title = "New Title";
  const GURL url = GURL("http://www.foo.com");

  // Ensure that local bookmark has new title and the server has the old one.
  ASSERT_EQ(1u,
            CountBookmarksWithTitlesMatching(kSingleProfileIndex, new_title));

  const std::vector<sync_pb::SyncEntity> server_bookmarks_before =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks_before.size());
  ASSERT_EQ(title, server_bookmarks_before.front()
                       .specifics()
                       .bookmark()
                       .legacy_canonicalized_title());

  // Ensure that there is a favicon on the server and local node haven't started
  // loading of favicon.
  ASSERT_TRUE(
      server_bookmarks_before.front().specifics().bookmark().has_favicon());
  const BookmarkNode* node =
      bookmarks_helper::GetUniqueNodeByURL(kSingleProfileIndex, url);
  ASSERT_FALSE(node->is_favicon_loading());
  ASSERT_FALSE(node->is_favicon_loaded());

  ASSERT_TRUE(GetClient(kSingleProfileIndex)->StartSyncService());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(kSingleProfileIndex), GetFakeServer(),
                  {{new_title, url}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // Last commit should initiate favicon loading.
  ASSERT_TRUE(BookmarkFaviconLoadedChecker(kSingleProfileIndex, url).Wait());
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  const std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks.size());

  // Once loaded, the favicon must be uploaded to the server. This expectation
  // passes only if kSyncDoNotCommitBookmarksWithoutFavicon is enabled and
  // prevents specifics from committing without loaded favicon.
  EXPECT_TRUE(server_bookmarks.front().specifics().bookmark().has_favicon());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks,
    ShouldNotReploadUponFaviconLoad) {
  const GURL url = GURL("http://www.foo.com");

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());

  // Make sure the favicon gets loaded.
  const BookmarkNode* bookmark_node =
      GetUniqueNodeByURL(kSingleProfileIndex, url);
  ASSERT_NE(nullptr, bookmark_node);
  GetBookmarkModel(kSingleProfileIndex)->GetFavicon(bookmark_node);
  ASSERT_TRUE(BookmarkFaviconLoadedChecker(kSingleProfileIndex, url).Wait());

  // Make sure all local commits make it to the server.
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());

  // Verify that the bookmark hasn't been uploaded (no local updates issued). No
  // commits are expected despite the fact that the server-side bookmark is a
  // legacy bookmark without the most recent fields (e.g. GUID), because loading
  // favicons should not lead to commits unless the favicon itself changed.
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_UPDATE=*/2));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadRemoteBookmarks,
    ShouldReuploadFullTitleAfterInitialMerge) {
  ASSERT_TRUE(SetupClients());

  const std::string title = "Title";

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.BuildFolderWithoutFullTitle();
  ASSERT_FALSE(remote_folder->GetSpecifics().bookmark().has_full_title());
  fake_server_->InjectEntity(std::move(remote_folder));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(kSingleProfileIndex), GetFakeServer(),
                  {{title, /*url=*/GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());
}

// This test looks similar to
// ShouldReuploadFullTitleAfterRestartOnIncrementalChange, but current test
// initiates reupload after restart only (before restart the entity is in synced
// state).
IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks,
    PRE_ShouldReuploadFullTitleForOldClients) {
  // Prepare legacy bookmark without full_title field in specifics and store it
  // locally.
  ASSERT_TRUE(SetupSync());

  // Make an incremental remote creation of bookmark without full_title.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kBookmarkTitle);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.BuildBookmarkWithoutFullTitle(GURL(kBookmarkPageUrl));

  fake_server_->InjectEntity(std::move(remote_folder));

  ASSERT_TRUE(BookmarksTitleChecker(kSingleProfileIndex, kBookmarkTitle,
                                    /*expected_count=*/1)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks,
    ShouldReuploadFullTitleForOldClients) {
  // This test checks that the legacy bookmark which was stored locally will
  // imply reupload to the server when reupload feature is enabled.
  ASSERT_EQ(
      1u,
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS).size());
  ASSERT_FALSE(GetFakeServer()
                   ->GetSyncEntitiesByModelType(syncer::BOOKMARKS)
                   .front()
                   .specifics()
                   .bookmark()
                   .has_full_title());
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());
  ASSERT_TRUE(
      BookmarkFaviconLoadedChecker(kSingleProfileIndex, GURL(kBookmarkPageUrl))
          .Wait());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(kSingleProfileIndex), GetFakeServer(),
                  {{kBookmarkTitle, GURL(kBookmarkPageUrl)}},
                  /*cryptographer=*/nullptr)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadRemoteBookmarks,
    ShouldReuploadBookmarkWithFaviconOnInitialMerge) {
  const GURL kIconUrl("http://www.google.com/favicon.ico");

  // Create a bookmark on the server which has a favicon and doesn't have GUID.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kBookmarkTitle);
  bookmark_builder.SetFavicon(CreateFavicon(SK_ColorRED), kIconUrl);
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark_entity =
      bookmark_builder.BuildBookmark(GURL(kBookmarkPageUrl),
                                     /*is_legacy=*/true);
  ASSERT_FALSE(bookmark_entity->GetSpecifics().bookmark().has_guid());
  fake_server_->InjectEntity(std::move(bookmark_entity));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      BookmarkFaviconLoadedChecker(kSingleProfileIndex, GURL(kBookmarkPageUrl))
          .Wait());
  const bookmarks::BookmarkNode* bookmark =
      bookmarks_helper::GetUniqueNodeByURL(kSingleProfileIndex,
                                           GURL(kBookmarkPageUrl));
  ASSERT_THAT(bookmark, NotNull());
  const gfx::Image& favicon =
      GetBookmarkModel(kSingleProfileIndex)->GetFavicon(bookmark);
  ASSERT_FALSE(favicon.IsEmpty());
  ASSERT_THAT(bookmark->icon_url(), Pointee(Eq(kIconUrl)));

  // BookmarkModelMatchesFakeServerChecker uses GUIDs to verify if the local
  // model matches the server bookmarks which verifies that the bookmark has
  // been reuploaded.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  const std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_THAT(server_bookmarks, SizeIs(1));
  EXPECT_TRUE(server_bookmarks.front().specifics().bookmark().has_guid());

  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_UPDATE*/ 2));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.BookmarkEntityReuploadNeeded.OnInitialMerge", true));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadRemoteBookmarks,
    PRE_ShouldReuploadFullTitleAfterRestartOnIncrementalChange) {
  ASSERT_TRUE(SetupSync());

  // Make an incremental remote creation of bookmark.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kBookmarkTitle);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.BuildBookmarkWithoutFullTitle(GURL(kBookmarkPageUrl));
  ASSERT_FALSE(remote_folder->GetSpecifics().bookmark().has_full_title());
  fake_server_->InjectEntity(std::move(remote_folder));

  ASSERT_TRUE(BookmarksTitleChecker(kSingleProfileIndex, kBookmarkTitle,
                                    /*expected_count=*/1)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadRemoteBookmarks,
    ShouldReuploadFullTitleAfterRestartOnIncrementalChange) {
  // Check that the full title was not uploaded to the server yet.
  ASSERT_EQ(
      1u,
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS).size());
  ASSERT_FALSE(GetFakeServer()
                   ->GetSyncEntitiesByModelType(syncer::BOOKMARKS)
                   .front()
                   .specifics()
                   .bookmark()
                   .has_full_title());

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());
  ASSERT_TRUE(
      BookmarkFaviconLoadedChecker(kSingleProfileIndex, GURL(kBookmarkPageUrl))
          .Wait());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(kSingleProfileIndex), GetFakeServer(),
                  {{kBookmarkTitle, GURL(kBookmarkPageUrl)}},
                  /*cryptographer=*/nullptr)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTestWithEnabledClientTags,
                       CommitLocalCreationWithClientTag) {
  ASSERT_TRUE(SetupSync());

  const std::string kTitle = "Title";
  const BookmarkNode* folder = AddFolder(
      kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, kTitle);

  // Wait until the local bookmark gets committed.
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  GetSyncService(kSingleProfileIndex), GetFakeServer(),
                  {{kTitle, /*url=*/GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // Verify the client tag hash was committed to the server.
  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks.size());
  EXPECT_EQ(server_bookmarks[0].client_defined_unique_tag(),
            syncer::ClientTagHash::FromUnhashed(
                syncer::BOOKMARKS, folder->guid().AsLowercaseString())
                .value());
}

}  // namespace
