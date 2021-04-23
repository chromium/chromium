// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for BoxUploader.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader_test_helper.h"

namespace enterprise_connectors {

class BoxUploaderCreateTest : public BoxUploaderTestBase {};

TEST_F(BoxUploaderCreateTest, TestFileSizes) {
  ASSERT_TRUE(BoxUploader::Create(&test_item_));
  test_item_.SetTotalBytes(BoxApiCallFlow::kChunkFileUploadMinSize * 2);
  ASSERT_FALSE(BoxUploader::Create(&test_item_));
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploader: Pre-Upload Test
////////////////////////////////////////////////////////////////////////////////

class BoxUploaderForTest : public BoxUploader {
 public:
  explicit BoxUploaderForTest(download::DownloadItem* download_item,
                              base::OnceCallback<void(void)> preupload_cb)
      : BoxUploader(download_item), preupload_cb_(std::move(preupload_cb)) {}

 protected:
  // These 2 methods are overridden to intercept the upload API call to test the
  // pre-upload steps specifically.
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override {
    upload_call_created_ = true;
    return std::make_unique<MockApiCallFlow>();
  }

  void StartCurrentApiCall() override {
    if (upload_call_created_)
      std::move(preupload_cb_).Run();
    else
      BoxUploader::StartCurrentApiCall();
  }

 private:
  bool upload_call_created_ = false;
  base::OnceCallback<void(void)> preupload_cb_;
};

class BoxUploaderTest : public BoxUploaderTestBase {
 public:
  BoxUploaderTest()
      : BoxUploaderTestBase(
            FILE_PATH_LITERAL("box_uploader_test.txt.crdownload")),
        uploader_(std::make_unique<BoxUploaderForTest>(
            &test_item_,
            base::BindOnce(&BoxUploaderTest::InterceptedPreUpload,
                           base::Unretained(this)))) {}

 protected:
  void SetUp() override {
    BoxUploaderTestBase::SetUp();
    CreateTemporaryFile();
    InitUploader(uploader_.get());
  }

  void TearDown() override {
    EXPECT_FALSE(upload_success_);
    // If upload was not initiated due to some error, file should've been
    // deleted as part of error handling. Otherwise, upload API call flow got
    // InterceptedPreUpload(), so file was not deleted.
    EXPECT_EQ(upload_initiated_, base::PathExists(GetFilePath()));
  }

  void InterceptedPreUpload() {
    upload_initiated_ = true;
    Quit();
  }

  bool upload_initiated_ = false;
  std::unique_ptr<BoxUploaderForTest> uploader_;
};

TEST_F(BoxUploaderTest, HasExistingFolderOnBox) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploaderTest, NoExistingFolderOnBox_CreatFolder) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_CREATED,
                 kFileSystemBoxCreateFolderResponseBody);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
  EXPECT_FALSE(upload_success_);
}

TEST_F(BoxUploaderTest, AuthenticationRetry) {
  // Check that authentication was refreshed upon net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_UNAUTHORIZED);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Should be retrying authentication, no report via callback yet.
  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_FALSE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), "");

  // Check that it's able to continue after authentication has been refreshed.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploaderTest, CreateFolder_UnexpectedFailure) {
  // Check that the API calls flow is terminated upon any other failure code
  // other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_NOT_FOUND);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Should just report failure via callback.
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_FALSE(upload_initiated_);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), "");
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploader: Preflight Check Test
////////////////////////////////////////////////////////////////////////////////

class BoxUploader_PreflightCheckTest : public BoxUploaderTest {
 public:
  void SetUp() override {
    // Assume there is already a folder_id stored in prefs; will skip directly
    // to preflight check.
    InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);
    BoxUploaderTest::SetUp();
    ASSERT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  }
};

TEST_F(BoxUploader_PreflightCheckTest, Success) {
  // Preflight check passes (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, Conflict) {
  // Preflight check implies a conflict (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_CONFLICT);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  ASSERT_FALSE(upload_initiated_);
  EXPECT_TRUE(download_thread_cb_called_) << "Conflict should terminate flow.";
  // Currently the upload is abandoned.
  // TODO(https://crbug.com/1198617): Update once the conflict is handled.
}

TEST_F(BoxUploader_PreflightCheckTest, CachedFolder404_ButFound) {
  // The cached folder_id returns a 404, so we try to find or create the folder
  // again (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_NOT_FOUND);

  uploader_->TryTask(url_factory_, "test_token");

  // TODO(https://crbug.com/1199194): Re-enable this check without modifying
  // non-Test code much.
  //  RunWithQuitClosure();
  //
  //  EXPECT_TRUE(test_url_loader_factory_.IsPending(
  //  kFileSystemBoxFindFolderUrl));
  //  EXPECT_EQ(prefs->GetString(kFileSystemUploadFolderIdPref), std::string());

  // Found the folder:
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  // Preflight check passes (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, CachedFolder404) {
  // The cached folder_id returns a 404, so we try to find or create the folder
  // again (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_NOT_FOUND);

  uploader_->TryTask(url_factory_, "test_token");

  // TODO(https://crbug.com/1199194): Re-enable this check without modifying
  // non-Test code much.
  //  RunWithQuitClosure();
  //
  //  EXPECT_TRUE(test_url_loader_factory_.IsPending(
  //  kFileSystemBoxFindFolderUrl));
  //  EXPECT_EQ(prefs->GetString(kFileSystemUploadFolderIdPref), std::string());

  // We don't find the ChromeDownloads for and have to create it.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_CREATED,
                 kFileSystemBoxCreateFolderResponseBody);
  // Preflight check passes (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  RunWithQuitClosure();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, AuthenticationRetry) {
  // Check that authentication retry callback is called upon
  // net::HTTP_UNAUTHORIZED (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_UNAUTHORIZED);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Need to retry authentication, so no report via callback yet.
  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);

  // Refresh OAuth2 tokens, then preflight check passes (dummy body since not
  // reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploader: File Delete Test
////////////////////////////////////////////////////////////////////////////////

class BoxUploaderForFileDeleteTest : public BoxUploader {
 public:
  // using BoxUploader::BoxUploader() doesn't work.
  explicit BoxUploaderForFileDeleteTest(download::DownloadItem* download_item)
      : BoxUploader(download_item) {}

  using BoxUploader::OnApiCallFlowDone;

  // Overriding to skip API calls flow for this set of tests.
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override {
    return std::make_unique<MockApiCallFlow>();
  }
  void StartCurrentApiCall() override { OnApiCallFlowDone(true); }
};

class BoxUploader_FileDeleteTest : public BoxUploaderTestBase {
 public:
  BoxUploader_FileDeleteTest()
      : BoxUploaderTestBase(
            FILE_PATH_LITERAL("box_uploader_file_delete_test.txt.crdownload")),
        uploader_(std::make_unique<BoxUploaderForFileDeleteTest>(&test_item_)) {
  }

 protected:
  void SetUp() override {
    BoxUploaderTestBase::SetUp();
    InitUploader(uploader_.get());
  }

  void TearDown() override {
    EXPECT_EQ(authentication_retry_, 0);
    EXPECT_TRUE(download_thread_cb_called_);
  }

  std::unique_ptr<BoxUploaderForFileDeleteTest> uploader_;
};

TEST_F(BoxUploader_FileDeleteTest, TryTask_DeleteOnApiSuccess) {
  CreateTemporaryFile();

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath()));  // Make sure file is deleted.
  ASSERT_TRUE(upload_success_);
}

TEST_F(BoxUploader_FileDeleteTest, NoFileToDelete) {
  // Make sure file doesn't exist.
  ASSERT_FALSE(base::PathExists(GetFilePath()));

  uploader_->OnApiCallFlowDone(true);
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath())) << "No file should be created.";
  ASSERT_FALSE(upload_success_);
}

TEST_F(BoxUploader_FileDeleteTest, OnApiCallFlowFailure) {
  CreateTemporaryFile();

  uploader_->OnApiCallFlowDone(false);
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath()));  // Make sure file is deleted.
  ASSERT_FALSE(upload_success_);
}

////////////////////////////////////////////////////////////////////////////////
// BoxDirectUploaderTest
////////////////////////////////////////////////////////////////////////////////

class BoxDirectUploaderTest : public BoxUploaderTestBase {
 public:
  BoxDirectUploaderTest()
      : BoxUploaderTestBase(
            FILE_PATH_LITERAL("box_direct_uploader_test.txt.crdownload")),
        uploader_(std::make_unique<BoxDirectUploader>(&test_item_)) {}

  void SetUp() override {
    BoxUploaderTestBase::SetUp();

    // Assume there is already a folder_id stored in prefs and preflight check
    // passes; will skip directly to upload.
    InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);
    AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

    CreateTemporaryFile();
    InitUploader(uploader_.get());
    ASSERT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  }

  void TearDown() override {
    EXPECT_FALSE(base::PathExists(GetFilePath()));  // Ensure file is deleted.
  }

  std::unique_ptr<BoxDirectUploader> uploader_;
};

TEST_F(BoxDirectUploaderTest, SuccessfulUpload) {
  AddFetchResult(kFileSystemBoxDirectUploadUrl, net::HTTP_CREATED);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxDirectUploaderTest, UnexpectedFailure) {
  // Check that the API calls flow is terminated upon any other failure code
  // other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxDirectUploadUrl, net::HTTP_NOT_FOUND);

  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Should just report failure via callback.
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_FALSE(upload_success_);
}

}  // namespace enterprise_connectors
