// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/android/bookmark_bridge.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/reading_list/android/reading_list_manager.h"
#include "chrome/browser/reading_list/android/reading_list_manager_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/android/bookmark_id.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/page_image_service/image_service.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/features.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using bookmarks::android::JavaBookmarkIdGetId;
using page_image_service::ImageService;

// Unit tests for `BookmarkBridge`.
class BookmarkBridgeTest : public testing::Test {
 public:
  BookmarkBridgeTest() = default;
  ~BookmarkBridgeTest() override = default;

  BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

  BookmarkBridge* bookmark_bridge() { return bookmark_bridge_.get(); }

  ReadingListManager* local_or_syncable_reading_list_manager() {
    return local_or_syncable_reading_list_manager_.get();
  }

  ReadingListManager* account_reading_list_manager() {
    return account_reading_list_manager_.get();
  }

  const BookmarkNode* AddURL(const BookmarkNode* parent,
                             size_t index,
                             const std::u16string& title,
                             const GURL& url) {
    return bookmark_model()->AddURL(parent, index, title, url,
                                    /*meta_info=*/nullptr, clock_.Now());
  }

  void CreateBookmarkBridge(bool enable_account_bookmarks) {
    bookmark_bridge_.reset();

    ReadingListManagerImpl::IdGenerationFunction rl_id_gen_func =
        base::BindRepeating([](int64_t* id) { return (*id)++; },
                            base::Owned(std::make_unique<int64_t>(0)));

    local_or_syncable_reading_list_model_ =
        CreateReadingListModel(syncer::StorageType::kUnspecified);
    auto local_or_syncable_reading_list_manager =
        std::make_unique<ReadingListManagerImpl>(
            local_or_syncable_reading_list_model_.get(), rl_id_gen_func);
    local_or_syncable_reading_list_manager_ =
        local_or_syncable_reading_list_manager.get();
    account_reading_list_model_ =
        CreateReadingListModel(syncer::StorageType::kAccount);

    std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client =
        std::make_unique<bookmarks::TestBookmarkClient>();
    BookmarkNode* managed_node = bookmark_client->EnableManagedNode();
    managed_node->SetTitle(u"Managed bookmarks");
    bookmark_model_ =
        std::make_unique<bookmarks::BookmarkModel>(std::move(bookmark_client));
    bookmark_model_->LoadEmptyForTest();

    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());

    std::unique_ptr<ReadingListManagerImpl> account_reading_list_manager =
        nullptr;
    if (enable_account_bookmarks) {
      features_.InitWithFeatures(
          /*enabled_features=*/{syncer::kEnableBookmarkFoldersForAccountStorage,
                                syncer::kReplaceSyncPromosWithSignInPromos},
          /*disabled_features=*/{});
      bookmark_model_->CreateAccountPermanentFolders();

      account_reading_list_manager = std::make_unique<ReadingListManagerImpl>(
          account_reading_list_model_.get(), rl_id_gen_func);
      account_reading_list_manager_ = account_reading_list_manager.get();
    }

    // TODO(crbug.com/1503231): Add image_service once a mock is available.
    bookmark_bridge_ = std::make_unique<BookmarkBridge>(
        profile_, bookmark_model_.get(), managed_bookmark_service_,
        partner_bookmarks_shim_,
        std::move(local_or_syncable_reading_list_manager),
        std::move(account_reading_list_manager), /*image_service=*/nullptr);

    bookmark_bridge_->LoadEmptyPartnerBookmarkShimForTesting(
        AttachCurrentThread());
    partner_bookmarks_shim_->SetPartnerBookmarksRoot(
        PartnerBookmarksReader::CreatePartnerBookmarksRootForTesting());
  }

 protected:
  // testing::Test
  void SetUp() override {
    // Setup the profile, and service factories.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        "BookmarkBridgeTest", /*testing_factories=*/{
            {BookmarkModelFactory::GetInstance(),
             BookmarkModelFactory::GetDefaultFactory()},
            {ManagedBookmarkServiceFactory::GetInstance(),
             ManagedBookmarkServiceFactory::GetDefaultFactory()}});

    // Setup bookmark sources from their factories.
    managed_bookmark_service_ =
        ManagedBookmarkServiceFactory::GetForProfile(profile_);
    partner_bookmarks_shim_ =
        PartnerBookmarksShim::BuildForBrowserContext(profile_);

    CreateBookmarkBridge(/*enable_account_bookmarks=*/false);
  }

  void TearDown() override {
    // reading_list_model_.reset();
    bookmark_bridge_.reset();
    profile_manager_.reset();
  }

  std::unique_ptr<ReadingListModel> CreateReadingListModel(
      syncer::StorageType storage_type) {
    auto storage = std::make_unique<FakeReadingListModelStorage>();
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();
    auto reading_list_model = std::make_unique<ReadingListModelImpl>(
        std::move(storage), storage_type,
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &clock_);
    EXPECT_TRUE(storage_ptr->TriggerLoadCompletion());
    return std::move(reading_list_model);
  }

  base::test::ScopedFeatureList features_;
  base::SimpleTestClock clock_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<BookmarkModel> bookmark_model_;
  raw_ptr<ManagedBookmarkService> managed_bookmark_service_;
  raw_ptr<PartnerBookmarksShim> partner_bookmarks_shim_;

  std::unique_ptr<ReadingListModel> account_reading_list_model_;
  raw_ptr<ReadingListManagerImpl> account_reading_list_manager_;

  std::unique_ptr<ReadingListModel> local_or_syncable_reading_list_model_;
  raw_ptr<ReadingListManagerImpl> local_or_syncable_reading_list_manager_;

  std::unique_ptr<BookmarkBridge> bookmark_bridge_;

  content::BrowserTaskEnvironment task_environment_;
};

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
  GURL url = GURL("http://foo.com");
  ASSERT_EQ(
      nullptr,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));

  // Add to the reading list and verify that it's the most recently added.
  auto* recently_added =
      local_or_syncable_reading_list_manager()->Add(url, "test");
  ASSERT_EQ(
      recently_added,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));

  local_or_syncable_reading_list_manager_->SetIsLoadedForTests(false);
  ASSERT_EQ(
      nullptr,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));
}

TEST_F(
    BookmarkBridgeTest,
    TestGetMostRecentlyAddedUserBookmarkIdForUrlBeforeReadingListLoadsWithAccountBookmarks) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);
  GURL url = GURL("http://foo.com");
  ASSERT_EQ(
      nullptr,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));

  // Add to the reading list and verify that it's the most recently added.
  auto* recently_added = account_reading_list_manager_->Add(url, "test");
  ASSERT_EQ(
      recently_added,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));

  account_reading_list_manager_->SetIsLoadedForTests(false);
  ASSERT_EQ(
      nullptr,
      bookmark_bridge()->GetMostRecentlyAddedUserBookmarkIdForUrlImpl(url));
}

TEST_F(BookmarkBridgeTest, TestIsBookmarked) {
  JNIEnv* const env = AttachCurrentThread();
  GURL url = GURL("http://foo.com");
  auto java_url = url::GURLAndroid::FromNativeGURL(env, url);
  ASSERT_FALSE(bookmark_bridge()->IsBookmarked(
      env, JavaParamRef<jobject>(env, java_url.obj())));

  AddURL(bookmark_model()->other_node(), 0, u"foo", url);
  ASSERT_TRUE(bookmark_bridge()->IsBookmarked(
      env, JavaParamRef<jobject>(env, java_url.obj())));

  bookmark_model()->RemoveAllUserBookmarks();
  ASSERT_FALSE(bookmark_bridge()->IsBookmarked(
      env, JavaParamRef<jobject>(env, java_url.obj())));

  local_or_syncable_reading_list_manager()->Add(url, "bar");
  ASSERT_TRUE(bookmark_bridge()->IsBookmarked(
      env, JavaParamRef<jobject>(env, java_url.obj())));
}

TEST_F(BookmarkBridgeTest, TestGetTopLevelFolderIds) {
  std::vector<const BookmarkNode*> folders =
      bookmark_bridge()->GetTopLevelFolderIdsImpl(
          /*ignore_visibility=*/false);

  // The 2 folders should be: mobile bookmarks, reading list.
  EXPECT_EQ(2u, folders.size());
  EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
  EXPECT_EQ(u"Reading list", folders[1]->GetTitle());

  // When ignoring visibility, all top-level folders should be visible.
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*ignore_visibility=*/true);

  // The 2 folders should be: mobile bookmarks, reading list.
  EXPECT_EQ(5u, folders.size());
  EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
  EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
  EXPECT_EQ(u"Other bookmarks", folders[2]->GetTitle());
  EXPECT_EQ(u"Managed bookmarks", folders[3]->GetTitle());
  EXPECT_EQ(u"Reading list", folders[4]->GetTitle());

  // Adding a bookmark to the bookmark bar will include it in the top level
  // folders that are returned.
  AddURL(bookmark_model()->bookmark_bar_node(), 0, u"first",
         GURL("http://foo.com"));
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*ignore_visibility=*/false);
  EXPECT_EQ(3u, folders.size());
  EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
  EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
  EXPECT_EQ(u"Reading list", folders[2]->GetTitle());
}

TEST_F(BookmarkBridgeTest, AccountFoldersNullWhileNotEnabled) {
  JNIEnv* const env = AttachCurrentThread();
  EXPECT_TRUE(bookmark_bridge()->GetAccountMobileFolderId(env).is_null());
  EXPECT_TRUE(bookmark_bridge()->GetAccountOtherFolderId(env).is_null());
  EXPECT_TRUE(bookmark_bridge()->GetAccountDesktopFolderId(env).is_null());
  EXPECT_TRUE(bookmark_bridge()->GetAccountReadingListFolder(env).is_null());
}

// TODO(crbug.com/1509189): Also enable bookmark account folders here.
TEST_F(BookmarkBridgeTest, TestGetTopLevelFolderIdsAccountActive) {
  CreateBookmarkBridge(/*enable_account_bookmarks=*/true);
  std::vector<const BookmarkNode*> folders =
      bookmark_bridge()->GetTopLevelFolderIdsImpl(
          /*ignore_visibility=*/false);

  // The 2 folders should be: mobile bookmarks, reading list.
  EXPECT_EQ(4u, folders.size());
  EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
  EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
  EXPECT_EQ(u"Mobile bookmarks", folders[1]->GetTitle());
  EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
  EXPECT_EQ(u"Reading list", folders[2]->GetTitle());
  EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
  EXPECT_EQ(u"Reading list", folders[3]->GetTitle());
  EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));

  // Adding a bookmark to the bookmark bar will include it in the top level
  // folders that are returned.
  AddURL(bookmark_model()->bookmark_bar_node(), 0, u"first",
         GURL("http://foo.com"));
  folders = bookmark_bridge()->GetTopLevelFolderIdsImpl(
      /*ignore_visibility=*/false);
  EXPECT_EQ(5u, folders.size());
  EXPECT_EQ(u"Mobile bookmarks", folders[0]->GetTitle());
  EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[0]));
  EXPECT_EQ(u"Mobile bookmarks", folders[2]->GetTitle());
  EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[2]));
  EXPECT_EQ(u"Bookmarks bar", folders[1]->GetTitle());
  EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[1]));
  EXPECT_EQ(u"Reading list", folders[3]->GetTitle());
  EXPECT_TRUE(bookmark_bridge()->IsAccountBookmarkImpl(folders[3]));
  EXPECT_EQ(u"Reading list", folders[4]->GetTitle());
  EXPECT_FALSE(bookmark_bridge()->IsAccountBookmarkImpl(folders[4]));
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
  ASSERT_EQ(2, bookmark_bridge()->GetUnreadCount(
                   env, JavaParamRef<jobject>(
                            env, bookmark_bridge()
                                     ->GetLocalOrSyncableReadingListFolder(env)
                                     .obj())));

  local_or_syncable_reading_list_manager()->SetReadStatus(url, true);
  ASSERT_EQ(1, bookmark_bridge()->GetUnreadCount(
                   env, JavaParamRef<jobject>(
                            env, bookmark_bridge()
                                     ->GetLocalOrSyncableReadingListFolder(env)
                                     .obj())));
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

  const BookmarkNode* node =
      AddURL(bookmark_model()->other_node(), 0, u"test", url);
  bookmark_bridge()->MoveBookmarkImpl(node, bookmarks::BOOKMARK_TYPE_NORMAL,
                                      bookmark_model()->other_node(),
                                      bookmarks::BOOKMARK_TYPE_NORMAL, 0);
  // Early return means we don't hit a DCHECK.
}

TEST_F(BookmarkBridgeTest, TestMoveBookmarkToReadingList) {
  GURL url = GURL("http://foo.com");
  const std::u16string title = u"test";

  const BookmarkNode* node =
      AddURL(bookmark_model()->other_node(), 0, title, url);
  bookmark_bridge()->MoveBookmarkImpl(
      node, bookmarks::BOOKMARK_TYPE_NORMAL,
      local_or_syncable_reading_list_manager_->GetRoot(),
      bookmarks::BOOKMARK_TYPE_READING_LIST, 0);
  // Get children of new parent and verify it has the node in it.
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(
          local_or_syncable_reading_list_manager_->GetRoot());
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
      local_or_syncable_reading_list_manager_->GetRoot(),
      bookmarks::BOOKMARK_TYPE_READING_LIST, 0);
  // Get children of new parent and verify it hasn't been moved
  std::vector<const BookmarkNode*> children =
      bookmark_bridge()->GetChildIdsImpl(
          local_or_syncable_reading_list_manager_->GetRoot());
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
      local_or_syncable_reading_list_manager_->GetRoot());
  ASSERT_EQ(0u, children.size());
}
