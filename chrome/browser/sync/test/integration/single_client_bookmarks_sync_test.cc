// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine_impl/loopback_server/loopback_server_entity.h"
#include "components/sync/test/fake_server/bookmark_entity_builder.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "components/sync/test/fake_server/fake_server_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::UrlAndTitle;
using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
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
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::ModelMatchesVerifier;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using bookmarks_helper::RemoveAll;
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;

// All tests in this file utilize a single profile.
// TODO(pvalenzuela): Standardize this pattern by moving this constant to
// SyncTest and using it in all single client tests.
const int kSingleProfileIndex = 0;

// An arbitrary GUID, to be used for injecting the same bookmark entity to the
// fake server across PRE_MyTest and MyTest.
const char kBookmarkGuid[] = "e397ed62-9532-4dbf-ae55-200236eba15c";

class SingleClientBookmarksSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientBookmarksSyncTest()
      : FeatureToggler(switches::kProfileSyncServiceUsesThreadPool),
        SyncTest(SINGLE_CLIENT) {}
  ~SingleClientBookmarksSyncTest() override {}

  // Verify that the local bookmark model (for the Profile corresponding to
  // |index|) matches the data on the FakeServer. It is assumed that FakeServer
  // is being used and each bookmark has a unique title. Folders are not
  // verified.
  void VerifyBookmarkModelMatchesFakeServer(int index);

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientBookmarksSyncTest);
};

void SingleClientBookmarksSyncTest::VerifyBookmarkModelMatchesFakeServer(
    int index) {
  fake_server::FakeServerVerifier fake_server_verifier(GetFakeServer());
  std::vector<UrlAndTitle> local_bookmarks;
  GetBookmarkModel(index)->GetBookmarks(&local_bookmarks);

  // Verify that all local bookmark titles exist once on the server.
  std::vector<UrlAndTitle>::const_iterator it;
  for (it = local_bookmarks.begin(); it != local_bookmarks.end(); ++it) {
    ASSERT_TRUE(fake_server_verifier.VerifyEntityCountByTypeAndName(
        1,
        syncer::BOOKMARKS,
        base::UTF16ToUTF8(it->title)));
  }
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, Sanity) {
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
  const BookmarkNode* top = AddFolder(
      kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, "top");
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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  const BookmarkNode* porsche = AddURL(
      kSingleProfileIndex, bar, 2, "Porsche", GURL("http://www.porsche.com"));
  // Rearrange stuff in tier1_a.
  ASSERT_EQ(tier1_a, tier1_a_url2->parent());
  ASSERT_EQ(tier1_a, tier1_a_url1->parent());
  Move(kSingleProfileIndex, tier1_a_url2, tier1_a, 0);
  Move(kSingleProfileIndex, tier1_a_url1, tier1_a, 2);

  // Wait for the rearranged hierarchy to sync.
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  // Only verify FakeServer data if FakeServer is being used.
  // TODO(pvalenzuela): Use this style of verification in more tests once it is
  // proven stable.
  if (GetFakeServer())
    VerifyBookmarkModelMatchesFakeServer(kSingleProfileIndex);
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, CommitLocalCreations) {
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
  const BookmarkNode* top = AddFolder(
      kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, "top");
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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  EXPECT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, InjectedBookmark) {
  std::string title = "Montreal Canadiens";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(
      GURL("http://canadiens.nhl.com")));

  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());

  EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title));
}

// Test that a client doesn't mutate the favicon data in the process
// of storing the favicon data from sync to the database or in the process
// of requesting data from the database for sync.
IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       SetFaviconHiDPIDifferentCodec) {
  // Set the supported scale factors to 1x and 2x such that
  // BookmarkModel::GetFavicon() requests both 1x and 2x.
  // 1x -> for sync, 2x -> for the UI.
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::SetSupportedScaleFactors(supported_scale_factors);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

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
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

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
IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, DeleteFaviconFromSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon.ico");
  const BookmarkNode* bookmark = AddURL(kSingleProfileIndex, "title", page_url);
  SetFavicon(0, bookmark, icon_url, CreateFavicon(SK_ColorWHITE),
             bookmarks_helper::FROM_UI);
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  // Simulate receiving a favicon deletion from sync.
  DeleteFaviconMappings(kSingleProfileIndex, bookmark,
                        bookmarks_helper::FROM_SYNC);

  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  CheckHasNoFavicon(kSingleProfileIndex, page_url);
  EXPECT_TRUE(
      GetBookmarkModel(kSingleProfileIndex)->GetFavicon(bookmark).IsEmpty());
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  // Remove all bookmarks and wait for sync completion.
  RemoveAll(kSingleProfileIndex);
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  // Verify other node has no children now.
  EXPECT_TRUE(GetOtherNode(kSingleProfileIndex)->children().empty());
  EXPECT_TRUE(GetBookmarkBarNode(kSingleProfileIndex)->children().empty());
  // Verify model matches verifier.
  ASSERT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, DownloadDeletedBookmark) {
  std::string title = "Patrick Star";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(
      GURL("http://en.wikipedia.org/wiki/Patrick_Star")));

  DisableVerifier();
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

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       DownloadModifiedBookmark) {
  std::string title = "Syrup";
  GURL original_url = GURL("https://en.wikipedia.org/?title=Maple_syrup");
  GURL updated_url = GURL("https://en.wikipedia.org/wiki/Xylem");

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(original_url));

  DisableVerifier();
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

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, DownloadBookmarkFolder) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       DownloadLegacyBookmarkFolder) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder(/*is_legacy=*/true));

  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));
}

// Legacy bookmark clients append a blank space to empty titles, ".", ".." tiles
// before committing them because historically they were illegal server titles.
// This test makes sure that this functionality is implemented for backward
// compatibility with legacy clients.
IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ShouldCommitBookmarksWithIllegalServerNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const std::vector<std::string> illegal_titles = {"", ".", ".."};
  // Create 3 bookmarks under the bookmark bar with illegal titles.
  for (const std::string& illegal_title : illegal_titles) {
    ASSERT_TRUE(AddURL(kSingleProfileIndex, illegal_title,
                       GURL("http://www.google.com")));
  }

  // Wait till all entities are committed.
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());

  // Collect the titles committed on the server.
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  std::vector<std::string> committed_titles;
  for (const sync_pb::SyncEntity& entity : entities) {
    committed_titles.push_back(entity.specifics().bookmark().title());
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
IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ShouldCreateLocalBookmarksWithIllegalServerNames) {
  const std::vector<std::string> illegal_titles = {"", ".", ".."};

  // Create 3 bookmarks on the server under BookmarkBar with illegal server
  // titles with a blank space appended to simulate a commit from a legacy
  // client.
  fake_server::EntityBuilderFactory entity_builder_factory;
  for (const std::string& illegal_title : illegal_titles) {
    fake_server::BookmarkEntityBuilder bookmark_builder =
        entity_builder_factory.NewBookmarkEntityBuilder(illegal_title + " ");
    fake_server_->InjectEntity(
        bookmark_builder.BuildBookmark(GURL("http://www.google.com")));
  }

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // There should be bookmark with illegal title (without the appended space).
  for (const std::string& illegal_title : illegal_titles) {
    EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex,
                                                   illegal_title));
  }
}

// Legacy bookmark clients append a blank space to empty titles. This tests that
// this is respected when merging local and remote hierarchies.
IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ShouldTruncateBlanksWhenMatchingTitles) {
  const std::string remote_blank_title = " ";
  const std::string local_empty_title = "";

  // Create a folder on the server under BookmarkBar with a title with a blank
  // space.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(remote_blank_title);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  DisableVerifier();
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
IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
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

  DisableVerifier();
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

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
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

  DisableVerifier();
  ASSERT_TRUE(SetupClients());

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

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest, E2E_ONLY(SanitySetup)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
}

IN_PROC_BROWSER_TEST_P(
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
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  EXPECT_TRUE(ModelMatchesVerifier(kSingleProfileIndex));

  // There should have been one creation and no deletions.
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_CREATION=*/1));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.BOOKMARK",
                                         /*LOCAL_DELETION=*/0));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
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

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       PersistProgressMarkerOnRestart) {
  const std::string title = "Seattle Sounders FC";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  bookmark_builder.SetId(
      syncer::LoopbackServerEntity::CreateId(syncer::BOOKMARKS, kBookmarkGuid));
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
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

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithValidGUID) {
  // Start syncing.
  DisableVerifier();
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  // Create a bookmark folder with a valid GUID.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Issue remote creation with a valid GUID.
  std::unique_ptr<syncer::LoopbackServerEntity> folder =
      bookmark_builder.BuildFolder(/*is_legacy=*/false);
  const std::string guid = folder.get()->GetSpecifics().bookmark().guid();
  ASSERT_TRUE(base::IsValidGUID(guid));
  ASSERT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, guid));
  fake_server_->InjectEntity(std::move(folder));

  // A folder should have been added with the corresponding GUID.
  EXPECT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, guid).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(
      guid,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid());
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithoutValidGUID) {
  // Start syncing.
  DisableVerifier();
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  const std::string originator_client_item_id = base::GenerateGUID();
  ASSERT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex,
                                            originator_client_item_id));

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id);

  // Issue remote creation without a valid GUID but with a valid
  // originator_client_item_id.
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
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithoutValidGUIDOrOCII) {
  // Start syncing.
  DisableVerifier();
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
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/true);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // A bookmark should have been added with a newly assigned valid GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(base::IsValidGUID(
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid()));
  EXPECT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex,
                                            originator_client_item_id));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithValidGUID) {
  const GURL url = GURL("http://www.foo.com");
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Create bookmark in server with a valid GUID.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/false);
  const std::string guid = bookmark.get()->GetSpecifics().bookmark().guid();
  ASSERT_TRUE(base::IsValidGUID(guid));
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with the corresponding GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, guid));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithoutValidGUID) {
  const GURL url = GURL("http://www.foo.com");
  const std::string originator_client_item_id = base::GenerateGUID();
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id);

  // Create bookmark in server without a valid GUID but with a valid
  // originator_client_item_id.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/true);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with the originator_client_item_id as the
  // GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex,
                                           originator_client_item_id));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
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
  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with a newly assigned valid GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(base::IsValidGUID(
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid()));
  EXPECT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex,
                                            originator_client_item_id));
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       ApplyRemoteUpdateWithValidGUID) {
  // Create a bookmark.
  const GURL url = GURL("https://foo.com");
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Issue remote creation.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.BuildBookmark(url, /*is_legacy=*/false);
  const std::string old_guid = bookmark.get()->GetSpecifics().bookmark().guid();
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  DisableVerifier();
  ASSERT_TRUE(SetupSync());

  // Created bookmark should be in the model.
  ASSERT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_EQ(
      old_guid,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid());

  // Change bookmark GUID in the server to simulate an update from another
  // client that has a differente GUID assigned to this bookmark. This can
  // happen because every client assigns GUIDs independently when the GUID is
  // missing, and the values should eventually be consistent across clients.
  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::BOOKMARKS);
  ASSERT_EQ(1ul, server_bookmarks.size());
  sync_pb::EntitySpecifics specifics = server_bookmarks[0].specifics();
  const std::string new_guid = base::GenerateGUID();
  specifics.mutable_bookmark()->set_guid(new_guid);
  ASSERT_TRUE(GetFakeServer()->ModifyEntitySpecifics(
      /*entity_id=*/server_bookmarks[0].id_string(), specifics));

  // The bookmark GUID should have been updated with the corresponding value.
  EXPECT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, new_guid).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(
      new_guid,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->guid());
}

IN_PROC_BROWSER_TEST_P(SingleClientBookmarksSyncTest,
                       MergeRemoteUpdateWithValidGUID) {
  DisableVerifier();
  ASSERT_TRUE(SetupClients());

  // Create a local bookmark folder.
  const std::string title = "Seattle Sounders FC";
  const BookmarkNode* local_folder = AddFolder(
      kSingleProfileIndex, GetBookmarkBarNode(kSingleProfileIndex), 0, title);
  const std::string old_guid = local_folder->guid();
  ASSERT_TRUE(local_folder);
  ASSERT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, old_guid).Wait());
  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  // Create an equivalent remote folder.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.BuildFolder(/*is_legacy=*/false);
  const std::string new_guid = remote_folder->GetSpecifics().bookmark().guid();
  fake_server_->InjectEntity(std::move(remote_folder));

  // Start syncing.
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // The folder GUID should have been updated with the corresponding value.
  EXPECT_TRUE(BookmarksGUIDChecker(kSingleProfileIndex, new_guid).Wait());
  EXPECT_FALSE(ContainsBookmarkNodeWithGUID(kSingleProfileIndex, old_guid));
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
}

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientBookmarksSyncTest,
                         ::testing::Values(false, true));
}  // namespace
