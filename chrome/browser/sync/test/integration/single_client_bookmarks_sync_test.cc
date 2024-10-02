// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/committed_all_nudged_changes_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/bookmark_update_preprocessing.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/invalidations/interested_data_types_handler.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/bookmark_entity_builder.h"
#include "components/sync/test/entity_builder_factory.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "components/sync/test/fake_server_verifier.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::BookmarkFaviconLoadedChecker;
using bookmarks_helper::BookmarkModelMatchesFakeServerChecker;
using bookmarks_helper::BookmarksTitleChecker;
using bookmarks_helper::BookmarksUrlChecker;
using bookmarks_helper::BookmarksUuidChecker;
using bookmarks_helper::CheckHasNoFavicon;
using bookmarks_helper::ContainsBookmarkNodeWithUuid;
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
using bookmarks_helper::SetFavicon;
using bookmarks_helper::SetTitle;
using BookmarkGeneration =
    fake_server::BookmarkEntityBuilder::BookmarkGeneration;
using syncer::MatchesDeletionOrigin;
using testing::AllOf;
using testing::Contains;
using testing::Each;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::Pointee;
using testing::SizeIs;

// All tests in this file utilize a single profile.
// TODO(pvalenzuela): Standardize this pattern by moving this constant to
// SyncTest and using it in all single client tests.
const int kSingleProfileIndex = 0;

#if !BUILDFLAG(IS_ANDROID)
// An arbitrary GUID, to be used for injecting the same bookmark entity to the
// fake server across PRE_MyTest and MyTest.
const char kBookmarkGuid[] = "e397ed62-9532-4dbf-ae55-200236eba15c";
#endif  // !BUILDFLAG(IS_ANDROID)

// A title and a URL which are used across PRE_MyTest and MyTest.
const char kBookmarkTitle[] = "Title";
const char kBookmarkPageUrl[] = "http://www.foo.com/";

MATCHER(HasUniquePosition, "") {
  return arg.specifics().bookmark().has_unique_position();
}

// Fake device info sync service that does the necessary setup to be used in a
// SyncTest. It basically disables DEVICE_INFO commits.
class FakeDeviceInfoSyncServiceWithInvalidations
    : public syncer::FakeDeviceInfoSyncService,
      public syncer::InterestedDataTypesHandler {
 public:
  explicit FakeDeviceInfoSyncServiceWithInvalidations(
      syncer::SyncInvalidationsService* sync_invalidations_service)
      : syncer::FakeDeviceInfoSyncService(/*skip_engine_connection=*/true),
        sync_invalidations_service_(sync_invalidations_service) {
    sync_invalidations_service_->SetInterestedDataTypesHandler(this);
  }
  ~FakeDeviceInfoSyncServiceWithInvalidations() override {
    sync_invalidations_service_->SetInterestedDataTypesHandler(nullptr);
  }

  // InterestedDataTypesHandler implementation.
  void OnInterestedDataTypesChanged() override {}
  void SetCommittedAdditionalInterestedDataTypesCallback(
      base::RepeatingCallback<void(const syncer::DataTypeSet&)> callback)
      override {}

 private:
  const raw_ptr<syncer::SyncInvalidationsService> sync_invalidations_service_;
};

std::unique_ptr<KeyedService> BuildFakeDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<FakeDeviceInfoSyncServiceWithInvalidations>(
      SyncInvalidationsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

// Waits until the tasks posted by the error handler have been processed.
class BookmarksDataTypeErrorChecker : public SingleClientStatusChangeChecker {
 public:
  using SingleClientStatusChangeChecker::SingleClientStatusChangeChecker;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for Bookmarks data type error.";
    return service()->HasAnyModelErrorForTest({syncer::BOOKMARKS});
  }
};

class SingleClientBookmarksSyncTest : public SyncTest {
 public:
  SingleClientBookmarksSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientBookmarksSyncTest(const SingleClientBookmarksSyncTest&) = delete;
  SingleClientBookmarksSyncTest& operator=(
      const SingleClientBookmarksSyncTest&) = delete;

  ~SingleClientBookmarksSyncTest() override = default;
};

class SingleClientBookmarksSyncTestWithEnabledReuploadBookmarks
    : public SyncTest {
 public:
  SingleClientBookmarksSyncTestWithEnabledReuploadBookmarks()
      : SyncTest(SINGLE_CLIENT) {
    features_override_.InitAndEnableFeature(switches::kSyncReuploadBookmarks);
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

class SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks
    : public SyncTest {
 public:
  SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks()
      : SyncTest(SINGLE_CLIENT) {
    features_override_.InitAndDisableFeature(switches::kSyncReuploadBookmarks);
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

class SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks
    : public SingleClientBookmarksSyncTest {
 public:
  SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks() {
    features_override_.InitWithFeatureState(switches::kSyncReuploadBookmarks,
                                            !content::IsPreTest());
  }

 private:
  base::test::ScopedFeatureList features_override_;
};

class SingleClientBookmarksThrottlingSyncTest : public SyncTest {
 public:
  SingleClientBookmarksThrottlingSyncTest() : SyncTest(SINGLE_CLIENT) {}

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SingleClientBookmarksThrottlingSyncTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Fake DeviceInfoSyncService with its fake bridge for device info to make
    // sure there are no device info commits interfering with the extended nudge
    // delay.
    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildFakeDeviceInfoSyncService));
  }

  // Rarely, self notification for nigori interferes with the
  // DepleteQuotaAndRecover test causing the tested commit to happen too early.
  bool TestUsesSelfNotifications() override { return false; }

  void SetupBookmarksSync() {
    // Only enable bookmarks so that sync is not nudged by another data type
    // (with a shorter delay).
    ASSERT_TRUE(GetClient(0)->SetupSync(
        base::BindOnce([](syncer::SyncUserSettings* user_settings) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
          user_settings->SetSelectedOsTypes(false, {});
#endif
          user_settings->SetSelectedTypes(
              false, {syncer::UserSelectableType::kBookmarks});
        })));
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
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
  const BookmarkNode* cnn =
      AddURL(kSingleProfileIndex, bar, 0, "CNN", GURL("http://www.cnn.com"));
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

  const BookmarkNode* porsche = AddURL(kSingleProfileIndex, bar, 2, "Porsche",
                                       GURL("http://www.porsche.com"));
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

  ASSERT_EQ(1u, tier1_a_url0->parent()->GetIndexOf(tier1_a_url0));
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
  const BookmarkNode* wired = AddURL(kSingleProfileIndex, bar, 2, "Wired News",
                                     GURL("http://www.wired.com"));
  const BookmarkNode* tier2_b =
      AddFolder(kSingleProfileIndex, tier1_b, 0, "tier2_b");
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
  const BookmarkNode* tier3_b =
      AddFolder(kSingleProfileIndex, tier2_b, 1, "tier3_b");
  const BookmarkNode* leafs =
      AddURL(kSingleProfileIndex, tier1_a, 0, "Toronto Maple Leafs",
             GURL("http://mapleleafs.nhl.com"));
  const BookmarkNode* wynn = AddURL(kSingleProfileIndex, bar, 1, "Wynn",
                                    GURL("http://www.wynnlasvegas.com"));

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
  fake_server_->InjectEntity(
      bookmark_builder.BuildBookmark(GURL("http://canadiens.nhl.com")));

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
      title1, /*originator_cache_guid=*/
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      /*originator_client_item_id=*/"1");
  fake_server::BookmarkEntityBuilder bookmark_builder2(
      title2, /*originator_cache_guid=*/
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      /*originator_client_item_id=*/"1");

  fake_server_->InjectEntity(
      bookmark_builder1
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL("http://page1.com")));
  fake_server_->InjectEntity(
      bookmark_builder2
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL("http://page2.com")));

  ASSERT_TRUE(SetupSync());

  EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title1));
  EXPECT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title2));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadLegacyUppercaseGuid2016BookmarksAndCommit) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::string uppercase_uuid_str =
      base::ToUpperASCII(uuid.AsLowercaseString());
  const std::string title1 = "Title1";
  const std::string title2 = "Title2";

  // Bookmarks created around 2016, between [M44..M52) use an uppercase UUID as
  // originator client item ID.
  fake_server::BookmarkEntityBuilder bookmark_builder(
      title1, /*originator_cache_guid=*/
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      /*originator_client_item_id=*/uppercase_uuid_str);

  fake_server_->InjectEntity(
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL("http://page1.com")));

  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(1u, CountBookmarksWithTitlesMatching(kSingleProfileIndex, title1));

  // The UUID should have been canonicalized (lowercased) in BookmarkModel.
  EXPECT_TRUE(ContainsBookmarkNodeWithUuid(kSingleProfileIndex, uuid));

  // Changing the title should populate the server-side UUID in specifics in
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

  // Verify the UUID that was committed to the server.
  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks.size());
  ASSERT_EQ(
      title2,
      server_bookmarks[0].specifics().bookmark().legacy_canonicalized_title());
  EXPECT_EQ(uuid.AsLowercaseString(),
            server_bookmarks[0].specifics().bookmark().guid());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       DownloadModernBookmarkCollidingPre2015BookmarkId) {
  const std::string title1 = "Title1";
  const std::string title2 = "Title2";

  const std::string kOriginalOriginatorCacheUuid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string kOriginalOriginatorClientItemId = "1";

  // One pre-2015 bookmark, nothing special here.
  fake_server::BookmarkEntityBuilder bookmark_builder1(
      title1, kOriginalOriginatorCacheUuid, kOriginalOriginatorClientItemId);

  // A second bookmark, possibly uploaded by a buggy client, happens to use an
  // originator client item ID that collides with the UUID that would have been
  // inferred for the original pre-2015 bookmark.
  fake_server::BookmarkEntityBuilder bookmark_builder2(
      title2, /*originator_cache_guid=*/
      base::Uuid::GenerateRandomV4().AsLowercaseString(),
      /*originator_client_item_id=*/
      syncer::InferGuidForLegacyBookmarkForTesting(
          kOriginalOriginatorCacheUuid, kOriginalOriginatorClientItemId));

  fake_server_->InjectEntity(
      bookmark_builder1
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL("http://page1.com")));
  fake_server_->InjectEntity(
      bookmark_builder2
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL("http://page2.com")));

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
  ui::SetSupportedResourceScaleFactors({ui::k100Percent, ui::k200Percent});

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const GURL page_url("http://www.google.com");
  const GURL icon_url("http://www.google.com/favicon.ico");
  const BookmarkNode* bookmark = AddURL(kSingleProfileIndex, "title", page_url);

  // Simulate receiving a favicon from sync encoded by a different PNG encoder
  // than the one native to the OS. This tests the PNG data is not decoded to
  // SkBitmap (or any other image format) then encoded back to PNG on the path
  // between sync and the database.
  gfx::Image original_favicon =
      Create1xFaviconFromPNGFile("favicon_cocoa_png_codec.png");
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

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest, OneFolderRemovedEvent) {
  ASSERT_TRUE(SetupClients());
  // Starting state:
  // other_node
  //    -> folder0
  //      -> http://yahoo.com
  //    -> http://www.cnn.com
  // bookmark_bar

  const BookmarkNode* folder0 = AddFolder(
      kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, "folder0");
  ASSERT_TRUE(AddURL(kSingleProfileIndex, folder0, 0, "Yahoo",
                     GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 1,
                     "CNN", GURL("http://www.cnn.com")));

  // Set up sync, wait for its completion and verify that changes propagated.
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(2u, GetOtherNode(kSingleProfileIndex)->children().size());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  // Remove one folder and wait for sync completion.
  const base::Location kDeletionLocation = FROM_HERE;
  GetBookmarkModel(kSingleProfileIndex)
      ->Remove(folder0, bookmarks::metrics::BookmarkEditSource::kOther,
               kDeletionLocation);
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  EXPECT_EQ(1u, GetOtherNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  // The folder contained one bookmark inside, so two deletions should have been
  // recorded.
  EXPECT_THAT(
      GetFakeServer()->GetCommittedDeletionOrigins(syncer::DataType::BOOKMARKS),
      AllOf(SizeIs(2),
            Each(MatchesDeletionOrigin(version_info::GetVersionNumber(),
                                       kDeletionLocation))));
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
  const base::Location kDeletionLocation = FROM_HERE;
  GetBookmarkModel(kSingleProfileIndex)
      ->RemoveAllUserBookmarks(kDeletionLocation);
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  EXPECT_THAT(
      GetFakeServer()->GetCommittedDeletionOrigins(syncer::DataType::BOOKMARKS),
      AllOf(SizeIs(11),
            Each(MatchesDeletionOrigin(version_info::GetVersionNumber(),
                                       kDeletionLocation))));

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
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
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
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
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
  fake_server_->InjectEntity(
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildFolder());

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
      fake_server_->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
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
    fake_server_->InjectEntity(
        bookmark_builder
            .SetGeneration(BookmarkGeneration::kValidGuidAndLegacyTitle)
            .BuildBookmark(GURL("http://www.google.com")));
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
  fake_server_->InjectEntity(
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kValidGuidAndLegacyTitle)
          .BuildFolder());

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
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
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
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.BOOKMARK",
                   syncer::DataTypeEntityChange::kLocalCreation));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.BOOKMARK",
                   syncer::DataTypeEntityChange::kLocalDeletion));
}

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
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

  EXPECT_NE(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.BOOKMARK",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
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

  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitSyncSetupCompletion());

  // After restart, the last sync cycle snapshot should be empty.
  // Once a sync request happened (e.g. by a poll), that snapshot is populated.
  // We use the following checker to simply wait for an non-empty snapshot.
  GetSyncService(0)->TriggerRefresh({syncer::BOOKMARKS});
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.BOOKMARK",
                   syncer::DataTypeEntityChange::kRemoteInitialUpdate));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithValidUuid) {
  // Start syncing.
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  // Create a bookmark folder with a valid UUID.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Issue remote creation with a valid UUID.
  base::HistogramTester histogram_tester;
  std::unique_ptr<syncer::LoopbackServerEntity> folder =
      bookmark_builder.BuildFolder();
  const base::Uuid uuid = base::Uuid::ParseCaseInsensitive(
      folder.get()->GetSpecifics().bookmark().guid());
  ASSERT_TRUE(uuid.is_valid());
  ASSERT_FALSE(ContainsBookmarkNodeWithUuid(kSingleProfileIndex, uuid));
  fake_server_->InjectEntity(std::move(folder));

  // A folder should have been added with the corresponding UUID.
  EXPECT_TRUE(BookmarksUuidChecker(kSingleProfileIndex, uuid).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(
      uuid,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->uuid());
  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kSpecifics=*/0));

  EXPECT_NE(0U,
            histogram_tester
                .GetAllSamples(
                    "Sync.NonReflectionUpdateFreshnessPossiblySkewed2.BOOKMARK")
                .size());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ApplyRemoteCreationWithoutValidGUID) {
  // Start syncing.
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());

  const base::Uuid originator_client_item_id = base::Uuid::GenerateRandomV4();
  ASSERT_FALSE(ContainsBookmarkNodeWithUuid(kSingleProfileIndex,
                                            originator_client_item_id));

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id.AsLowercaseString());

  // Issue remote creation without a valid GUID but with a valid
  // originator_client_item_id.
  base::HistogramTester histogram_tester;
  std::unique_ptr<syncer::LoopbackServerEntity> folder =
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildFolder();
  ASSERT_TRUE(folder.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(folder));

  // A bookmark folder should have been added with the originator_client_item_id
  // as the GUID.
  EXPECT_TRUE(
      BookmarksUuidChecker(kSingleProfileIndex, originator_client_item_id)
          .Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_EQ(
      originator_client_item_id,
      GetBookmarkBarNode(kSingleProfileIndex)->children()[0].get()->uuid());

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
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(url);
  ASSERT_TRUE(bookmark.get()->GetSpecifics().bookmark().guid().empty());
  fake_server_->InjectEntity(std::move(bookmark));

  // A bookmark should have been added with a newly assigned valid GUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_FALSE(ContainsBookmarkNodeWithUuid(
      kSingleProfileIndex,
      base::Uuid::ParseCaseInsensitive(originator_client_item_id)));

  EXPECT_EQ(1, histogram_tester.GetBucketCount("Sync.BookmarkGUIDSource2",
                                               /*kInferred=*/3));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithValidUuid) {
  const GURL url = GURL("http://www.foo.com");
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Seattle Sounders FC");

  // Create bookmark in server with a valid UUID.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kHierarchyFieldsInSpecifics)
          .BuildBookmark(url);
  const base::Uuid uuid = base::Uuid::ParseCaseInsensitive(
      bookmark.get()->GetSpecifics().bookmark().guid());
  ASSERT_TRUE(uuid.is_valid());
  fake_server_->InjectEntity(std::move(bookmark));

  // Start syncing.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients());
  ASSERT_EQ(0u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // A bookmark should have been added with the corresponding UUID.
  EXPECT_TRUE(BookmarksUrlChecker(kSingleProfileIndex, url, 1).Wait());
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  EXPECT_TRUE(ContainsBookmarkNodeWithUuid(kSingleProfileIndex, uuid));

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
      fake_server_->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks_before.size());

  // Remove the node and undo the action.
  bookmark_model->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                         FROM_HERE);
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
      fake_server_->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks_after.size());
  EXPECT_GT(server_bookmarks_after.front().version(),
            server_bookmarks_before.front().version());
  EXPECT_EQ(server_bookmarks_after.front().id_string(),
            server_bookmarks_before.front().id_string());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       MergeRemoteCreationWithoutValidGUID) {
  const GURL url = GURL("http://www.foo.com");
  const base::Uuid originator_client_item_id = base::Uuid::GenerateRandomV4();
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          "Seattle Sounders FC", originator_client_item_id.AsLowercaseString());

  // Create bookmark in server without a valid GUID but with a valid
  // originator_client_item_id.
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(url);
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
  EXPECT_TRUE(ContainsBookmarkNodeWithUuid(kSingleProfileIndex,
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
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(url);
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
  EXPECT_FALSE(ContainsBookmarkNodeWithUuid(
      kSingleProfileIndex,
      base::Uuid::ParseCaseInsensitive(originator_client_item_id)));

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
  const base::Uuid old_uuid = local_folder->uuid();
  SCOPED_TRACE(std::string("old_uuid=") + old_uuid.AsLowercaseString());

  ASSERT_TRUE(local_folder);
  ASSERT_TRUE(BookmarksUuidChecker(kSingleProfileIndex, old_uuid).Wait());
  ASSERT_EQ(1u, CountFoldersWithTitlesMatching(kSingleProfileIndex, title));

  // Create an equivalent remote folder.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.BuildFolder();
  const base::Uuid new_uuid = base::Uuid::ParseCaseInsensitive(
      remote_folder->GetSpecifics().bookmark().guid());
  fake_server_->InjectEntity(std::move(remote_folder));

  // Start syncing.
  ASSERT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
  ASSERT_TRUE(SetupSync());

  // The folder UUID should have been updated with the corresponding value.
  EXPECT_TRUE(BookmarksUuidChecker(kSingleProfileIndex, new_uuid).Wait());
  EXPECT_FALSE(ContainsBookmarkNodeWithUuid(kSingleProfileIndex, old_uuid));
  EXPECT_EQ(1u, GetBookmarkBarNode(kSingleProfileIndex)->children().size());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTest,
    ShouldReportErrorIfIncrementalLocalCreationCrossesMaxCountLimit) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set a limit of 4 bookmarks. This is to avoid erroring out when the fake
  // server sends an update of size 4.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));

  // Add 2 new bookmarks to exceed the limit.
  const BookmarkNode* bookmark_bar_node =
      GetBookmarkBarNode(kSingleProfileIndex);

  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/bookmark_bar_node, /*index=*/0, kTitle1,
                     GURL(kUrl1)));

  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/bookmark_bar_node, /*index=*/1, kTitle2,
                     GURL(kUrl2)));

  // Creation of 2 local bookmarks should exceed the limit of 4.
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());
  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldReportErrorIfBookmarksCountExceedsLimitOnStartup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set a limit of 4 bookmarks. This is to avoid erroring out when the fake
  // server sends an update of size 4.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);

  // Add 2 new bookmarks to exceed the limit.
  const BookmarkNode* bookmark_bar_node =
      GetBookmarkBarNode(kSingleProfileIndex);

  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/bookmark_bar_node, /*index=*/0, kTitle1,
                     GURL(kUrl1)));

  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/bookmark_bar_node, /*index=*/1, kTitle2,
                     GURL(kUrl2)));

  ASSERT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // We now have 5 local bookmarks(3 permanent + 2 added), which exceeds our
  // limit of 4 bookmarks.
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());
  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTest,
    ShouldReportErrorIfBookmarksCountExceedsLimitAfterInitialUpdate) {
  // Create a bookmark on the server under BookmarkBar with a truncated title.
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle1);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(GURL(kUrl1)));

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Set a limit of 5 bookmarks. This is to avoid erroring out when the fake
  // server sends an update of size 5.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(5);

  // Set up 2 preexisting local bookmark under other node.
  const BookmarkNode* other_node = GetOtherNode(kSingleProfileIndex);

  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/other_node, /*index=*/0, kTitle2, GURL(kUrl2)));

  const std::string kTitle3 = "title3";
  const std::string kUrl3 = "http://www.url3.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/other_node, /*index=*/0, kTitle3, GURL(kUrl3)));

  ASSERT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // We should now have 6 local bookmarks, (3 permanent + 2 locally added + 1
  // from remote), which exceeeds our limit of 5 bookmarks.
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());
  // Note that remote bookmarks being added, even though we error out, is the
  // current behaviour and is not a requirement.
  EXPECT_THAT(GetBookmarkBarNode(kSingleProfileIndex)->children(),
              ElementsAre(bookmarks_helper::IsUrlBookmarkWithTitleAndUrl(
                  kTitle1, GURL(kUrl1))));
  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTest,
    ShouldReportErrorIfBookmarksCountExceedsLimitAfterIncrementalUpdate) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Set a limit of 4 bookmarks. This is to avoid erroring out when the fake
  // server sends an update of size 4.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);

  // Set up a preexisting local bookmark under other node.
  const BookmarkNode* other_node = GetOtherNode(kSingleProfileIndex);

  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/other_node, /*index=*/0, kTitle1, GURL(kUrl1)));

  ASSERT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Create a bookmark on the server under BookmarkBar.
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle2);
  fake_server_->InjectEntity(bookmark_builder.BuildBookmark(GURL(kUrl2)));

  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());
  // Note that remote bookmarks being added, even though we error out, is the
  // current behaviour and is not a requirement.
  EXPECT_THAT(GetBookmarkBarNode(kSingleProfileIndex)->children(),
              ElementsAre(bookmarks_helper::IsUrlBookmarkWithTitleAndUrl(
                  kTitle2, GURL(kUrl2))));
  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldReportErrorIfInitialUpdatesCrossMaxCountLimit) {
  // Create two bookmarks on the server under BookmarkBar with a truncated
  // title.
  fake_server::EntityBuilderFactory entity_builder_factory;
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  fake_server_->InjectEntity(
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle1).BuildBookmark(
          GURL(kUrl1)));

  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  fake_server_->InjectEntity(
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle2).BuildBookmark(
          GURL(kUrl2)));

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Set a limit of 4 bookmarks. This should result in an error when we get an
  // update of size 5.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);

  ASSERT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Update of size 5 exceeds the limit.
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());
  EXPECT_TRUE(GetBookmarkBarNode(kSingleProfileIndex)->children().empty());
  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

// Android doesn't currently support PRE_ tests, see crbug.com/40200835 or
// crbug.com/40145099.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithDisabledReuploadBookmarks,
    PRE_ShouldNotReploadUponFaviconLoad) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder("Foo Title");

  // Create a legacy bookmark on the server (no |type|, |unique_position| fields
  // populated). The fact that it's a legacy bookmark means any locally-produced
  // specifics would be different for this bookmark (new fields like |type|
  // would be populated).
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmark_builder.SetGeneration(BookmarkGeneration::kValidGuidAndFullTitle)
          .BuildBookmark(GURL(kBookmarkPageUrl));
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

  ASSERT_TRUE(BookmarkFaviconLoadedChecker(0, GURL(kBookmarkPageUrl)).Wait());
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{title, GURL(kBookmarkPageUrl)}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // Mimic the user being offline (until the next restart), to make sure the
  // entity is unsync-ed upon browser startup (next test).
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  SetTitle(kSingleProfileIndex, bookmark, new_title);
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       ShouldUploadUnsyncedEntityAfterRestart) {
  const std::string title = "Title";
  const std::string new_title = "New Title";
  const GURL url = GURL(kBookmarkPageUrl);

  // Ensure that the server bookmark has the old title.
  const std::vector<sync_pb::SyncEntity> server_bookmarks_before =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks_before.size());
  ASSERT_EQ(title, server_bookmarks_before.front()
                       .specifics()
                       .bookmark()
                       .legacy_canonicalized_title());

  // Ensure that local bookmark has the new title.
  ASSERT_TRUE(SetupClients());
  const BookmarkNode* node =
      bookmarks_helper::GetUniqueNodeByURL(kSingleProfileIndex, url);
  ASSERT_EQ(1u,
            CountBookmarksWithTitlesMatching(kSingleProfileIndex, new_title));

  // Ensure that there is a favicon on the server and local node haven't started
  // loading of favicon.
  ASSERT_TRUE(
      server_bookmarks_before.front().specifics().bookmark().has_favicon());
  ASSERT_FALSE(node->is_favicon_loading());
  ASSERT_FALSE(node->is_favicon_loaded());

  // Ensure that the new title eventually makes it to the server.
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{new_title, url}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // Last commit should initiate favicon loading.
  ASSERT_TRUE(BookmarkFaviconLoadedChecker(kSingleProfileIndex, url).Wait());
  ASSERT_TRUE(
      UpdatedProgressMarkerChecker(GetSyncService(kSingleProfileIndex)).Wait());
  const std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks.size());

  // Once loaded, the favicon must be uploaded to the server.
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
      CommittedAllNudgedChangesChecker(GetSyncService(kSingleProfileIndex))
          .Wait());

  // Verify that the bookmark hasn't been uploaded (no local updates issued). No
  // commits are expected despite the fact that the server-side bookmark is a
  // legacy bookmark without the most recent fields (e.g. GUID), because loading
  // favicons should not lead to commits unless the favicon itself changed.
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Sync.DataTypeEntityChange.BOOKMARK",
                   syncer::DataTypeEntityChange::kLocalUpdate));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadBookmarks,
    ShouldReuploadBookmarkAfterInitialMerge) {
  ASSERT_TRUE(SetupClients());

  const std::string title = "Title";

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kValidGuidAndLegacyTitle)
          .BuildFolder();
  ASSERT_FALSE(remote_folder->GetSpecifics().bookmark().has_unique_position());
  fake_server_->InjectEntity(std::move(remote_folder));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{title, /*url=*/GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());
}

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
// Initiate reupload after restart when the feature toggle has been just enabled
// (before restart the entity is in synced state).
IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks,
    PRE_ShouldReuploadForOldClients) {
  ASSERT_TRUE(SetupSync());

  // Make an incremental remote creation of bookmark without |unique_position|.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kBookmarkTitle);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.SetGeneration(BookmarkGeneration::kValidGuidAndFullTitle)
          .BuildBookmark(GURL(kBookmarkPageUrl));

  fake_server_->InjectEntity(std::move(remote_folder));

  ASSERT_TRUE(BookmarksTitleChecker(kSingleProfileIndex, kBookmarkTitle,
                                    /*expected_count=*/1)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadPreexistingBookmarks,
    ShouldReuploadForOldClients) {
  // This test checks that the legacy bookmark which was stored locally will
  // imply reupload to the server when reupload feature is enabled.
  ASSERT_EQ(
      1u, GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS).size());
  ASSERT_FALSE(GetFakeServer()
                   ->GetSyncEntitiesByDataType(syncer::BOOKMARKS)
                   .front()
                   .specifics()
                   .bookmark()
                   .has_unique_position());
  // |parent_guid| is used by BookmarkModelMatchesFakeServerChecker. It was
  // introduced with |unique_position| and both fields should be reuploaded
  // simultaneously.
  ASSERT_FALSE(GetFakeServer()
                   ->GetSyncEntitiesByDataType(syncer::BOOKMARKS)
                   .front()
                   .specifics()
                   .bookmark()
                   .has_parent_guid());

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitSyncSetupCompletion());

  // Run a sync cycle to trigger bookmarks reupload on browser startup. This is
  // required since bookmarks get reuploaded only after the latest changes are
  // downloaded to avoid uploading outdated data.
  GetSyncService(kSingleProfileIndex)->TriggerRefresh({syncer::BOOKMARKS});

  // Bookmark favicon will be loaded if there are local changes.
  ASSERT_TRUE(
      BookmarkFaviconLoadedChecker(kSingleProfileIndex, GURL(kBookmarkPageUrl))
          .Wait());
  EXPECT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_TRUE(GetFakeServer()
                  ->GetSyncEntitiesByDataType(syncer::BOOKMARKS)
                  .front()
                  .specifics()
                  .bookmark()
                  .has_unique_position());
  EXPECT_TRUE(GetFakeServer()
                  ->GetSyncEntitiesByDataType(syncer::BOOKMARKS)
                  .front()
                  .specifics()
                  .bookmark()
                  .has_parent_guid());
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadBookmarks,
    ShouldReuploadBookmarkWithFaviconOnInitialMerge) {
  const GURL kIconUrl("http://www.google.com/favicon.ico");

  // Create a bookmark on the server which has a favicon and doesn't have GUID.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kBookmarkTitle);
  bookmark_builder.SetFavicon(CreateFavicon(SK_ColorRED), kIconUrl);
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark_entity =
      bookmark_builder
          .SetGeneration(BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL(kBookmarkPageUrl));
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
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_THAT(server_bookmarks, SizeIs(1));
  EXPECT_TRUE(server_bookmarks.front().specifics().bookmark().has_guid());

  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.DataTypeEntityChange.BOOKMARK",
                                         /*LOCAL_UPDATE*/ 2));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.BookmarkEntityReuploadNeeded.OnInitialMerge", true));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledReuploadBookmarks,
    ShouldReuploadUniquePositionOnIncrementalChange) {
  ASSERT_TRUE(SetupSync());

  // Make an incremental remote creation of bookmark.
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kBookmarkTitle);
  std::unique_ptr<syncer::LoopbackServerEntity> remote_folder =
      bookmark_builder.SetGeneration(BookmarkGeneration::kValidGuidAndFullTitle)
          .BuildBookmark(GURL(kBookmarkPageUrl));
  fake_server_->InjectEntity(std::move(remote_folder));

  // Download entities from the fake server.
  ASSERT_TRUE(BookmarksTitleChecker(kSingleProfileIndex, kBookmarkTitle,
                                    /*expected_count=*/1)
                  .Wait());

  // The bookmark should be unsynced at this point, but the data type is not
  // nudged for commit. Verify that the next sync cycle doesn't commit the
  // bookmark which should be reuploaded.
  const std::string kTitle2 = "Title 2";
  const GURL kBookmarkPageUrl2("http://url2.com");
  fake_server_->InjectEntity(
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle2)
          .SetGeneration(BookmarkGeneration::kValidGuidAndFullTitle)
          .BuildBookmark(kBookmarkPageUrl2));
  ASSERT_TRUE(
      BookmarksTitleChecker(kSingleProfileIndex, kTitle2, /*expected_count=*/1)
          .Wait());

  // Check that unique_position was not uploaded to the server yet.
  EXPECT_THAT(GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS),
              Contains(Not(HasUniquePosition())).Times(2));

  // Add another folder to initiate commit to the server.
  AddFolder(kSingleProfileIndex, "Folder 2");
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  // All elements must have unique_position now.
  EXPECT_THAT(GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS),
              Contains(HasUniquePosition()).Times(3));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksSyncTest,
                       CommitLocalCreationWithClientTag) {
  ASSERT_TRUE(SetupSync());

  const std::string kTitle = "Title";
  const BookmarkNode* folder = AddFolder(
      kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, kTitle);

  // Wait until the local bookmark gets committed.
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kTitle, /*url=*/GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());

  // Verify the client tag hash was committed to the server.
  std::vector<sync_pb::SyncEntity> server_bookmarks =
      GetFakeServer()->GetSyncEntitiesByDataType(syncer::BOOKMARKS);
  ASSERT_EQ(1u, server_bookmarks.size());
  EXPECT_EQ(server_bookmarks[0].client_tag_hash(),
            syncer::ClientTagHash::FromUnhashed(
                syncer::BOOKMARKS, folder->uuid().AsLowercaseString())
                .value());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksThrottlingSyncTest, DepleteQuota) {
  ASSERT_TRUE(SetupClients());

  // Setup custom quota params: to effectively never refill.
  sync_pb::ClientCommand client_command;
  client_command.set_extension_types_max_tokens(3);
  client_command.set_extension_types_refill_interval_seconds(10000);
  GetFakeServer()->SetClientCommand(client_command);

  // Add enough bookmarks to deplete quota in the initial cycle.
  const BookmarkNode* folder = GetOtherNode(kSingleProfileIndex);
  // The quota is fully depleted in 3 messages. As the default number of
  // entities per message on the client is 25, that requires 25*2+1 entities.
  for (int i = 0; i < (25 * 2 + 1); i++) {
    AddURL(kSingleProfileIndex, folder, 0, base::StringPrintf("url %u", i),
           GURL(base::StringPrintf("http://mail.google.com/%u", i)));
  }

  base::HistogramTester histogram_tester;
  SetupBookmarksSync();

  // All bookmarks get committed in the commit cycle but the quota gets
  // depleted.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.DataTypeCommitMessageHasDepletedQuota",
                   DataTypeHistogramValue(syncer::BOOKMARKS)));
  // Recovering from depleted quota is tested by another test.
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksThrottlingSyncTest,
                       DepletedQuotaDoesNotStopCommitCycle) {
  ASSERT_TRUE(SetupClients());

  // Setup custom quota params: to effectively never refill.
  sync_pb::ClientCommand client_command;
  client_command.set_extension_types_max_tokens(3);
  client_command.set_extension_types_refill_interval_seconds(10000);
  GetFakeServer()->SetClientCommand(client_command);

  // Add enough bookmarks to deplete quota in the initial cycle.
  const BookmarkNode* folder = GetOtherNode(kSingleProfileIndex);
  // The quota is fully depleted in 3 messages. As the default number of
  // entities per message on the client is 25, that requires 25*2+1 entities.
  // If the browser commits 100 more entities, this means 4 more commits hit
  // quota depletion.
  for (int i = 0; i < (25 * 2 + 101); i++) {
    AddURL(kSingleProfileIndex, folder, 0, base::StringPrintf("url %u", i),
           GURL(base::StringPrintf("http://mail.google.com/%u", i)));
  }

  base::HistogramTester histogram_tester;
  SetupBookmarksSync();

  // All bookmarks get committed in the commit cycle despite the quota gets
  // depleted long before all is committed.
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());
  EXPECT_EQ(4 + 1, histogram_tester.GetBucketCount(
                       "Sync.DataTypeCommitMessageHasDepletedQuota",
                       DataTypeHistogramValue(syncer::BOOKMARKS)));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksThrottlingSyncTest,
                       DoNotDepleteQuota) {
  ASSERT_TRUE(SetupClients());

  // Setup custom quota params: to effectively never refill.
  sync_pb::ClientCommand client_command;
  client_command.set_extension_types_max_tokens(4);
  client_command.set_extension_types_refill_interval_seconds(10000);
  GetFakeServer()->SetClientCommand(client_command);

  // Add not enough bookmarks to deplete quota in the initial cycle.
  const BookmarkNode* folder = GetOtherNode(kSingleProfileIndex);
  // The quota is still not fully depleted after 3 messages. As the default
  // number of entities per message on the client is 25, sending 2 messages
  // requires 25+1 entities. One extra message is sent later.
  for (int i = 0; i < (25 + 1); i++) {
    AddURL(kSingleProfileIndex, folder, 0, base::StringPrintf("url %u", i),
           GURL(base::StringPrintf("http://mail.google.com/%u", i)));
  }

  SetupBookmarksSync();
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  base::HistogramTester histogram_tester;

  // Adding another entity does again trigger an update (normal nudge delay).
  std::string client_title = "Foo";
  AddFolder(kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0,
            client_title);
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  // There is no record in the depleted quota histogram.
  histogram_tester.ExpectTotalCount(
      "Sync.DataTypeCommitMessageHasDepletedQuota", 0);
  histogram_tester.ExpectTotalCount("Sync.DataTypeCommitWithDepletedQuota", 0);
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksThrottlingSyncTest,
                       DepleteQuotaAndRecover) {
  ASSERT_TRUE(SetupClients());

  // Setup custom quota params: to effectively never refill, and custom nudge
  // delay of only 2 seconds.
  sync_pb::ClientCommand client_command;
  client_command.set_extension_types_max_tokens(3);
  client_command.set_extension_types_refill_interval_seconds(10000);
  client_command.set_extension_types_depleted_quota_nudge_delay_seconds(2);
  GetFakeServer()->SetClientCommand(client_command);

  // Add enough bookmarks to deplete quota in the initial cycle.
  const BookmarkNode* folder = GetOtherNode(kSingleProfileIndex);
  // The quota is fully depleted in 3 messages. As the default number of
  // entities per message on the client is 25, that requires 25*2+1 entities.
  for (int i = 0; i < (25 * 2 + 1); i++) {
    AddURL(kSingleProfileIndex, folder, 0, base::StringPrintf("url %u", i),
           GURL(base::StringPrintf("http://mail.google.com/%u", i)));
  }

  {
    base::HistogramTester histogram_tester;
    SetupBookmarksSync();

    ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                    kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                    GetFakeServer())
                    .Wait());
    // The quota should *just* be depleted now.
    EXPECT_EQ(1, histogram_tester.GetBucketCount(
                     "Sync.DataTypeCommitMessageHasDepletedQuota",
                     DataTypeHistogramValue(syncer::BOOKMARKS)));
  }

  // Need to send another bookmark in the next cycle. As the current cycle
  // determines the next nudge delay. Thus, only now the next commit is
  // scheduled in 3s from now.
  AddFolder(kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, "Foo");
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  {
    base::HistogramTester histogram_tester;

    // Adding another bookmark does not trigger an immediate commit: The
    // bookmarks data type is out of quota, so gets a long nudge delay.
    base::TimeTicks time = base::TimeTicks::Now();
    AddFolder(kSingleProfileIndex, GetOtherNode(kSingleProfileIndex), 0, "Bar");

    // Since the extra nudge delay is only two seconds, it still manages to
    // commit before test timeout.
    ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                    kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                    GetFakeServer())
                    .Wait());
    // Check that it takes at least one second, that should be robust enough to
    // not flake.
    EXPECT_GT(base::TimeTicks::Now() - time, base::Seconds(1));

    EXPECT_EQ(1, histogram_tester.GetBucketCount(
                     "Sync.DataTypeCommitMessageHasDepletedQuota",
                     DataTypeHistogramValue(syncer::BOOKMARKS)));
    EXPECT_GT(histogram_tester.GetBucketCount(
                  "Sync.DataTypeCommitWithDepletedQuota",
                  DataTypeHistogramValue(syncer::BOOKMARKS)),
              0);
  }
}

// On ChromeOS, Sync-the-feature gets started automatically once a primary
// account is signed in and the transport mode is not a thing.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class SingleClientBookmarksWithAccountStorageSyncTest
    : public SingleClientBookmarksSyncTest {
 public:
  SingleClientBookmarksWithAccountStorageSyncTest() {}

  SingleClientBookmarksWithAccountStorageSyncTest(
      const SingleClientBookmarksWithAccountStorageSyncTest&) = delete;
  SingleClientBookmarksWithAccountStorageSyncTest& operator=(
      const SingleClientBookmarksWithAccountStorageSyncTest&) = delete;

  ~SingleClientBookmarksWithAccountStorageSyncTest() override = default;

 private:
  base::test::ScopedFeatureList features_override_{
      syncer::kSyncEnableBookmarksInTransportMode};
};

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksWithAccountStorageSyncTest,
                       ShouldDownloadDataUponSigninAndClearUponSignout) {
  const std::string kLocalOnlyTitle = "Local Only";
  const std::string kAccountOnlyTitle = "Account Only";

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kAccountOnlyTitle);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  ASSERT_TRUE(SetupClients());

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  // Create a local bookmark folder while the user is signed out.
  model->AddFolder(/*parent=*/model->bookmark_bar_node(), /*index=*/0,
                   base::UTF8ToUTF16(kLocalOnlyTitle));

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kLocalOnlyTitle)));
  ASSERT_THAT(model->account_bookmark_bar_node(), IsNull());

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->SignInPrimaryAccount());
  // Note: Depending on the state of feature flags (specifically
  // kReplaceSyncPromosWithSignInPromos), Bookmarks may or may not be considered
  // selected by default.
  GetSyncService(kSingleProfileIndex)
      ->GetUserSettings()
      ->SetSelectedType(syncer::UserSelectableType::kBookmarks, true);
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetActiveDataTypes()
                  .Has(syncer::BOOKMARKS));

  EXPECT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kLocalOnlyTitle)));

  ASSERT_THAT(model->account_bookmark_bar_node(), NotNull());
  EXPECT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kAccountOnlyTitle)));

  // Sign out again.
  GetClient(kSingleProfileIndex)->SignOutPrimaryAccount();
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)
                   ->GetActiveDataTypes()
                   .Has(syncer::BOOKMARKS));

  EXPECT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kLocalOnlyTitle)));
  EXPECT_THAT(model->account_bookmark_bar_node(), IsNull());
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksWithAccountStorageSyncTest,
                       ShouldHandleMovesAcrossStorageBoundaries) {
  const std::string kInitiallyLocalTitle = "Initially Local";
  const std::string kInitiallyAccountTitle = "Initially Account";

  const std::string kInitiallyLocalNestedBookmarkTitle =
      "Initially Local Nested";
  const GURL kInitiallyLocalNestedBookmarkUrl("https://test.com");

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kInitiallyAccountTitle);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  ASSERT_TRUE(SetupClients());

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  {
    const BookmarkNode* folder =
        model->AddFolder(/*parent=*/model->bookmark_bar_node(), /*index=*/0,
                         base::UTF8ToUTF16(kInitiallyLocalTitle));
    model->AddURL(/*parent=*/folder, /*index=*/0,
                  base::UTF8ToUTF16(kInitiallyLocalNestedBookmarkTitle),
                  kInitiallyLocalNestedBookmarkUrl);
  }

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->SignInPrimaryAccount());
  // Note: Depending on the state of feature flags (specifically
  // kReplaceSyncPromosWithSignInPromos), Bookmarks may or may not be considered
  // selected by default.
  GetSyncService(kSingleProfileIndex)
      ->GetUserSettings()
      ->SetSelectedType(syncer::UserSelectableType::kBookmarks, true);
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetActiveDataTypes()
                  .Has(syncer::BOOKMARKS));

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyLocalTitle)));
  ASSERT_THAT(model->account_bookmark_bar_node(), NotNull());
  ASSERT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyAccountTitle)));

  // Move the account folder to local only.
  model->Move(model->account_bookmark_bar_node()->children().front().get(),
              model->bookmark_bar_node(),
              /*index=*/1);
  EXPECT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {}, /*cryptographer=*/nullptr)
                  .Wait());
  EXPECT_THAT(model->account_bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyLocalTitle),
                          IsFolderWithTitle(kInitiallyAccountTitle)));

  // Move one local folder with a child bookmark to the account.
  model->Move(model->bookmark_bar_node()->children().front().get(),
              model->account_bookmark_bar_node(),
              /*index=*/0);
  EXPECT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kInitiallyLocalTitle, GURL()},
                   {kInitiallyLocalNestedBookmarkTitle,
                    kInitiallyLocalNestedBookmarkUrl}},
                  /*cryptographer=*/nullptr)
                  .Wait());
  EXPECT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyLocalTitle)));
  EXPECT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyAccountTitle)));
}

// Regression test for crbug.com/329278277: turning sync-the-feature on, then
// off, and later signing in with account bookmarks enabled should lead to all
// bookmarks being duplicated (local bookmarks and account bookmarks). The user
// needs to take explicit action (e.g. exercise batch upload flow) to clean up
// these duplicates (but this part is not covered in the test).
IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksWithAccountStorageSyncTest,
    ShouldExposeDuplicatedBookmarksAfterTurningSyncOffAndSignIn) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";

  ASSERT_TRUE(SetupClients());

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  // Create two local folders while the user is signed out and sync is off.
  AddFolder(kSingleProfileIndex, /*parent=*/model->bookmark_bar_node(),
            /*index=*/0, kTitle1);
  AddFolder(kSingleProfileIndex, /*parent=*/model->bookmark_bar_node(),
            /*index=*/1, kTitle2);

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kTitle1, /*url=*/GURL()}, {kTitle2, /*url=*/GURL()}},
                  /*cryptographer=*/nullptr)
                  .Wait());
  ASSERT_THAT(model->account_bookmark_bar_node(), IsNull());

  // Turn Sync off by removing the primary account.
  GetClient(0)->SignOutPrimaryAccount();

  ASSERT_THAT(
      model->bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithTitle(kTitle1), IsFolderWithTitle(kTitle2)));

  // Sign in again, but don't actually enable Sync-the-feature (so that Sync
  // will start in transport mode).
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->SignInPrimaryAccount());
  // Note: Depending on the state of feature flags (specifically
  // kReplaceSyncPromosWithSignInPromos), Bookmarks may or may not be considered
  // selected by default.
  GetSyncService(kSingleProfileIndex)
      ->GetUserSettings()
      ->SetSelectedType(syncer::UserSelectableType::kBookmarks, true);
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetActiveDataTypes()
                  .Has(syncer::BOOKMARKS));
  ASSERT_THAT(model->account_bookmark_bar_node(), NotNull());

  // The folders should now be duplicated in local and account bookmarks.
  EXPECT_THAT(
      model->bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithTitle(kTitle1), IsFolderWithTitle(kTitle2)));
  EXPECT_THAT(
      model->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithTitle(kTitle1), IsFolderWithTitle(kTitle2)));

  // Move one folder individually from local to account, involving a UUID
  // collision.
  ASSERT_EQ(2u, model->bookmark_bar_node()->children().size());
  ASSERT_EQ(model->bookmark_bar_node()->children()[0]->uuid(),
            model->account_bookmark_bar_node()->children()[0]->uuid());
  model->Move(model->bookmark_bar_node()->children()[0].get(),
              /*new_parent=*/model->account_bookmark_bar_node(),
              /*index=*/model->account_bookmark_bar_node()->children().size());
  EXPECT_THAT(
      model->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithTitle(kTitle1), IsFolderWithTitle(kTitle2),
                  IsFolderWithTitle(kTitle1)));

  // Move one folder individually from account to local, involving a UUID
  // collision.
  ASSERT_EQ(1u, model->bookmark_bar_node()->children().size());
  ASSERT_EQ(model->bookmark_bar_node()->children()[0]->uuid(),
            model->account_bookmark_bar_node()->children()[1]->uuid());
  model->Move(model->account_bookmark_bar_node()->children()[1].get(),
              /*new_parent=*/model->bookmark_bar_node(),
              /*index=*/model->bookmark_bar_node()->children().size());
}

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientBookmarksWithAccountStorageSyncTest,
                       PRE_PersistAccountBookmarksAcrossRestarts) {
  const std::string kInitiallyLocalTitle = "Initially Local";
  const std::string kInitiallyAccountTitle = "Initially Account";

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(kInitiallyAccountTitle);
  fake_server_->InjectEntity(bookmark_builder.BuildFolder());

  ASSERT_TRUE(SetupClients());

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  model->AddFolder(/*parent=*/model->bookmark_bar_node(), /*index=*/0,
                   base::UTF8ToUTF16(kInitiallyLocalTitle));

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->SignInPrimaryAccount());
  // Note: Depending on the state of feature flags (specifically
  // kReplaceSyncPromosWithSignInPromos), Bookmarks may or may not be considered
  // selected by default.
  GetSyncService(kSingleProfileIndex)
      ->GetUserSettings()
      ->SetSelectedType(syncer::UserSelectableType::kBookmarks, true);
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kBookmarks));
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetActiveDataTypes()
                  .Has(syncer::BOOKMARKS));

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyLocalTitle)));
  ASSERT_THAT(model->account_bookmark_bar_node(), NotNull());
  ASSERT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyAccountTitle)));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksWithAccountStorageSyncTest,
                       PersistAccountBookmarksAcrossRestarts) {
  // Same values as in the PRE_ test.
  const std::string kInitiallyLocalTitle = "Initially Local";
  const std::string kInitiallyAccountTitle = "Initially Account";

  // Mimic the user being offline to verify that account bookmarks are loaded
  // from disk instead of being redownloaded.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kBookmarks));

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  // Local bookmarks should continue to exist (sanity-checking fixture).
  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyLocalTitle)));

  // Account bookmarks should continue existing even while offline.
  ASSERT_THAT(model->account_bookmark_bar_node(), NotNull());
  EXPECT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kInitiallyAccountTitle)));
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksWithAccountStorageSyncTest,
                       PRE_ShouldPersistIfInitialUpdatesCrossMaxCountLimit) {
  // Create two bookmarks on the server under BookmarkBar with a truncated
  // title.
  fake_server::EntityBuilderFactory entity_builder_factory;
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  fake_server_->InjectEntity(
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle1).BuildBookmark(
          GURL(kUrl1)));

  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  fake_server_->InjectEntity(
      entity_builder_factory.NewBookmarkEntityBuilder(kTitle2).BuildBookmark(
          GURL(kUrl2)));

  ASSERT_TRUE(SetupClients());
  // Set a limit of 4 bookmarks. This should result in an error when we get an
  // update of size 5.
  AccountBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);
  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->SignInPrimaryAccount());
  // Note: Depending on the state of feature flags (specifically
  // kReplaceSyncPromosWithSignInPromos), Bookmarks may or may not be considered
  // selected by default.
  GetSyncService(kSingleProfileIndex)
      ->GetUserSettings()
      ->SetSelectedType(syncer::UserSelectableType::kBookmarks, true);

  // Update of size 5 exceeds the limit.
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);
  EXPECT_THAT(model->account_bookmark_bar_node(), IsNull());
  EXPECT_THAT(model->account_other_node(), IsNull());
  EXPECT_THAT(model->account_mobile_node(), IsNull());

  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(SingleClientBookmarksWithAccountStorageSyncTest,
                       ShouldPersistIfInitialUpdatesCrossMaxCountLimit) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());

  // The fact that too many bookmarks were downloaded should have been persisted
  // and hence remembered now. Note that this test doesn't override
  // SetBookmarksLimitForTesting(), so the error must have been detected in
  // the PRE_ test.
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());

  // Account permanent nodes should remain absent.
  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);
  EXPECT_THAT(model->account_bookmark_bar_node(), IsNull());
  EXPECT_THAT(model->account_other_node(), IsNull());
  EXPECT_THAT(model->account_mobile_node(), IsNull());

  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTest,
    PRE_ShouldAllowRecoverIflLocalBookmarksDeletedBelowMaxCountLimit) {
  ASSERT_TRUE(SetupClients());

  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);

  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));

  // Add 2 new bookmarks to exceed the limit.
  const BookmarkNode* bookmark_bar_node =
      GetBookmarkBarNode(kSingleProfileIndex);

  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/bookmark_bar_node, /*index=*/0, "title0",
                     GURL("http://www.url0.com")));
  ASSERT_TRUE(AddURL(kSingleProfileIndex,
                     /*parent=*/bookmark_bar_node, /*index=*/1, "title1",
                     GURL("http://www.url1.com")));
  EXPECT_TRUE(
      BookmarksDataTypeErrorChecker(GetClient(kSingleProfileIndex)->service())
          .Wait());

  // Delete one bookmark to bring the count below the limit.
  Remove(kSingleProfileIndex, bookmark_bar_node, /*index=*/0);

  // Bookmarks should be in an error state. Thus excluding it from the
  // CheckForDataTypeFailures() check.
  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::BOOKMARKS});
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTest,
    ShouldAllowRecoverIflLocalBookmarksDeletedBelowMaxCountLimit) {
  ASSERT_TRUE(SetupClients());

  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
      GetProfile(kSingleProfileIndex))
      ->SetBookmarksLimitForTesting(4);

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetActiveDataTypes()
                  .Has(syncer::BOOKMARKS));
  EXPECT_FALSE(GetClient(kSingleProfileIndex)
                   ->service()
                   ->HasAnyModelErrorForTest({syncer::BOOKMARKS}));
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Android doesn't currently support PRE_ tests, see crbug.com/40200835 or
// crbug.com/40145099.
#if !BUILDFLAG(IS_ANDROID)
class SingleClientBookmarksSyncTestWithEnabledMigrateSyncingUserToSignedIn
    : public SingleClientBookmarksWithAccountStorageSyncTest {
 protected:
  SingleClientBookmarksSyncTestWithEnabledMigrateSyncingUserToSignedIn() {
    if (content::IsPreTest()) {
      features_override_.InitAndDisableFeature(
          switches::kMigrateSyncingUserToSignedIn);
    } else {
      features_override_.InitWithFeatures(
          /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                                switches::kMigrateSyncingUserToSignedIn},
          /*disabled_features=*/{});
    }
  }

  const std::string kTestTitle = "Test Title";

 private:
  base::test::ScopedFeatureList features_override_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledMigrateSyncingUserToSignedIn,
    PRE_SyncToSigninMigration) {
  ASSERT_TRUE(SetupClients());
  AddFolder(kSingleProfileIndex, kTestTitle);

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(BookmarkModelMatchesFakeServerChecker(
                  kSingleProfileIndex, GetSyncService(kSingleProfileIndex),
                  GetFakeServer())
                  .Wait());

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kTestTitle)));

  // Sync-the-feature is on, so account bookmarks should not exist.
  ASSERT_THAT(model->account_bookmark_bar_node(), IsNull());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientBookmarksSyncTestWithEnabledMigrateSyncingUserToSignedIn,
    SyncToSigninMigration) {
  // Mimic the user being offline to verify that account bookmarks are loaded
  // from disk instead of being redownloaded.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(kSingleProfileIndex)->AwaitEngineInitialization());
  ASSERT_FALSE(GetSyncService(kSingleProfileIndex)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(kSingleProfileIndex)
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kBookmarks));

  BookmarkModel* model = GetBookmarkModel(kSingleProfileIndex);

  // Local bookmarks should be empty.
  ASSERT_THAT(model->bookmark_bar_node()->children(), IsEmpty());

  // Account bookmarks should continue existing even while offline.
  ASSERT_THAT(model->account_bookmark_bar_node(), NotNull());
  EXPECT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithTitle(kTestTitle)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
