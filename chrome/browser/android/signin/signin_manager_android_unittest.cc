// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "signin_manager_android.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

class MockOfflinePageModel : public offline_pages::StubOfflinePageModel {
 public:
  void DeleteCachedPagesByURLPredicate(
      const offline_pages::UrlPredicate& predicate,
      offline_pages::DeletePageCallback callback) override {
    std::move(callback).Run(DeletePageResult::SUCCESS);
  }
};

std::unique_ptr<KeyedService> BuildOfflinePageModel(SimpleFactoryKey* key) {
  return std::make_unique<MockOfflinePageModel>();
}

}  // namespace

class SigninManagerAndroidTest : public ::testing::Test {
 public:
  SigninManagerAndroidTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~SigninManagerAndroidTest() override {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Testing Profile");
    // TODO(crbug.com/748484): Remove requirement for this delegate in
    // unit_tests.
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile_));
  }

  TestingProfile* profile() { return profile_; }

  // Adds two testing bookmarks to |profile_|.
  bookmarks::BookmarkModel* AddTestBookmarks() {
    profile_->CreateBookmarkModel(true);
    // Creating a BookmarkModel also a creates a StubOfflinePageModel.
    // We need to replace this with a mock that responds to deletions.
    offline_pages::OfflinePageModelFactory::GetInstance()->SetTestingFactory(
        profile_->GetProfileKey(), base::BindRepeating(&BuildOfflinePageModel));
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile_);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                           base::ASCIIToUTF16("Example 1"),
                           GURL("https://example.org/1"));
    bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 1,
                           base::ASCIIToUTF16("Example 2"),
                           GURL("https://example.com/2"));

    return bookmark_model;
  }

  // Calls SigninManager::WipeData(|all_data|) and waits for its completion.
  void WipeData(bool all_data) {
    std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop());
    SigninManagerAndroid::WipeData(profile(), all_data,
                                   run_loop->QuitClosure());
    run_loop->Run();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;  // Owned by |profile_manager_|.

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroidTest);
};

// TODO(crbug.com/929456): This test does not actually test anything; the
// CannedBrowsingDataCacheStorageHelper isn't hooked up to observe any
// deletions. Disabled to allow refactoring of browsing data code.
TEST_F(SigninManagerAndroidTest, DISABLED_DeleteGoogleServiceWorkerCaches) {
  struct TestCase {
    std::string worker_url;
    bool should_be_deleted;
  } kTestCases[] = {
      // A Google domain.
      {"https://google.com/foo/bar", true},

      // A Google domain with long TLD.
      {"https://plus.google.co.uk/?query_params", true},

      // Youtube.
      {"https://youtube.com", false},

      // A random domain.
      {"https://a.b.c.example.com", false},

      // Another Google domain.
      {"https://www.google.de/worker.html", true},

      // Ports don't matter, only TLDs.
      {"https://google.com:8444/worker.html", true},
  };

  // TODO(crbug.com/929456): This helper is not attached anywhere to
  // be able to observe deletions.
  // Add service workers.
  auto helper = base::MakeRefCounted<CannedBrowsingDataCacheStorageHelper>(
      content::BrowserContext::GetDefaultStoragePartition(profile())
          ->GetCacheStorageContext());

  for (const TestCase& test_case : kTestCases)
    helper->Add(url::Origin::Create(GURL(test_case.worker_url)));

  ASSERT_EQ(base::size(kTestCases), helper->GetCount());

  // Delete service workers and wait for completion.
  base::RunLoop run_loop;
  SigninManagerAndroid::WipeData(profile(),
                                 false /* only Google service worker caches */,
                                 run_loop.QuitClosure());
  run_loop.Run();

  // Test whether the correct service worker caches were deleted.
  std::set<url::Origin> remaining_cache_storages = helper->GetOrigins();

  // TODO(crbug.com/929456): If deleted, the key should not be present.
  for (const TestCase& test_case : kTestCases) {
    EXPECT_EQ(test_case.should_be_deleted,
              base::Contains(remaining_cache_storages,
                             url::Origin::Create(GURL(test_case.worker_url))))
        << test_case.worker_url << " should "
        << (test_case.should_be_deleted ? "" : "NOT ")
        << "be deleted, but it was"
        << (test_case.should_be_deleted ? "NOT" : "") << ".";
  }
}

// Tests that wiping all data also deletes bookmarks.
TEST_F(SigninManagerAndroidTest, DeleteBookmarksWhenWipingAllData) {
  bookmarks::BookmarkModel* bookmark_model = AddTestBookmarks();
  ASSERT_GE(bookmark_model->bookmark_bar_node()->children().size(), 0u);
  WipeData(true);
  EXPECT_EQ(0u, bookmark_model->bookmark_bar_node()->children().size());
}

// Tests that wiping Google service worker caches does not delete bookmarks.
TEST_F(SigninManagerAndroidTest, DontDeleteBookmarksWhenDeletingSWCaches) {
  bookmarks::BookmarkModel* bookmark_model = AddTestBookmarks();
  size_t num_bookmarks = bookmark_model->bookmark_bar_node()->children().size();
  ASSERT_GE(num_bookmarks, 0u);
  WipeData(false);
  EXPECT_EQ(num_bookmarks,
            bookmark_model->bookmark_bar_node()->children().size());
}
