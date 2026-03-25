// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/bookmark_test_with_encryption_stages.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {
namespace {

bool WaitUntilClearTextFilesExist(const base::FilePath& profile_path) {
  return base::test::RunUntil([&]() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(
               profile_path.Append(kLocalOrSyncableBookmarksFileName)) &&
           base::PathExists(profile_path.Append(kAccountBookmarksFileName));
  });
}

bool WaitUntilClearTextFilesDontExist(const base::FilePath& profile_path) {
  return base::test::RunUntil([&]() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return !base::PathExists(
               profile_path.Append(kLocalOrSyncableBookmarksFileName)) &&
           !base::PathExists(profile_path.Append(kAccountBookmarksFileName));
  });
}

bool WaitUntilEncryptedFilesExist(const base::FilePath& profile_path) {
  return base::test::RunUntil([&]() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(profile_path.Append(
               kEncryptedLocalOrSyncableBookmarksFileName)) &&
           base::PathExists(
               profile_path.Append(kEncryptedAccountBookmarksFileName));
  });
}

bool WaitUntilEncryptedFilesDontExist(const base::FilePath& profile_path) {
  return base::test::RunUntil([&]() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return !base::PathExists(profile_path.Append(
               kEncryptedLocalOrSyncableBookmarksFileName)) &&
           !base::PathExists(
               profile_path.Append(kEncryptedAccountBookmarksFileName));
  });
}

void AddBookmarks(BookmarkModel* bookmark_model) {
  const BookmarkNode* const root = bookmark_model->bookmark_bar_node();
  bookmark_model->AddURL(root, 0, u"Title 1", GURL("http://google.com/1"));
  bookmark_model->AddURL(root, 1, u"Title 2", GURL("http://google.com/2"));

  bookmark_model->CreateAccountPermanentFolders();
  const BookmarkNode* const account_root =
      bookmark_model->account_bookmark_bar_node();
  bookmark_model->AddURL(account_root, 0, u"Title 3",
                         GURL("http://google.com/3"));
}

// Verify the number of bookmarks matches the number of bookmarks added in
// AddBookmarks().
MATCHER(HasExpectedBookmarksCount, "") {
  return arg->bookmark_bar_node()->children().size() == 2 &&
         arg->account_bookmark_bar_node()->children().size() == 1;
}

class BaseEncryptedBookmarkBrowserTest : public PlatformBrowserTest {
 public:
  BaseEncryptedBookmarkBrowserTest() = default;

  void SetUp() override {
    auto encryption_stages = GetEncryptionStagesForEachStep();
    // Number of encryption stages should be the same as the number of test
    // steps.
    CHECK(encryption_stages.size() > GetTestPreCount())
        << "One or more test steps are missing an encryption stage.";

    bookmarks::test::InitFeaturesForBookmarkTestEncryptionStage(
        encryption_feature_list_,
        encryption_stages[encryption_stages.size() - GetTestPreCount() - 1]);

    PlatformBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Set a fake sync service to add & keep bookmarks in the account node.
    PlatformBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AccountBookmarkSyncServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(
                              [](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
                                auto service = std::make_unique<
                                    sync_bookmarks::BookmarkSyncService>(
                                    syncer::WipeModelUponSyncDisabledBehavior::
                                        kAlways);
                                service->SetIsTrackingMetadataForTesting();
                                return service;
                              }));
                }));
  }

  BookmarkModel* WaitForBookmarkModel(Profile* profile) {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    return bookmark_model;
  }

  virtual std::vector<bookmarks::BookmarkEncryptionStage>
  GetEncryptionStagesForEachStep() = 0;

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
  base::test::ScopedFeatureList encryption_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// Tests the case where user goes through all the encryption stages.
class AllEncryptedStagesTest : public BaseEncryptedBookmarkBrowserTest {
 public:
  AllEncryptedStagesTest() = default;

  std::vector<bookmarks::BookmarkEncryptionStage>
  GetEncryptionStagesForEachStep() override {
    return {bookmarks::BookmarkEncryptionStage::kDisabled,
            bookmarks::BookmarkEncryptionStage::kWriteBothReadOnlyClear,
            bookmarks::BookmarkEncryptionStage::kWriteBothReadPreferEncrypted,
            // We run this stage twice: first to verify that clear-text files
            // are deleted, and second to verify that bookmarks load correctly
            // from the encrypted files when clear-text files are absent.
            bookmarks::BookmarkEncryptionStage::
                kWriteOnlyEncryptedReadPreferEncrypted,
            bookmarks::BookmarkEncryptionStage::
                kWriteOnlyEncryptedReadPreferEncrypted};
  }
};

// Encryption disabled (prod behavior).
IN_PROC_BROWSER_TEST_F(AllEncryptedStagesTest,
                       PRE_PRE_PRE_PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kDisabled);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());
  AddBookmarks(bookmark_model);

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesDontExist(GetProfile()->GetPath()));
}

// Moving to write both, read clear.
IN_PROC_BROWSER_TEST_F(AllEncryptedStagesTest,
                       PRE_PRE_PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kWriteBothReadOnlyClear);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Moving to write both, read encrypted.
IN_PROC_BROWSER_TEST_F(AllEncryptedStagesTest,
                       PRE_PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kWriteBothReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Moving to write only encrypted, read encrypted.
IN_PROC_BROWSER_TEST_F(AllEncryptedStagesTest, PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::
               kWriteOnlyEncryptedReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesDontExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Staying at write only encrypted, read encrypted.
IN_PROC_BROWSER_TEST_F(AllEncryptedStagesTest, BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::
               kWriteOnlyEncryptedReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesDontExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Tests the case where user skips stage 1 (write both, read clear) and goes
// directly to stage 2 (write both, read encrypted).
class SkipStage1Test : public BaseEncryptedBookmarkBrowserTest {
 public:
  SkipStage1Test() = default;
  SkipStage1Test(const SkipStage1Test&) = delete;
  SkipStage1Test& operator=(const SkipStage1Test&) = delete;

  std::vector<bookmarks::BookmarkEncryptionStage>
  GetEncryptionStagesForEachStep() override {
    return {bookmarks::BookmarkEncryptionStage::kDisabled,
            bookmarks::BookmarkEncryptionStage::kWriteBothReadPreferEncrypted,
            bookmarks::BookmarkEncryptionStage::
                kWriteOnlyEncryptedReadPreferEncrypted};
  }
};

// Encryption disabled (prod behavior).
IN_PROC_BROWSER_TEST_F(SkipStage1Test, PRE_PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kDisabled);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());
  AddBookmarks(bookmark_model);

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesDontExist(GetProfile()->GetPath()));
}

// Moving to write both, read encrypted directly (skipping write both, read
// clear).
IN_PROC_BROWSER_TEST_F(SkipStage1Test, PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kWriteBothReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Moving to write only encrypted, read encrypted.
IN_PROC_BROWSER_TEST_F(SkipStage1Test, BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::
               kWriteOnlyEncryptedReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesDontExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Tests the case where user goes to stage 1 and then directly to stage 3 (write
// only encrypted, read preferred encrypted), skipping stage 2 (write both, read
// encrypted).
class SkipStage2Test : public BaseEncryptedBookmarkBrowserTest {
 public:
  SkipStage2Test() = default;

  std::vector<bookmarks::BookmarkEncryptionStage>
  GetEncryptionStagesForEachStep() override {
    return {bookmarks::BookmarkEncryptionStage::kDisabled,
            bookmarks::BookmarkEncryptionStage::kWriteBothReadOnlyClear,
            bookmarks::BookmarkEncryptionStage::
                kWriteOnlyEncryptedReadPreferEncrypted};
  }
};

// Encryption disabled (prod behavior).
IN_PROC_BROWSER_TEST_F(SkipStage2Test, PRE_PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kDisabled);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());
  AddBookmarks(bookmark_model);

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesDontExist(GetProfile()->GetPath()));
}

// Moving to write both, read clear.
IN_PROC_BROWSER_TEST_F(SkipStage2Test, PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kWriteBothReadOnlyClear);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Moving to write only encrypted, read encrypted.
IN_PROC_BROWSER_TEST_F(SkipStage2Test, BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::
               kWriteOnlyEncryptedReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesDontExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Tests the case where user goes directly to stage 3 (write only encrypted,
// read preferred encrypted). Stage 1 (write both, read clear) and stage 2
// (write both, read encrypted) are skipped.
class SkipStage1AndStage2Test : public BaseEncryptedBookmarkBrowserTest {
 public:
  SkipStage1AndStage2Test() = default;

  std::vector<bookmarks::BookmarkEncryptionStage>
  GetEncryptionStagesForEachStep() override {
    return {bookmarks::BookmarkEncryptionStage::kDisabled,
            // We run this stage twice: first to verify that clear-text files
            // are used as a fallback when encrypted files do not exist yet, and
            // second to verify that encrypted files load correctly and
            // clear-text files are deleted.
            bookmarks::BookmarkEncryptionStage::
                kWriteOnlyEncryptedReadPreferEncrypted,
            bookmarks::BookmarkEncryptionStage::
                kWriteOnlyEncryptedReadPreferEncrypted};
  }
};

// Encryption disabled (prod behavior).
IN_PROC_BROWSER_TEST_F(SkipStage1AndStage2Test,
                       PRE_PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::kDisabled);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());
  AddBookmarks(bookmark_model);

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesDontExist(GetProfile()->GetPath()));
}

// Moving to write only encrypted, read encrypted.
IN_PROC_BROWSER_TEST_F(SkipStage1AndStage2Test, PRE_BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::
               kWriteOnlyEncryptedReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  // The encrypted files didn't exist at the previous stage, so we fallback to
  // loading the clear text files and creating the encrypted files.
  // Clear text files are only deleted after a successful load from the
  // encrypted file so they should still exist.
  EXPECT_TRUE(WaitUntilClearTextFilesExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

// Staying at write only encrypted, read encrypted.
// Verify the clear text files are deleted.
IN_PROC_BROWSER_TEST_F(SkipStage1AndStage2Test, BookmarksStillAvailable) {
  CHECK_EQ(GetCurrentBookmarkEncryptionStageForTesting(),
           bookmarks::BookmarkEncryptionStage::
               kWriteOnlyEncryptedReadPreferEncrypted);
  BookmarkModel* bookmark_model = WaitForBookmarkModel(GetProfile());

  EXPECT_THAT(bookmark_model, HasExpectedBookmarksCount());
  EXPECT_TRUE(WaitUntilClearTextFilesDontExist(GetProfile()->GetPath()));
  EXPECT_TRUE(WaitUntilEncryptedFilesExist(GetProfile()->GetPath()));
}

}  // namespace
}  // namespace bookmarks
