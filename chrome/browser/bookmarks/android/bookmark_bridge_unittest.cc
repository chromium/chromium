// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/android/bookmark_bridge.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_utils.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/partnerbookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/partnerbookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/test/mock_commit_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using bookmarks::android::JavaBookmarkIdCreateBookmarkId;
using testing::ElementsAre;
using testing::IsNull;
using testing::NotNull;

// Unit tests for `BookmarkBridge`.
class BookmarkBridgeTest : public testing::Test {
 public:
  BookmarkBridgeTest() = default;
  ~BookmarkBridgeTest() override = default;

  // testing::Test
  void SetUp() override {
    // Setup the profile, and service factories.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        "BookmarkBridgeTest", /*testing_factories=*/{
            TestingProfile::TestingFactory{
                BookmarkModelFactory::GetInstance(),
                BookmarkModelFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                ManagedBookmarkServiceFactory::GetInstance(),
                ManagedBookmarkServiceFactory::GetDefaultFactory()}});

    // Setup bookmark sources from their factories.
    managed_bookmark_service_ =
        ManagedBookmarkServiceFactory::GetForProfile(profile_);
    partner_bookmarks_shim_ =
        PartnerBookmarksShim::BuildForBrowserContext(profile_);

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    CreateBookmarkBridge(/*enable_account_bookmarks=*/false);
  }

  void TearDown() override {
    bookmark_bridge_.reset();
    bookmark_model_.reset();
    profile_manager_.reset();
    PartnerBookmarksShim::ClearPartnerModelForTesting();
  }

  BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

  BookmarkBridge* bookmark_bridge() { return bookmark_bridge_.get(); }

  reading_list::ReadingListManager* local_or_syncable_reading_list_manager() {
    return bookmark_bridge_->GetLocalOrSyncableReadingListManagerForTesting();
  }

  reading_list::ReadingListManager* account_reading_list_manager() {
    return bookmark_bridge_
        ->GetAccountReadingListManagerIfAvailableForTesting();
  }

  signin::IdentityTestEnvironment* identity_test_environment() {
    return identity_test_environment_.get();
  }

  const BookmarkNode* AddURL(const BookmarkNode* parent,
                             size_t index,
                             const std::u16string& title,
                             const GURL& url) {
    return bookmark_model()->AddURL(parent, index, title, url,
                                    /*meta_info=*/nullptr, clock_.Now());
  }

  void StartSyncing() {
    std::unique_ptr<syncer::DataTypeActivationResponse> activation_response;
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    dual_reading_list_model_->GetSyncControllerDelegateForTransportMode()
        ->OnSyncStarting(
            request, base::BindLambdaForTesting(
                         [&](std::unique_ptr<syncer::DataTypeActivationResponse>
                                 response) {
                           activation_response = std::move(response);
                           loop.Quit();
                         }));
    loop.Run();

    activation_response->type_processor->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());

    // After this update initial sync is for sure done.
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    activation_response->type_processor->OnUpdateReceived(
        state, {}, /*gc_directive=*/std::nullopt);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return dual_reading_list_model_->GetAccountModelIfSyncing() != nullptr;
    }));
  }

  void CreateBookmarkBridge(bool enable_account_bookmarks,
                            bool load_reading_list_model = true) {
    bookmark_bridge_.reset();
    bookmark_model_.reset();

    base::WeakPtr<FakeReadingListModelStorage>
        local_or_syncable_model_storage_ptr;
    auto local_or_syncable_reading_list_model = CreateReadingListModel(
        syncer::StorageType::kUnspecified, local_or_syncable_model_storage_ptr);

    base::WeakPtr<FakeReadingListModelStorage>
        account_reading_list_model_storage_ptr;
    auto account_reading_list_model = CreateReadingListModel(
        syncer::StorageType::kAccount, account_reading_list_model_storage_ptr);

    if (load_reading_list_model) {
      EXPECT_TRUE(
          local_or_syncable_model_storage_ptr->TriggerLoadCompletion() &&
          account_reading_list_model_storage_ptr->TriggerLoadCompletion());
    }

    dual_reading_list_model_ =
        std::make_unique<reading_list::DualReadingListModel>(
            std::move(local_or_syncable_reading_list_model),
            std::move(account_reading_list_model));

    std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client =
        std::make_unique<TestBookmarkClientWithManagedService>(
            managed_bookmark_service_);
    bookmark_model_ =
        std::make_unique<bookmarks::BookmarkModel>(std::move(bookmark_client));
    bookmark_model_->LoadEmptyForTest();

    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());

    if (enable_account_bookmarks) {
      bookmark_model_->CreateAccountPermanentFolders();
      if (load_reading_list_model) {
        // If the `account_reading_list_model` is not loaded, StartSyncing()
        // will run into failure.
        StartSyncing();
      }
    }

    bookmark_bridge_ = std::make_unique<BookmarkBridge>(
        profile_, bookmark_model_.get(), managed_bookmark_service_,
        dual_reading_list_model_.get(), partner_bookmarks_shim_,
        identity_test_environment_->identity_manager());

    bookmark_bridge_->LoadEmptyPartnerBookmarkShimForTesting(
        AttachCurrentThread());
    partner_bookmarks_shim_->SetPartnerBookmarksRoot(
        PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting());
  }

 protected:
  std::unique_ptr<ReadingListModelImpl> CreateReadingListModel(
      syncer::StorageType storage_type,
      base::WeakPtr<FakeReadingListModelStorage>& storage_ptr) {
    auto storage = std::make_unique<FakeReadingListModelStorage>();
    storage_ptr = storage->AsWeakPtr();
    auto reading_list_model = std::make_unique<ReadingListModelImpl>(
        std::move(storage), storage_type,
        storage_type == syncer::StorageType::kAccount
            ? syncer::WipeModelUponSyncDisabledBehavior::kAlways
            : syncer::WipeModelUponSyncDisabledBehavior::kNever,
        &clock_);
    return reading_list_model;
  }
  base::SimpleTestClock clock_;
  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<Profile> profile_;
  raw_ptr<ManagedBookmarkService> managed_bookmark_service_;
  raw_ptr<PartnerBookmarksShim> partner_bookmarks_shim_;

  std::unique_ptr<BookmarkModel> bookmark_model_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<reading_list::DualReadingListModel> dual_reading_list_model_;
  std::unique_ptr<BookmarkBridge> bookmark_bridge_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
};

TEST_F(BookmarkBridgeTest,
       TestAccountModelAvailabilityChangeUponChangesInSyncState) {
  CreateBookmarkBridge(/*enable_account_bookmarks*/ true);
  ASSERT_THAT(account_reading_list_manager(), NotNull());

  dual_reading_list_model_->GetSyncControllerDelegateForTransportMode()
      ->OnSyncStopping(syncer::CLEAR_METADATA);
  EXPECT_THAT(account_reading_list_manager(), IsNull());

  StartSyncing();
  EXPECT_THAT(account_reading_list_manager(), NotNull());
}

TEST_F(BookmarkBridgeTest, TestGetMostRecentlyAddedUserBookmarkIdForUrl) {
  GURL url = GURL("http://foo.com");

  // The first call will have no result.
  ASSERT_EQ(
      nullptr,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));

  // Verify that the last bookmark that was added is the result.
  AddURL(bookmark_model()->other_node(), 0, u"first", url);
  clock_.Advance(base::Seconds(1));
  AddURL(bookmark_model()->other_node(), 0, u"second", url);
  clock_.Advance(base::Seconds(1));
  auto* recently_added =
      AddURL(bookmark_model()->other_node(), 0, u"third", url);
  clock_.Advance(base::Seconds(1));

  ASSERT_EQ(
      recently_added,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));

  // Add to the reading list and verify that it's the most recently added.
  recently_added = local_or_syncable_reading_list_manager()->Add(url, "fourth");
  ASSERT_EQ(
      recently_added,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));
}

TEST_F(BookmarkBridgeTest,
       TestGetMostRecentlyAddedUserBookmarkIdForUrlBeforeReadingListLoads) {
  CreateBookmarkBridge(/*enable_account_bookmarks*/ false,
                       /*load_reading_list_model*/ false);
  ASSERT_FALSE(local_or_syncable_reading_list_manager()->IsLoaded());
  EXPECT_THAT(bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(
                  GURL("http://foo.com")),
              IsNull());
}

TEST_F(
    BookmarkBridgeTest,
    TestGetMostRecentlyAddedUserBookmarkIdForUrlBeforeReadingListLoadsWithAccountBookmarks) {
  CreateBookmarkBridge(/*enable_account_bookmarks*/ true,
                       /*load_reading_list_model*/ false);
  ASSERT_FALSE(local_or_syncable_reading_list_manager()->IsLoaded());
  // When the account_reading_list_model is not loaded, sync will not start,
  // which means `account_reading_list_manager` will be null.
  ASSERT_THAT(account_reading_list_manager(), IsNull());
  EXPECT_THAT(bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(
                  GURL("http://foo.com")),
              IsNull());
}

TEST_F(BookmarkBridgeTest, TestIsBookmarked) {
  JNIEnv* const env = AttachCurrentThread();
  GURL url = GURL("http://foo.com");
  ASSERT_FALSE(bookmark_bridge()->IsBookmarked(env, url));

  AddURL(bookmark_model()->other_node(), 0, u"foo", url);
  ASSERT_TRUE(bookmark_bridge()->IsBookmarked(env, url));

  bookmark_model()->RemoveAllUserBookmarks(FROM_HERE);
  ASSERT_FALSE(bookmark_bridge()->IsBookmarked(env, url));

  local_or_syncable_reading_list_manager()->Add(url, "bar");
  ASSERT_TRUE(bookmark_bridge()->IsBookmarked(env, url));
}

TEST_F(BookmarkBridgeTest, TestGetTopLevelFolderIds) {
  std::vector<const BookmarkNode*> folders =
      bookmark_bridge()->GetTopLevelFolderIdsImpl(
          /*force_visible_mask=*/BookmarkNodeMaskBit::NONE);

  if (bookmarks::TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    // For desktop Android, the 4 folders should be: mobile bookmarks, bookmarks
    // bar, other bookmarks, and reading list. Note that "Mobile bookmarks" is
    // already populated when the bridge was created.
    EXPECT_EQ(4u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
    EXPECT_EQ(u"Other bookmarks", folders[2]->GetTitle());
    EXPECT_EQ(u"Reading list", folders[3]->GetTitle());
  } else {
    // For mobile Android, the 2 folders should be: mobile bookmarks, reading
    // list.
    EXPECT_EQ(2u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_EQ(u"Reading list", folders[1]->GetTitle());
  }

  // When forcing visibility, all top-level folders should be visible.
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*force_visible_mask=*/BookmarkNodeMaskBit::ALL);

  // The 4 folders should be: mobile bookmarks, bookmarks bar, other bookmarks,
  // and reading list.
  EXPECT_EQ(4u, folders.size());
  EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
  EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
  EXPECT_EQ(u"Other bookmarks", folders[2]->GetTitle());
  EXPECT_EQ(u"Reading list", folders[3]->GetTitle());

  // Adding a bookmark to the bookmark bar will include it in the top level
  // folders that are returned.
  AddURL(bookmark_model()->bookmark_bar_node(), 0, u"first",
         GURL("http://foo.com"));
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*force_visible_mask=*/BookmarkNodeMaskBit::NONE);

  if (bookmarks::TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_EQ(4u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
    EXPECT_EQ(u"Other bookmarks", folders[2]->GetTitle());
    EXPECT_EQ(u"Reading list", folders[3]->GetTitle());
  } else {
    EXPECT_EQ(3u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
    EXPECT_EQ(u"Reading list", folders[2]->GetTitle());
  }
}

TEST_F(BookmarkBridgeTest, AccountFoldersNullWhileNotEnabled) {
  JNIEnv* const env = AttachCurrentThread();
  EXPECT_TRUE(bookmark_bridge()->GetAccountMobileFolderId(env).is_null());
  EXPECT_TRUE(bookmark_bridge()->GetAccountOtherFolderId(env).is_null());
  EXPECT_TRUE(bookmark_bridge()->GetAccountDesktopFolderId(env).is_null());
  EXPECT_TRUE(bookmark_bridge()->GetAccountReadingListFolder(env).is_null());
}

TEST_F(BookmarkBridgeTest, TestGetTopLevelFolderIdsAccountActive) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);

  std::vector<const BookmarkNode*> folders =
      bookmark_bridge()->GetTopLevelFolderIdsImpl(
          /*force_visible_mask=*/BookmarkNodeMaskBit::NONE);

  if (bookmarks::TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_EQ(4u, folders.size());
    EXPECT_EQ(u"Bookmarks bar", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Other bookmarks", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Reading list", folders[2]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
    EXPECT_EQ(u"Mobile bookmarks", folders[3]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));
  } else {
    EXPECT_EQ(3u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Reading list", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Mobile bookmarks", folders[2]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
  }

  // When there are no partner bookmarks, the local mobile node will be hidden.
  partner_bookmarks_shim_->SetPartnerBookmarksRoot(nullptr);
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*force_visible_mask=*/BookmarkNodeMaskBit::NONE);

  if (bookmarks::TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_EQ(3u, folders.size());
    EXPECT_EQ(u"Bookmarks bar", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Other bookmarks", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Reading list", folders[2]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
  } else {
    EXPECT_EQ(2u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Reading list", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
  }

  // All account and some local folders should be included when forcing
  // visibility.
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*force_visible_mask=*/BookmarkNodeMaskBit::ALL);
  if (bookmarks::TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_EQ(5u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Other bookmarks", folders[2]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
    EXPECT_EQ(u"Reading list", folders[3]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));
    EXPECT_EQ(u"Reading list", folders[4]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[4]));
  } else {
    EXPECT_EQ(6u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Other bookmarks", folders[2]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
    EXPECT_EQ(u"Reading list", folders[3]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));
    EXPECT_EQ(u"Mobile bookmarks", folders[4]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[4]));
    EXPECT_EQ(u"Reading list", folders[5]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[5]));
  }

  // Adding a bookmark to the bookmark bar will include it in the top level
  // folders that are returned.
  AddURL(bookmark_model()->bookmark_bar_node(), 0, u"first",
         GURL("http://foo.com"));
  // Adding a bookmark node to mobile bookmarks will include it in the list.
  AddURL(bookmark_model()->mobile_node(), 0, u"second", GURL("http://foo.com"));
  // Adding to the local reading list will include it in the list.
  local_or_syncable_reading_list_manager()->Add(GURL("http://foo.com"),
                                                "third");
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*force_visible_mask=*/BookmarkNodeMaskBit::NONE);

  if (bookmarks::TestBookmarkClient::IsDesktopFormFactorByDefault()) {
    EXPECT_EQ(6u, folders.size());
    EXPECT_EQ(u"Bookmarks bar", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Other bookmarks", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Reading list", folders[2]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
    EXPECT_EQ(u"Mobile bookmarks", folders[3]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));
    EXPECT_EQ(u"Bookmarks bar", folders[4]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[4]));
    EXPECT_EQ(u"Reading list", folders[5]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[5]));
  } else {
    EXPECT_EQ(5u, folders.size());
    EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
    EXPECT_EQ(u"Reading list", folders[1]->GetTitle());
    EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
    EXPECT_EQ(u"Mobile bookmarks", folders[2]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
    EXPECT_EQ(u"Bookmarks bar", folders[3]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));
    EXPECT_EQ(u"Reading list", folders[4]->GetTitle());
    EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[4]));
  }
}

TEST_F(BookmarkBridgeTest, AccountFoldersNonNullWhileEnabled) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);
  JNIEnv* const env = AttachCurrentThread();
  EXPECT_FALSE(bookmark_bridge()->GetAccountMobileFolderId(env).is_null());
  EXPECT_FALSE(bookmark_bridge()->GetAccountOtherFolderId(env).is_null());
  EXPECT_FALSE(bookmark_bridge()->GetAccountDesktopFolderId(env).is_null());
  EXPECT_FALSE(bookmark_bridge()->GetAccountReadingListFolder(env).is_null());
}

TEST_F(BookmarkBridgeTest, GetChildIdsMobileShowsPartner) {
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(bookmark_model()->mobile_node());

  ASSERT_EQ(1u, children.size());
  ASSERT_EQ(partner_bookmarks_shim_->GetPartnerBookmarksRoot(), children[0]);
  ASSERT_EQ(bookmarks::BookmarkType::BOOKMARK_TYPE_PARTNER,
            bookmark_bridge()->GetBookmarkType(children[0]));

  partner_bookmarks_shim_->SetPartnerBookmarksRoot(nullptr);
  children =
      bookmark_bridge()->GetChildIdsImpl(bookmark_model()->mobile_node());
  ASSERT_EQ(0u, children.size());
}

TEST_F(BookmarkBridgeTest, GetUnreadCountLocalOrSyncable) {
  GURL url = GURL("http://foo.com");
  local_or_syncable_reading_list_manager()->Add(url, "foo");
  local_or_syncable_reading_list_manager()->Add(GURL("http://bar.com"), "bar");

  JNIEnv* const env = AttachCurrentThread();
  ASSERT_EQ(
      2, bookmark_bridge()->GetUnreadCount(
             env, bookmark_bridge()->GetLocalOrSyncableReadingListFolder(env)));

  local_or_syncable_reading_list_manager()->SetReadStatus(url, true);
  ASSERT_EQ(
      1, bookmark_bridge()->GetUnreadCount(
             env, bookmark_bridge()->GetLocalOrSyncableReadingListFolder(env)));
}

TEST_F(BookmarkBridgeTest, SetReadStatus) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);

  GURL url1 = GURL("http://foo.com");
  GURL url2 = GURL("http://bar.com");
  const bookmarks::BookmarkNode* local1 =
      local_or_syncable_reading_list_manager()->Add(url1, "foo");
  const bookmarks::BookmarkNode* local2 =
      local_or_syncable_reading_list_manager()->Add(url2, "bar");
  const bookmarks::BookmarkNode* acc1 =
      account_reading_list_manager()->Add(url1, "foo");

  bookmark_bridge()->SetReadStatusImpl(url1, true);
  bookmark_bridge()->SetReadStatusImpl(url2, true);
  ASSERT_TRUE(local_or_syncable_reading_list_manager()->GetReadStatus(local1));
  ASSERT_TRUE(local_or_syncable_reading_list_manager()->GetReadStatus(local2));
  ASSERT_TRUE(account_reading_list_manager()->GetReadStatus(acc1));

  const bookmarks::BookmarkNode* acc2 =
      account_reading_list_manager()->Add(url2, "bar");
  ASSERT_FALSE(account_reading_list_manager()->GetReadStatus(acc2));

  bookmark_bridge()->SetReadStatusImpl(url1, false);
  bookmark_bridge()->SetReadStatusImpl(url2, false);
  ASSERT_FALSE(local_or_syncable_reading_list_manager()->GetReadStatus(local1));
  ASSERT_FALSE(local_or_syncable_reading_list_manager()->GetReadStatus(local2));
  ASSERT_FALSE(account_reading_list_manager()->GetReadStatus(acc1));
  ASSERT_FALSE(account_reading_list_manager()->GetReadStatus(acc2));
}

// Test that the correct type, parent node, etc are returned for account
// reading list nodes.
TEST_F(BookmarkBridgeTest, TestAccountReadingListNodes) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);

  GURL url = GURL("http://foo.com");

  local_or_syncable_reading_list_manager()->Add(url, "foo");
  const BookmarkNode* local_rl_node =
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url);
  EXPECT_EQ(bookmarks::BOOKMARK_TYPE_READING_LIST,
            bookmark_bridge()->GetBookmarkType(local_rl_node));
  EXPECT_EQ(local_or_syncable_reading_list_manager()->GetRoot(),
            local_rl_node->parent());
  EXPECT_EQ(local_rl_node->parent(),
            bookmark_bridge()->GetParentNode(local_rl_node));
  clock_.Advance(base::Seconds(1));

  ASSERT_THAT(account_reading_list_manager(), NotNull());

  account_reading_list_manager()->Add(url, "foo");
  const BookmarkNode* account_rl_node =
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url);
  EXPECT_EQ(bookmarks::BOOKMARK_TYPE_READING_LIST,
            bookmark_bridge()->GetBookmarkType(account_rl_node));
  EXPECT_EQ(account_reading_list_manager()->GetRoot(),
            account_rl_node->parent());
  EXPECT_EQ(account_rl_node->parent(),
            bookmark_bridge()->GetParentNode(account_rl_node));
}

TEST_F(BookmarkBridgeTest, TestSearchBookmarks) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);

  GURL url = GURL("http://foo.com");

  ASSERT_THAT(account_reading_list_manager(), NotNull());

  account_reading_list_manager()->Add(url, "foo");
  local_or_syncable_reading_list_manager()->Add(url, "foo");
  local_or_syncable_reading_list_manager()->Add(url, "baz");

  power_bookmarks::PowerBookmarkQueryFields query1;
  query1.word_phrase_query = std::make_unique<std::u16string>(u"foo");
  std::vector<const BookmarkNode*> results1 =
      bookmark_bridge()->SearchBookmarksImpl(query1, 999);
  EXPECT_EQ(2u, results1.size());

  power_bookmarks::PowerBookmarkQueryFields query2;
  query2.word_phrase_query = std::make_unique<std::u16string>(u"baz");
  std::vector<const BookmarkNode*> results2 =
      bookmark_bridge()->SearchBookmarksImpl(query2, 999);
  EXPECT_EQ(1u, results2.size());
}

TEST_F(BookmarkBridgeTest, TestMoveBookmark) {
  GURL url = GURL("http://foo.com");

  const BookmarkNode* node =
      AddURL(bookmark_model()->other_node(), 0, u"test", url);
  bookmark_bridge()->MoveBookmarkImpl(node, bookmarks::BOOKMARK_TYPE_NORMAL,
                                      bookmark_model()->bookmark_bar_node(),
                                      bookmarks::BOOKMARK_TYPE_NORMAL, 0);
  // Get children of new parent and verify it has the node in it.
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(bookmark_model()->bookmark_bar_node());
  ASSERT_EQ(1u, children.size());
  ASSERT_EQ(children[0], node);

  // Get children of old parent and verify it has no nodes.
  children = bookmark_bridge()->GetChildIdsImpl(bookmark_model()->other_node());
  ASSERT_EQ(0u, children.size());
}

TEST_F(BookmarkBridgeTest, TestMoveBookmarkToOwnParentReturnsEarly) {
  GURL url = GURL("http://foo.com");

  const BookmarkNode* first = AddURL(bookmark_model()->other_node(),
                                     /*index=*/0, /*title=*/u"first", url);
  const BookmarkNode* second = AddURL(bookmark_model()->other_node(),
                                      /*index=*/1, /*title=*/u"second", url);
  bookmark_bridge()->MoveBookmarkImpl(
      first, bookmarks::BOOKMARK_TYPE_NORMAL, bookmark_model()->other_node(),
      bookmarks::BOOKMARK_TYPE_NORMAL, /*index=*/2);
  EXPECT_THAT(
      bookmark_bridge()->GetChildIdsImpl(bookmark_model()->other_node()),
      ElementsAre(first, second));
}

TEST_F(BookmarkBridgeTest, TestMoveBookmarkToReadingList) {
  GURL url = GURL("http://foo.com");
  const std::u16string title = u"test";

  const BookmarkNode* node =
      AddURL(bookmark_model()->other_node(), 0, title, url);
  bookmark_bridge()->MoveBookmarkImpl(
      node, bookmarks::BOOKMARK_TYPE_NORMAL,
      local_or_syncable_reading_list_manager()->GetRoot(),
      bookmarks::BOOKMARK_TYPE_READING_LIST, 0);
  // Get children of new parent and verify it has the node in it.
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(
          local_or_syncable_reading_list_manager()->GetRoot());
  ASSERT_EQ(1u, children.size());
  ASSERT_EQ(children[0]->GetTitle(), title);
  ASSERT_EQ(children[0]->url(), url);

  // Get children of old parent and verify it has no nodes.
  children = bookmark_bridge()->GetChildIdsImpl(bookmark_model()->other_node());
  ASSERT_EQ(0u, children.size());
}

TEST_F(BookmarkBridgeTest, TestMoveBookmarkToReadingListAddFails) {
  GURL url = GURL("chrome://newtab");
  const std::u16string title = u"native page";

  const BookmarkNode* node =
      AddURL(bookmark_model()->other_node(), 0, title, url);
  bookmark_bridge()->MoveBookmarkImpl(
      node, bookmarks::BOOKMARK_TYPE_NORMAL,
      local_or_syncable_reading_list_manager()->GetRoot(),
      bookmarks::BOOKMARK_TYPE_READING_LIST, 0);
  // Get children of new parent and verify it hasn't been moved
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(
          local_or_syncable_reading_list_manager()->GetRoot());
  ASSERT_EQ(0u, children.size());

  // Get children of old parent and verify the original bookmark is still there.
  children = bookmark_bridge()->GetChildIdsImpl(bookmark_model()->other_node());
  ASSERT_EQ(1u, children.size());
  ASSERT_EQ(children[0]->GetTitle(), title);
  ASSERT_EQ(children[0]->url(), url);
}

TEST_F(BookmarkBridgeTest, TestMoveReadingListToBookmark) {
  GURL url = GURL("http://foo.com");
  const std::u16string title = u"test";

  const BookmarkNode* node =
      local_or_syncable_reading_list_manager()->Add(url, "test");
  bookmark_bridge()->MoveBookmarkImpl(node,
                                      bookmarks::BOOKMARK_TYPE_READING_LIST,
                                      bookmark_model()->bookmark_bar_node(),
                                      bookmarks::BOOKMARK_TYPE_NORMAL, 0);
  // Get children of new parent and verify it has the node in it.
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(bookmark_model()->bookmark_bar_node());
  ASSERT_EQ(1u, children.size());
  ASSERT_EQ(children[0]->GetTitle(), title);
  ASSERT_EQ(children[0]->url(), url);

  // Get children of old parent and verify it has no nodes.
  children = bookmark_bridge()->GetChildIdsImpl(
      local_or_syncable_reading_list_manager()->GetRoot());
  ASSERT_EQ(0u, children.size());
}

class BookmarkBridgeClipboardTest : public BookmarkBridgeTest {
 public:
  void SetUp() override {
    ui::TestClipboard::CreateForCurrentThread();
    BookmarkBridgeTest::SetUp();
  }

  void TearDown() override {
    BookmarkBridgeTest::TearDown();
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  std::unique_ptr<bookmarks::BookmarkNodeData> ReadBookmarkDataSync() {
    base::test::TestFuture<std::unique_ptr<bookmarks::BookmarkNodeData>> future;
    bookmarks::BookmarkNodeData::ReadFromClipboard(
        ui::ClipboardBuffer::kCopyPaste, future.GetCallback());
    return future.Take();
  }
};

TEST_F(BookmarkBridgeClipboardTest, CopyBookmarkWritesDataAndPreservesMeta) {
  const BookmarkNode* bar = bookmark_model()->bookmark_bar_node();
  const GURL url("https://example.com/");
  const BookmarkNode* node = AddURL(bar, /*index=*/0, u"Example", url);
  bookmark_model()->SetNodeMetaInfo(node, "key", "value");

  bookmark_bridge()->CopyToClipboardImpl(node, /*is_off_the_record=*/false);

  EXPECT_EQ(1u, bar->children().size());
  auto data = ReadBookmarkDataSync();
  ASSERT_TRUE(data);
  ASSERT_EQ(1u, data->size());
  EXPECT_TRUE(data->elements[0].is_url);
  EXPECT_EQ(url, data->elements[0].url);
  EXPECT_EQ(u"Example", data->elements[0].title);
  EXPECT_EQ("value", data->elements[0].meta_info_map["key"]);
}

TEST_F(BookmarkBridgeClipboardTest, CutFolderWritesSubtreeAndRemovesNode) {
  const BookmarkNode* bar = bookmark_model()->bookmark_bar_node();
  const BookmarkNode* folder =
      bookmark_model()->AddFolder(bar, /*index=*/0, u"Source");
  AddURL(folder, /*index=*/0, u"Child 1", GURL("https://one.example/"));
  const BookmarkNode* nested =
      bookmark_model()->AddFolder(folder, /*index=*/1, u"Nested");
  AddURL(nested, /*index=*/0, u"Child 2", GURL("https://two.example/"));

  bookmark_bridge()->CutToClipboardImpl(folder, /*is_off_the_record=*/false);

  EXPECT_TRUE(bar->children().empty());
  auto data = ReadBookmarkDataSync();
  ASSERT_TRUE(data);
  ASSERT_EQ(1u, data->size());
  EXPECT_FALSE(data->elements[0].is_url);
  EXPECT_EQ(u"Source", data->elements[0].title);
  ASSERT_EQ(2u, data->elements[0].children.size());
  EXPECT_EQ(u"Child 1", data->elements[0].children[0].title);
  EXPECT_EQ(u"Nested", data->elements[0].children[1].title);
  ASSERT_EQ(1u, data->elements[0].children[1].children.size());
  EXPECT_EQ(u"Child 2", data->elements[0].children[1].children[0].title);
}

TEST_F(BookmarkBridgeClipboardTest, PermanentFoldersCannotBeCopiedOrCut) {
  const BookmarkNode* bar = bookmark_model()->bookmark_bar_node();
  const BookmarkNode* managed_node = managed_bookmark_service_->managed_node();
  ASSERT_THAT(managed_node, NotNull());

  bookmark_bridge()->CopyToClipboardImpl(bar, /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CopyToClipboardImpl(managed_node,
                                         /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboardImpl(bar, /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboardImpl(managed_node,
                                        /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
}

TEST_F(BookmarkBridgeClipboardTest, ManagedBookmarkCanBeCopiedButNotCut) {
  const BookmarkNode* managed_node = managed_bookmark_service_->managed_node();
  ASSERT_THAT(managed_node, NotNull());
  const BookmarkNode* managed_child = AddURL(
      managed_node, /*index=*/0, u"Managed", GURL("https://managed.example/"));

  bookmark_bridge()->CutToClipboardImpl(managed_child,
                                        /*is_off_the_record=*/false);
  EXPECT_EQ(1u, managed_node->children().size());
  EXPECT_FALSE(ReadBookmarkDataSync());

  bookmark_bridge()->CopyToClipboardImpl(managed_child,
                                         /*is_off_the_record=*/false);
  auto managed_data = ReadBookmarkDataSync();
  ASSERT_TRUE(managed_data);
  EXPECT_EQ(GURL("https://managed.example/"), managed_data->elements[0].url);
}

TEST_F(BookmarkBridgeClipboardTest, EditPolicyBlocksCutButAllowsCopy) {
  const BookmarkNode* bar = bookmark_model()->bookmark_bar_node();
  const BookmarkNode* user_node =
      AddURL(bar, /*index=*/0, u"User", GURL("https://user.example/"));
  profile_->GetPrefs()->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                   /*value=*/false);

  bookmark_bridge()->CutToClipboardImpl(user_node, /*is_off_the_record=*/false);
  EXPECT_EQ(1u, bar->children().size());
  EXPECT_FALSE(ReadBookmarkDataSync());

  bookmark_bridge()->CopyToClipboardImpl(user_node,
                                         /*is_off_the_record=*/false);
  auto user_data = ReadBookmarkDataSync();
  ASSERT_TRUE(user_data);
  EXPECT_EQ(GURL("https://user.example/"), user_data->elements[0].url);
}

TEST_F(BookmarkBridgeClipboardTest, NonNormalAndNullNodesCannotBeCopiedOrCut) {
  const BookmarkNode* reading_list_node =
      local_or_syncable_reading_list_manager()->Add(
          GURL("https://reading.example/"), "Reading");
  ASSERT_THAT(reading_list_node, NotNull());
  const BookmarkNode* partner_root =
      partner_bookmarks_shim_->GetPartnerBookmarksRoot();
  ASSERT_THAT(partner_root, NotNull());

  bookmark_bridge()->CopyToClipboardImpl(/*node=*/nullptr,
                                         /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboardImpl(/*node=*/nullptr,
                                        /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());

  bookmark_bridge()->CopyToClipboardImpl(reading_list_node,
                                         /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboardImpl(reading_list_node,
                                        /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  EXPECT_EQ(1u, bookmark_bridge()
                    ->GetChildIdsImpl(
                        local_or_syncable_reading_list_manager()->GetRoot())
                    .size());

  bookmark_bridge()->CopyToClipboardImpl(partner_root,
                                         /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboardImpl(partner_root,
                                        /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
}

TEST_F(BookmarkBridgeClipboardTest, JniCopyAndCutEntriesFilterNonNormalTypes) {
  JNIEnv* const env = AttachCurrentThread();
  const BookmarkNode* bar = bookmark_model()->bookmark_bar_node();
  const GURL copy_url("https://jni-copy.example/");
  const GURL cut_url("https://jni-cut.example/");
  const BookmarkNode* copy_node = AddURL(bar, /*index=*/0, u"Copy", copy_url);
  const BookmarkNode* cut_node = AddURL(bar, /*index=*/1, u"Cut", cut_url);
  const BookmarkNode* reading_list_node =
      local_or_syncable_reading_list_manager()->Add(
          GURL("https://reading-jni.example/"), "Reading");
  ASSERT_THAT(reading_list_node, NotNull());
  const BookmarkNode* partner_root =
      partner_bookmarks_shim_->GetPartnerBookmarksRoot();
  ASSERT_THAT(partner_root, NotNull());

  // Non-normal bookmark types and missing normal IDs are ignored.
  auto reading_list_id = JavaBookmarkIdCreateBookmarkId(
      env, reading_list_node->id(), bookmarks::BOOKMARK_TYPE_READING_LIST);
  bookmark_bridge()->CopyToClipboard(env, reading_list_id,
                                     /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboard(env, reading_list_id,
                                    /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  EXPECT_EQ(1u, bookmark_bridge()
                    ->GetChildIdsImpl(
                        local_or_syncable_reading_list_manager()->GetRoot())
                    .size());

  auto partner_id = JavaBookmarkIdCreateBookmarkId(
      env, partner_root->id(), bookmarks::BOOKMARK_TYPE_PARTNER);
  bookmark_bridge()->CopyToClipboard(env, partner_id,
                                     /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboard(env, partner_id,
                                    /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());

  auto missing_normal_id = JavaBookmarkIdCreateBookmarkId(
      env, /*id=*/999999, bookmarks::BOOKMARK_TYPE_NORMAL);
  bookmark_bridge()->CopyToClipboard(env, missing_normal_id,
                                     /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());
  bookmark_bridge()->CutToClipboard(env, missing_normal_id,
                                    /*is_off_the_record=*/false);
  EXPECT_FALSE(ReadBookmarkDataSync());

  // Normal bookmark IDs resolve via GetNodeByID and copy/cut cleanly.
  auto copy_id = JavaBookmarkIdCreateBookmarkId(
      env, copy_node->id(), bookmarks::BOOKMARK_TYPE_NORMAL);
  bookmark_bridge()->CopyToClipboard(env, copy_id, /*is_off_the_record=*/false);
  EXPECT_EQ(2u, bar->children().size());
  auto copied_data = ReadBookmarkDataSync();
  ASSERT_TRUE(copied_data);
  ASSERT_EQ(1u, copied_data->size());
  EXPECT_EQ(copy_url, copied_data->elements[0].url);

  auto cut_id = JavaBookmarkIdCreateBookmarkId(env, cut_node->id(),
                                               bookmarks::BOOKMARK_TYPE_NORMAL);
  bookmark_bridge()->CutToClipboard(env, cut_id, /*is_off_the_record=*/false);
  ASSERT_EQ(1u, bar->children().size());
  EXPECT_EQ(copy_node, bar->children()[0].get());
  auto cut_data = ReadBookmarkDataSync();
  ASSERT_TRUE(cut_data);
  ASSERT_EQ(1u, cut_data->size());
  EXPECT_EQ(cut_url, cut_data->elements[0].url);
}
