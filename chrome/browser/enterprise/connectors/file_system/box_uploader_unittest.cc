// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for BoxUploader.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

class DownloadItemForTest : public content::FakeDownloadItem {
 public:
  explicit DownloadItemForTest(base::FilePath::StringPieceType file_name) {
    if (temp_dir_.CreateUniqueTempDir()) {
      base::FilePath file_path = temp_dir_.GetPath().Append(file_name);
      SetTemporaryFilePath(file_path);
      SetTargetFilePath(file_path.RemoveFinalExtension());
    }
  }
  void SetTemporaryFilePath(const base::FilePath& file_path) {
    file_path_ = file_path;
  }

  // GetFullPath() is expected to be merged with GetTemporaryPath(). Using
  // GetFullPath() in BoxUploader but using GetTemporaryPath() in test code here
  // for clarity.
  base::FilePath GetTemporaryFilePath() const override { return file_path_; }
  const base::FilePath& GetFullPath() const override { return file_path_; }

 protected:
  base::ScopedTempDir temp_dir_;

  base::FilePath file_path_;
};

class BoxUploaderTest : public testing::Test {
 public:
  BoxUploaderTest()
      : test_item_(FILE_PATH_LITERAL("box_uploader_test.txt.crdownload")),
        uploader_(&test_item_),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        url_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    EXPECT_TRUE(profile_manager_.SetUp());
    prefs = profile_manager_.CreateTestingProfile("test-user")->GetPrefs();
  }

  void SetUp() override {
    testing::Test::SetUp();
    if (test_item_.GetTemporaryFilePath().empty() ||
        !base::WriteFile(test_item_.GetTemporaryFilePath(),
                         "BoxUploaderTest")) {
      FAIL() << "Failed to create temporary file "
             << test_item_.GetTemporaryFilePath();
    }
    uploader_.Init(base::BindRepeating(&BoxUploaderTest::AuthenRetry,
                                       weak_factory_.GetWeakPtr()),
                   base::BindOnce(&BoxUploaderTest::DownloadComplete,
                                  weak_factory_.GetWeakPtr()),
                   prefs);
  }

  void AddFetchResult(const std::string& url,
                      net::HttpStatusCode response_code,
                      const std::string& body) {
    test_url_loader_factory_.AddResponse(url, body, response_code);
  }

  void AuthenRetry() {
    ++authentication_retry_;
    std::move(quit_closure_).Run();
  }

  void DownloadComplete(bool success) {
    download_callback_called_ = true;
    upload_success_ = success;
    std::move(quit_closure_).Run();
  }

  void RunWithQuitClosure() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  DownloadItemForTest test_item_;
  BoxUploader uploader_;

  int authentication_retry_{0};
  bool download_callback_called_{false};
  bool upload_success_{false};

  content::BrowserTaskEnvironment task_environment_;
  // Decoder and TestingProfileManager can only be declared after
  // TaskEnvironment.
  data_decoder::test::InProcessDataDecoder decoder_;
  TestingProfileManager profile_manager_;

  PrefService* prefs;
  base::OnceClosure quit_closure_;

  // For uploader.TryTask().
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_factory_;
  base::WeakPtrFactory<BoxUploaderTest> weak_factory_{this};
};

TEST_F(BoxUploaderTest, HasExistingFolder) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxUploaderTest, NoExistingFolder) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_CREATED,
                 kFileSystemBoxCreateFolderResponseBody);
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxUploaderTest, AuthenticationFailureInTryTask) {
  // Check that authentication retry callback is called upon
  // net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_UNAUTHORIZED,
                 std::string());
  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();
  ASSERT_EQ(authentication_retry_, 1);

  // Should be retrying authentication, no report via callback yet.
  EXPECT_FALSE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(), "");

  // Check that it's able to continue after authentication has been refreshed.
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.
  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();
  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxUploaderTest, UnexpectedFailureInTryTask) {
  // Check that authentication retry callback is NOT called upon
  // any other failure code other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_NOT_FOUND,
                 std::string());
  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();
  ASSERT_EQ(authentication_retry_, 0);

  // Should just report failure via callback.
  EXPECT_TRUE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(), "");
}

class BoxUploaderWithSavedFolderPrefTest : public BoxUploaderTest {
 public:
  void SetUp() override {
    prefs->SetString(kFileSystemUploadFolderIdPref,
                     kFileSystemBoxSavedInPrefFolderId);
    BoxUploaderTest::SetUp();
  }
};

TEST_F(BoxUploaderWithSavedFolderPrefTest, IfPreflightCheckPasses) {
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxSavedInPrefFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxUploaderWithSavedFolderPrefTest, IfPreflightCheckConflict) {
  // TODO(https://crbug.com/1198617): This implies a conflict and currently we
  // abandon the upload.
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_CONFLICT,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxSavedInPrefFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
}

TEST_F(BoxUploaderWithSavedFolderPrefTest,
       IfPreflightCheck404ButBoxFolderExists) {
  // The cached folder_id returns a 404, so we try to find or create the folder
  // again
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_NOT_FOUND,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");

  // TODO(https://crbug.com/1199194): Re-enable this check without modifying
  // non-Test code much.
  //  RunWithQuitClosure();
  //
  //  EXPECT_TRUE(test_url_loader_factory_.IsPending(
  //  kFileSystemBoxFindFolderUrl));
  //  EXPECT_EQ(prefs->GetString(kFileSystemUploadFolderIdPref), std::string());

  // We find the folder.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxUploaderWithSavedFolderPrefTest,
       IfPreflightCheck404AndFolderDoesNotExist) {
  // The cached folder_id returns a 404, so we try to find or create the folder
  // again
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_NOT_FOUND,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");

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
  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.
  RunWithQuitClosure();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

TEST_F(BoxUploaderWithSavedFolderPrefTest,
       AuthenticationFailureInPreflightCheck) {
  // Check that authentication retry callback is called upon
  // net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_UNAUTHORIZED,
                 std::string());
  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();
  ASSERT_EQ(authentication_retry_, 1);

  // Should be retrying authentication, no report via callback yet.
  EXPECT_FALSE(download_callback_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxSavedInPrefFolderId);

  AddFetchResult(
      kFileSystemBoxPreflightCheckUrl, net::HTTP_OK,
      std::string());  // Dummy body since we are not reading from body.
  AddFetchResult(
      kFileSystemBoxWholeFileUploadUrl, net::HTTP_CREATED,
      std::string());  // Dummy body since we are not reading from body.

  uploader_.TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_EQ(uploader_.GetFolderIdForTesting(),
            kFileSystemBoxSavedInPrefFolderId);
  EXPECT_TRUE(download_callback_called_);
  EXPECT_TRUE(upload_success_);
}

}  // namespace enterprise_connectors
