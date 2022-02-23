// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for BoxUploader.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"

#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader_test_helper.h"
#include "chrome/browser/enterprise/connectors/file_system/uma_metrics_util.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#define ASSERT_REASON_EQ(expected, actual)                          \
  ASSERT_EQ(download::DOWNLOAD_INTERRUPT_REASON_##expected, actual) \
      << "  download::DOWNLOAD_INTERRUPT_REASON_"                   \
      << DownloadInterruptReasonToString(actual);

using testing::Return;

namespace {
const base::FilePath::StringType kUploadFileName(
    FILE_PATH_LITERAL("box_uploader_test.txt"));
}  // namespace

namespace enterprise_connectors {

using Reason = BoxUploader::InterruptReason;

class BoxUploaderCreateTest : public BoxUploaderTestBase {
 public:
  void RunUploader() {
    // Assume preflight check passes, which is tested thoroughly separately.
    AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
    InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);
    uploader_ = BoxUploader::Create(&test_item_);
    ASSERT_TRUE(uploader_);
    InitUploader(uploader_.get());
    InitQuitClosure();
    uploader_->TryTask(url_factory_, "test_token");
    RunWithQuitClosure();
  }

  std::unique_ptr<BoxUploader> uploader_;
};

TEST_F(BoxUploaderCreateTest, TestSmallFile) {
  // Upload whole file, fail 404 (empty body since not reading from body):
  AddFetchResult(kFileSystemBoxDirectUploadUrl, net::HTTP_UNAUTHORIZED);

  CreateTemporaryFile();  // BoxDirectUploader reads the file.
  RunUploader();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_TRUE(base::PathExists(GetFilePath()));  // File not deleted yet.
}

TEST_F(BoxUploaderCreateTest, TestBigFile) {
  test_item_.SetTotalBytes(BoxApiCallFlow::kChunkFileUploadMinSize * 2);

  // Create upload session, fail 404 (empty body since not reading from body):
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl,
                 net::HTTP_UNAUTHORIZED);

  RunUploader();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_FALSE(upload_success_);
}

TEST_F(BoxUploaderCreateTest, LoadFromReroutedInfo) {
  test_item_.SetState(State::COMPLETE);

  DownloadItemRerouteInfo rerouted_info;
  rerouted_info.set_service_provider(BoxUploader::kServiceProvider);
  rerouted_info.mutable_box()->set_file_id(kFileSystemBoxUploadResponseFileId);
  rerouted_info.mutable_box()->set_folder_id(kFileSystemBoxFolderIdInPref);
  test_item_.SetRerouteInfo(rerouted_info);

  uploader_ = BoxUploader::Create(&test_item_);
  ASSERT_TRUE(uploader_);
  ASSERT_EQ(uploader_->GetUploadedFileUrl(),
            GURL(kFileSystemBoxUploadResponseFileUrl));
  ASSERT_EQ(uploader_->GetDestinationFolderUrl(),
            GURL(kFileSystemBoxFolderIdInPrefUrl));

  ASSERT_FALSE(authentication_retry_);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_FALSE(progress_update_cb_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_FALSE(base::PathExists(GetFilePath()));  // File not created.
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploader: Pre-Upload Test
////////////////////////////////////////////////////////////////////////////////

class BoxUploaderForTest : public BoxUploader {
 public:
  explicit BoxUploaderForTest(download::DownloadItem* download_item,
                              base::OnceCallback<void(void)> preupload_cb)
      : BoxUploader(download_item), preupload_cb_(std::move(preupload_cb)) {}

  using BoxUploader::GetUploadFileName;

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
      : BoxUploaderTestBase(kUploadFileName),
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
    // 1 update in Init(), and 1 update in StartUpload() when PreflightCheck
    // succeeds.
    EXPECT_EQ(progress_update_cb_called_, 2);
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

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  EXPECT_EQ(uploader_->GetUploadFileName().value(), kUploadFileName);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploaderTest, NoExistingFolderOnBox_CreatFolder) {
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_CREATED,
                 kFileSystemBoxCreateFolderResponseBody);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
  EXPECT_EQ(uploader_->GetUploadFileName().value(), kUploadFileName);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
  EXPECT_FALSE(upload_success_);
}

TEST_F(BoxUploaderTest, AuthenticationRetry) {
  // Check that authentication was refreshed upon net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_UNAUTHORIZED);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Should be retrying authentication, no report via callback yet.
  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_EQ(progress_update_cb_called_, 1);  // 1 in Init().
  EXPECT_FALSE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), "");

  // Check that it's able to continue after authentication has been refreshed.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseBody);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxFindFolderResponseFolderId);
  EXPECT_EQ(uploader_->GetUploadFileName().value(), kUploadFileName);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploaderTest, CreateFolder_UnexpectedFailure) {
  // Check that the API calls flow is terminated upon any other failure code
  // other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_NOT_FOUND);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Should just report failure via callback.
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_FALSE(upload_initiated_);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), "");
}

TEST_F(BoxUploaderTest, CreateFolder_TerminateTask) {
  // Check that the API calls flow is terminated upon any other failure code
  // other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  // Without mock fetch result for create folder, it's waiting. Test terminate.
  const auto terminate_reason = Reason::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  uploader_->TerminateTask(terminate_reason);
  RunWithQuitClosure();

  // Should just report failure via callback.
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_FALSE(upload_initiated_);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_EQ(reason_, terminate_reason);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), "");
}

TEST_F(BoxUploaderTest, CreateFolder_Conflict_DueToIndexingLatency) {
  // Check that a conflict response for create folder step will continue using
  // the provided conflicting folder.
  AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                 kFileSystemBoxFindFolderResponseEmptyEntriesList);
  AddFetchResult(kFileSystemBoxCreateFolderUrl, net::HTTP_CONFLICT,
                 kFileSystemBoxCreateFolderConflictResponseBody);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(),
            kFileSystemBoxCreateFolderResponseFolderId);
  EXPECT_EQ(uploader_->GetUploadFileName().value(), kUploadFileName);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
  EXPECT_FALSE(upload_success_);
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploader: Preflight Check Test
////////////////////////////////////////////////////////////////////////////////

class BoxUploader_PreflightCheckTest : public BoxUploaderTest {
 public:
  using AttemptCount = BoxUploader::UploadAttemptCount;
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

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(kFileSystemBoxFolderIdInPref, uploader_->GetFolderIdForTesting());
  EXPECT_EQ(kUploadFileName, uploader_->GetUploadFileName().value());
  EXPECT_EQ(kUploadFileName, file_name_reported_back_.value());
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, ConflictAndSuccessAfterkMaxUniqueTries) {
  // Preflight check implies a conflict until retries time out.
  base::HistogramTester histogram_tester;

  for (int i = 0; i < 10; i++) {
    AddSequentialFetchResult(kFileSystemBoxPreflightCheckUrl,
                             net::HTTP_CONFLICT);
  }
  AddSequentialFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_ZA");
  base::test::ScopedRestoreDefaultTimezone sast_time("Africa/Johannesburg");

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  ASSERT_TRUE(upload_initiated_);

  // TODO(http://crbug.com/1215562): Remove this hack once this bug is resolved.
  base::Time expected_start;
  DCHECK(base::Time::FromLocalExploded(kTestDateTime, &expected_start));
  base::Time::Exploded exploded;
  expected_start.LocalExplode(&exploded);
  std::string expected_name = base::StringPrintf(
      "box_uploader_test - %04d-%02d-%02dT%02d%02d%02d.%03d UTC+2h00.txt",
      exploded.year, exploded.month, exploded.day_of_month, exploded.hour,
      exploded.minute, exploded.second, exploded.millisecond);
  // ---------------------------------------------------------------------------

  EXPECT_EQ(uploader_->GetUploadFileName().MaybeAsASCII(), expected_name);
  histogram_tester.ExpectUniqueSample(kUniquifierUmaLabel,
                                      AttemptCount::kTimestampBasedName, 1);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, ConflictEvenWithTimestamp) {
  // Preflight check implies a conflict  including even for the timestamp based
  // filename. The upload should finally fail.
  base::HistogramTester histogram_tester;

  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_CONFLICT);
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_ZA");
  base::test::ScopedRestoreDefaultTimezone sast_time("Africa/Johannesburg");

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  ASSERT_FALSE(upload_initiated_);

  // The last tried filename is timestamp based even though it also fails.
  const base::FilePath expected_file_name(
      FILE_PATH_LITERAL("box_uploader_test.abandoned.txt"));
  EXPECT_EQ(uploader_->GetUploadFileName(), expected_file_name);
  EXPECT_EQ(file_name_reported_back_, expected_file_name);
  histogram_tester.ExpectUniqueSample(kUniquifierUmaLabel,
                                      AttemptCount::kAbandonedUpload, 1);
  EXPECT_TRUE(download_thread_cb_called_)
      << "Conflict, including with timestamp, should terminate flow.";
}

TEST_F(BoxUploader_PreflightCheckTest, ConflictThenSuccess) {
  // Preflight check results for successive filenames:
  base::HistogramTester histogram_tester;

  // box_uploader_test.txt
  AddSequentialFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_CONFLICT);

  // box_uploader_test (1).txt
  AddSequentialFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_CONFLICT);

  // box_uploader_test (2).txt
  AddSequentialFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  ASSERT_TRUE(upload_initiated_);
  EXPECT_EQ(uploader_->GetUploadFileName().MaybeAsASCII(),
            "box_uploader_test (2).txt");
  histogram_tester.ExpectUniqueSample(kUniquifierUmaLabel, 2, 1);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, CachedFolder404_ButFound) {
  // The cached folder_id returns a 404, so we try to find or create the folder
  // again (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_NOT_FOUND);

  InitQuitClosure();
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
  EXPECT_EQ(kUploadFileName, uploader_->GetUploadFileName().value());
  EXPECT_EQ(kUploadFileName, file_name_reported_back_.value());
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, CachedFolder404) {
  // The cached folder_id returns a 404, so we try to find or create the folder
  // again (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_NOT_FOUND);

  InitQuitClosure();
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
  EXPECT_EQ(kUploadFileName, uploader_->GetUploadFileName().value());
  EXPECT_EQ(kUploadFileName, file_name_reported_back_.value());
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

TEST_F(BoxUploader_PreflightCheckTest, AuthenticationRetry) {
  // Check that authentication retry callback is called upon
  // net::HTTP_UNAUTHORIZED (dummy body since not reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_UNAUTHORIZED);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Need to retry authentication, so no report via callback yet.
  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_EQ(progress_update_cb_called_, 1);  // 1 in Init().
  ASSERT_FALSE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);

  // Refresh OAuth2 tokens, then preflight check passes (dummy body since not
  // reading from it):
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  EXPECT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  EXPECT_EQ(kUploadFileName, uploader_->GetUploadFileName().value());
  EXPECT_EQ(kUploadFileName, file_name_reported_back_.value());
  ASSERT_TRUE(upload_initiated_);
  EXPECT_FALSE(download_thread_cb_called_);  // InterceptedPreUpload() above.
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploader: Net Error Test
////////////////////////////////////////////////////////////////////////////////

class MockApiCallFlowWithNetError : public MockApiCallFlow {
 public:
  using TaskCallback = base::OnceCallback<void(BoxApiCallResponse)>;
  MockApiCallFlowWithNetError(TaskCallback cb, net::Error error)
      : cb_(std::move(cb)), error_(error) {}

  void Start(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const std::string& access_token) override {
    std::move(cb_).Run(BoxApiCallResponse{false, error_});
  }

 private:
  TaskCallback cb_;
  net::Error error_;
};

class BoxUploaderForNetErrorTest : public BoxUploader {
 public:
  explicit BoxUploaderForNetErrorTest(download::DownloadItem* download_item)
      : BoxUploader(download_item) {}

 protected:
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override {
    return std::make_unique<MockApiCallFlowWithNetError>(
        base::BindOnce(&BoxUploaderForNetErrorTest::OnMockFlowResponse,
                       base::Unretained(this)),
        net::Error::ERR_TIMED_OUT);
  }

  void OnMockFlowResponse(BoxApiCallResponse response) {
    ASSERT_EQ(response.success, EnsureSuccess(response));
  }
};

class BoxUploaderNetErrorTest : public BoxUploaderTestBase {
 public:
  BoxUploaderNetErrorTest() : BoxUploaderTestBase(kUploadFileName) {}

 protected:
  void SetUp() override {
    BoxUploaderTestBase::SetUp();

    // To fly through first few steps to then trigger MakeFileUploadApiCall()
    // where we make the MockApiCallFlowWithNetError.
    AddFetchResult(kFileSystemBoxFindFolderUrl, net::HTTP_OK,
                   kFileSystemBoxFindFolderResponseBody);
    AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
    // No CreateTemporaryFile() here to make sure file delete failure does not
    // overwrite the pre-existing net error.
    uploader_ = std::make_unique<BoxUploaderForNetErrorTest>(&test_item_);
    InitUploader(uploader_.get());
  }

  void TearDown() override {
    EXPECT_FALSE(upload_success_);
    // If upload was not initiated due to some error, file should've been
    // deleted as part of error handling.
    EXPECT_FALSE(base::PathExists(GetFilePath()));
    // 1 update in Init(), 1 update in StartUpload() when PreflightCheck
    // succeeds, and 1 update in OnApiCallFlowDone().
    EXPECT_EQ(progress_update_cb_called_, 3);
  }
  std::unique_ptr<BoxUploaderForNetErrorTest> uploader_;
};

TEST_F(BoxUploaderNetErrorTest, UploadTimedOut) {
  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);  // Due to net::Error.
  EXPECT_EQ(reason_, download::ConvertNetErrorToInterruptReason(
                         net::Error::ERR_TIMED_OUT,
                         download::DOWNLOAD_INTERRUPT_FROM_NETWORK));
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

  void StartCurrentApiCall() override {
    OnApiCallFlowDone(Reason::DOWNLOAD_INTERRUPT_REASON_NONE,
                      kFileSystemBoxUploadResponseFileId);
  }
};

class BoxUploader_FileDeleteTest : public BoxUploaderTestBase {
 public:
  BoxUploader_FileDeleteTest()
      : BoxUploaderTestBase(
            FILE_PATH_LITERAL("box_uploader_file_delete_test.txt")),
        uploader_(std::make_unique<BoxUploaderForFileDeleteTest>(&test_item_)) {
  }

 protected:
  void SetUp() override {
    BoxUploaderTestBase::SetUp();
    InitUploader(uploader_.get());
  }

  void TearDown() override {
    EXPECT_EQ(authentication_retry_, 0);
    EXPECT_EQ(progress_update_cb_called_ > 0, download_thread_cb_called_);
    EXPECT_FALSE(test_item_.GetFileExternallyRemoved());
  }

  std::unique_ptr<BoxUploaderForFileDeleteTest> uploader_;
};

TEST_F(BoxUploader_FileDeleteTest, TryTask_DeleteOnApiSuccess) {
  CreateTemporaryFile();

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath()));  // Make sure file is deleted.
  EXPECT_TRUE(progress_update_cb_called_);
  ASSERT_TRUE(upload_success_);
}

TEST_F(BoxUploader_FileDeleteTest, NoFileToDelete) {
  // Make sure file doesn't exist.
  ASSERT_FALSE(base::PathExists(GetFilePath()));

  InitQuitClosure();
  uploader_->OnApiCallFlowDone(Reason::DOWNLOAD_INTERRUPT_REASON_NONE,
                               kFileSystemBoxUploadResponseFileId);
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath())) << "No file should be created.";
  EXPECT_TRUE(progress_update_cb_called_);
  EXPECT_TRUE(download_thread_cb_called_);
  ASSERT_FALSE(upload_success_);
}

TEST_F(BoxUploader_FileDeleteTest, OnApiCallFlowFailure) {
  CreateTemporaryFile();

  InitQuitClosure();
  uploader_->OnApiCallFlowDone(Reason::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
                               std::string());
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath()));  // Make sure file is deleted.
  EXPECT_TRUE(progress_update_cb_called_);
  EXPECT_TRUE(download_thread_cb_called_);
  ASSERT_FALSE(upload_success_);
}

TEST_F(BoxUploader_FileDeleteTest, TerminateTask) {
  CreateTemporaryFile();

  InitQuitClosure();
  uploader_->TerminateTask(Reason::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  RunWithQuitClosure();

  EXPECT_FALSE(base::PathExists(GetFilePath()));  // Make sure file is deleted.
  EXPECT_TRUE(progress_update_cb_called_);
  EXPECT_TRUE(download_thread_cb_called_);
  ASSERT_FALSE(upload_success_);
}

DownloadItemRerouteInfo MakeDownloadItemRerouteInfo(
    std::string folder_id = std::string(),
    std::string file_id = std::string()) {
  DownloadItemRerouteInfo info;
  info.set_service_provider(BoxUploader::kServiceProvider);
  info.mutable_box();  // Set the oneof to be Box.
  if (!folder_id.empty())
    info.mutable_box()->set_folder_id(folder_id);
  if (!file_id.empty())
    info.mutable_box()->set_file_id(file_id);
  return info;
}

class BoxUploader_FromRerouteInfoTestBase : public BoxUploaderTestBase {
 public:
  BoxUploader_FromRerouteInfoTestBase()
      : BoxUploaderTestBase(
            FILE_PATH_LITERAL("box_uploader_from_reroute_info_test.txt")) {}

 protected:
  void TearDown() override {
    EXPECT_EQ(authentication_retry_, 0);
    EXPECT_EQ(progress_update_cb_called_ > 0, download_thread_cb_called_);
    EXPECT_FALSE(test_item_.GetFileExternallyRemoved());
    // Ended with no local temporary file.
    ASSERT_FALSE(base::PathExists(GetFilePath()));
  }

  std::unique_ptr<BoxUploader> uploader_;
};

class BoxUploader_IncompleteItemFromRerouteInfoTest
    : public BoxUploader_FromRerouteInfoTestBase,
      public testing::WithParamInterface<State> {};

TEST_P(BoxUploader_IncompleteItemFromRerouteInfoTest, RetryUpload) {
  // Skip COMPLETE to allow Range() to generate all other enum values.
  if (GetParam() == State::COMPLETE)
    return SUCCEED();

  CreateTemporaryFile();

  test_item_.SetState(GetParam());
  DownloadItemRerouteInfo reroute_info = MakeDownloadItemRerouteInfo();
  test_item_.SetRerouteInfo(reroute_info);
  uploader_ = BoxUploader::Create(&test_item_);
  ASSERT_TRUE(uploader_);

  // Assume there is already a folder_id stored in prefs, preflight check
  // passes, and upload request succeeds.
  InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);
  AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
  AddFetchResult(kFileSystemBoxDirectUploadUrl, net::HTTP_CREATED,
                 kFileSystemBoxUploadResponseBody);

  // Try uploading.
  InitUploader(uploader_.get());
  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(reroute_info_reported_back_.box().file_id(),
            kFileSystemBoxUploadResponseFileId);
  ASSERT_EQ(reroute_info_reported_back_.box().folder_id(),
            kFileSystemBoxFolderIdInPref);
  ASSERT_EQ(uploader_->GetUploadedFileUrl(),
            GURL(kFileSystemBoxUploadResponseFileUrl));
  ASSERT_EQ(uploader_->GetDestinationFolderUrl(),
            GURL(kFileSystemBoxFolderIdInPrefUrl));

  ASSERT_FALSE(authentication_retry_);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_GE(progress_update_cb_called_, 2);
  ASSERT_TRUE(upload_success_);
}

INSTANTIATE_TEST_SUITE_P(,
                         BoxUploader_IncompleteItemFromRerouteInfoTest,
                         testing::Range(State::IN_PROGRESS,
                                        State::MAX_DOWNLOAD_STATE));

class BoxUploader_CompletedItemFromRerouteInfoTest
    : public BoxUploader_FromRerouteInfoTestBase {};

TEST_F(BoxUploader_CompletedItemFromRerouteInfoTest, Normal) {
  test_item_.SetState(State::COMPLETE);
  const std::string folder_id = "357321";
  const std::string file_id = "13576123";
  DownloadItemRerouteInfo reroute_info =
      MakeDownloadItemRerouteInfo(folder_id, file_id);
  test_item_.SetRerouteInfo(reroute_info);

  uploader_ = BoxUploader::Create(&test_item_);
  ASSERT_TRUE(uploader_);
  ASSERT_EQ(uploader_->GetUploadedFileUrl(),
            BoxApiCallFlow::MakeUrlToShowFile(file_id));
  ASSERT_EQ(uploader_->GetDestinationFolderUrl(),
            BoxApiCallFlow::MakeUrlToShowFolder(folder_id));

  EXPECT_FALSE(download_thread_cb_called_);  // No upload call was made.
  EXPECT_FALSE(progress_update_cb_called_);
}

////////////////////////////////////////////////////////////////////////////////
// BoxDirectUploaderTest
////////////////////////////////////////////////////////////////////////////////

class BoxDirectUploaderTest : public BoxUploaderTestBase {
 public:
  BoxDirectUploaderTest()
      : BoxUploaderTestBase(FILE_PATH_LITERAL("box_direct_uploader_test.txt")),
        uploader_(std::make_unique<BoxDirectUploader>(&test_item_)) {}

  void SetUp() override {
    BoxUploaderTestBase::SetUp();

    // Assume there is already a folder_id stored in prefs and preflight check
    // passes; will skip directly to upload.
    InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);
    AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);

    InitUploader(uploader_.get());
    ASSERT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);
  }

  void TearDown() override {
    EXPECT_FALSE(base::PathExists(GetFilePath()));  // Ensure file is deleted.
  }

  std::unique_ptr<BoxDirectUploader> uploader_;
};

TEST_F(BoxDirectUploaderTest, SuccessfulUpload) {
  AddFetchResult(kFileSystemBoxDirectUploadUrl, net::HTTP_CREATED,
                 kFileSystemBoxUploadResponseBody);

  CreateTemporaryFile();
  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_TRUE(progress_update_cb_called_);
  EXPECT_TRUE(upload_success_);
  EXPECT_EQ(uploader_->GetUploadedFileUrl(),
            kFileSystemBoxUploadResponseFileUrl);
  EXPECT_EQ(uploader_->GetDestinationFolderUrl(),
            kFileSystemBoxFolderIdInPrefUrl);
}

TEST_F(BoxDirectUploaderTest, FileReadFailure) {
  // Do not CreateTemporaryFile() so that file read fails.
  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_TRUE(progress_update_cb_called_);
  EXPECT_FALSE(upload_success_);
  EXPECT_TRUE(uploader_->GetUploadedFileUrl().is_empty());
  EXPECT_EQ(uploader_->GetDestinationFolderUrl(),
            kFileSystemBoxFolderIdInPrefUrl);
  ASSERT_REASON_EQ(FILE_FAILED, reason_);
}

TEST_F(BoxDirectUploaderTest, UnexpectedFailure) {
  // Check that the API calls flow is terminated upon any other failure code
  // other than net::HTTP_UNAUTHORIZED.
  AddFetchResult(kFileSystemBoxDirectUploadUrl, net::HTTP_NOT_FOUND);

  CreateTemporaryFile();
  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  // Should just report failure via callback.
  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);
  EXPECT_FALSE(upload_success_);
}

////////////////////////////////////////////////////////////////////////////////
// BoxChunkedUploaderTest
////////////////////////////////////////////////////////////////////////////////

class BoxChunkedUploaderTest : public BoxUploaderTestBase {
 public:
  BoxChunkedUploaderTest()
      : BoxUploaderTestBase(
            FILE_PATH_LITERAL("box_chunked_uploader_test.txt")) {}

  void SetUp() override {
    testing::Test::SetUp();
    // Assume there is already a folder_id stored.
    InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);

    CreateTemporaryFile();
    uploader_ = BoxUploader::Create(&test_item_);
    InitUploader(uploader_.get());

    // Assume preflight check passes, which is tested thoroughly separately.
    AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
  }

  void TearDown() override {
    EXPECT_TRUE(download_thread_cb_called_);
    EXPECT_FALSE(base::PathExists(GetFilePath()));  // Ensure file is deleted.
  }

  void CreateTemporaryFile() override {
    std::string content;
    GenerateFileContent(BoxApiCallFlow::kChunkFileUploadMinSize / 10,
                        BoxApiCallFlow::kChunkFileUploadMinSize * 2, content);
    CreateTemporaryFileWithContent(content);
  }

  void AddUploadSuccessFetchResultsForPart(size_t idx,
                                           net::HttpStatusCode response_code,
                                           size_t chunk_size,
                                           size_t expected_chunks,
                                           size_t total_size) {
    const size_t curr_chunk_size =
        (idx + 1 == expected_chunks) ? (total_size % chunk_size) : chunk_size;
    std::string body =
        CreateChunkedUploadPartResponse(chunk_size * idx, curr_chunk_size);
    AddSequentialFetchResult(kFileSystemBoxChunkedUploadSessionUrl,
                             response_code, body);
  }

  // Helper method to add upload fetch results sequentially. |last_nth| = 0
  // means no failure; otherwise, a failure http code must be provided.
  void AddUploadFetchResultsWithFailureAtLastNth(
      size_t last_nth,
      net::HttpStatusCode failure_http_code = net::HTTP_OK,
      std::string error_msg = std::string()) {
    const size_t chunk_size =
        kFileSystemBoxChunkedUploadCreateSessionResponsePartSize;
    const size_t total_size = test_item_.GetTotalBytes();
    const size_t chunks_count =
        CalculateExpectedChunkReadCount(total_size, chunk_size);
    ASSERT_GT(chunks_count, 1U);
    ASSERT_LE(total_size, chunk_size * chunks_count);

    size_t last_chunk = chunks_count;
    if (last_nth) {
      ASSERT_NE(failure_http_code, net::HTTP_OK)
          << "Must provide a failure http code";
      last_chunk = last_chunk - last_nth;
      ASSERT_GT(last_chunk, 0U);
      LOG(INFO) << "First " << last_chunk << " parts will succeed; total "
                << chunks_count << " parts; total file size " << total_size;
    }
    ASSERT_LE(last_chunk, chunks_count);

    for (size_t pdx = 0; pdx < last_chunk; ++pdx) {
      AddUploadSuccessFetchResultsForPart(pdx, net::HTTP_OK, chunk_size,
                                          chunks_count, total_size);
    }
    ASSERT_EQ(GetPendingSequentialResponsesCount(
                  kFileSystemBoxChunkedUploadSessionUrl),
              last_chunk);

    if (last_nth) {
      LOG(INFO) << "Failing part " << last_chunk << "th of " << chunks_count;
      AddSequentialFetchResult(
          kFileSystemBoxChunkedUploadSessionUrl, failure_http_code,
          CreateFailureResponse(failure_http_code, error_msg.c_str()));
    }
  }

  void AddUploadSuccessFetchResults() {
    AddUploadFetchResultsWithFailureAtLastNth(0);
  }

  std::unique_ptr<BoxUploader> uploader_;
};

TEST_F(BoxChunkedUploaderTest, SuccessfulUpload) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload parts:
  AddUploadSuccessFetchResults();
  // Commit upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCommitUrl, net::HTTP_CREATED,
                 kFileSystemBoxUploadResponseBody);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(progress_update_cb_called_);
  ASSERT_TRUE(upload_success_);
}

TEST_F(BoxChunkedUploaderTest, FailedToCreateSession) {
  // Create upload session (empty body since not reading from body):
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl,
                 net::HTTP_CONFLICT);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  ASSERT_FALSE(upload_success_);
}

TEST_F(BoxChunkedUploaderTest, HasFolderIdStoredInPrefs_ButFailedOnBox) {
  // Create upload session (empty body since not reading from body):
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl,
                 net::HTTP_NOT_FOUND);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  EXPECT_TRUE(uploader_->GetFolderIdForTesting().empty());

  ASSERT_EQ(authentication_retry_, 0);
  ASSERT_FALSE(upload_success_);
}

TEST_F(BoxChunkedUploaderTest, AuthenticationRetry_DuringCreateSession) {
  // Create upload session failed (empty body since not reading from body):
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl,
                 net::HTTP_UNAUTHORIZED);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_FALSE(upload_success_);

  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload parts:
  AddUploadSuccessFetchResults();
  // Commit upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCommitUrl, net::HTTP_CREATED,
                 kFileSystemBoxUploadResponseBody);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_TRUE(upload_success_);
}

TEST_F(BoxChunkedUploaderTest, AuthenticationRetry_DuringUploadPart) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload part failed (empty body since not reading from body):
  AddSequentialFetchResult(kFileSystemBoxChunkedUploadSessionUrl,
                           net::HTTP_UNAUTHORIZED);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_FALSE(upload_success_);

  // Clear mock responses above to ensure they are not called again:
  ClearFetchResults(kFileSystemBoxChunkedUploadCreateSessionUrl);

  // Upload parts:
  AddUploadSuccessFetchResults();
  // Commit upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCommitUrl, net::HTTP_CREATED,
                 kFileSystemBoxUploadResponseBody);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_TRUE(upload_success_);
}

TEST_F(BoxChunkedUploaderTest, AuthenticationRetry_DuringCommitSession) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload parts:
  AddUploadSuccessFetchResults();
  // Commit upload session failed (empty body since not reading from body):
  AddFetchResult(kFileSystemBoxChunkedUploadCommitUrl, net::HTTP_UNAUTHORIZED);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_FALSE(upload_success_);

  // Clear mock responses above to ensure they are not called again:
  ClearFetchResults(kFileSystemBoxChunkedUploadCreateSessionUrl);
  ClearFetchResults(kFileSystemBoxChunkedUploadCommitUrl);

  // Commit upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCommitUrl, net::HTTP_CREATED,
                 kFileSystemBoxUploadResponseBody);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 1);
  ASSERT_TRUE(upload_success_);
}

TEST_F(BoxChunkedUploaderTest, FailedToUploadPart) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload part 1 succeeded, but upload part 2 failed:
  AddUploadFetchResultsWithFailureAtLastNth(1, net::HTTP_PRECONDITION_FAILED);
  // Abort upload session (empty body since not reading from body):
  AddSequentialFetchResult(kFileSystemBoxChunkedUploadSessionUrl,
                           net::HTTP_NO_CONTENT);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  ASSERT_FALSE(upload_success_);
  ASSERT_REASON_EQ(SERVER_FAILED, reason_);
}

TEST_F(BoxChunkedUploaderTest, FailedToUploadPartThenFailedToAbortSession) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload part 1 succeeded, but upload part 2 failed:
  AddUploadFetchResultsWithFailureAtLastNth(1, net::HTTP_PRECONDITION_FAILED);
  // Abort upload session failed:
  AddSequentialFetchResult(
      kFileSystemBoxChunkedUploadSessionUrl, net::HTTP_GONE,
      CreateFailureResponse(net::HTTP_GONE, "session_expired"));

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  ASSERT_FALSE(upload_success_);
  ASSERT_REASON_EQ(SERVER_FAILED, reason_);
}

TEST_F(BoxChunkedUploaderTest, FailedToCommitSession) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload parts:
  AddUploadSuccessFetchResults();
  // Commit upload session failed:
  AddSequentialFetchResult(
      kFileSystemBoxChunkedUploadCommitUrl, net::HTTP_BAD_REQUEST,
      CreateFailureResponse(net::HTTP_BAD_REQUEST, "bad_digest"));
  // Abort upload session (empty body since not reading from body):
  AddSequentialFetchResult(kFileSystemBoxChunkedUploadSessionUrl,
                           net::HTTP_NO_CONTENT);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  ASSERT_FALSE(upload_success_);
  ASSERT_REASON_EQ(SERVER_FAILED, reason_);
}

TEST_F(BoxChunkedUploaderTest, CommitRetryAfter) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Upload parts:
  AddUploadSuccessFetchResults();

  // Mock a Retry-After header in response to commit upload session:
  auto header = network::CreateURLResponseHead(net::HTTP_ACCEPTED);
  header->headers->AddHeader("Retry-After", "1");
  AddSequentialFetchResult(kFileSystemBoxChunkedUploadCommitUrl,
                           std::move(header));
  // Commit upload session succeeded after retry:
  AddSequentialFetchResult(kFileSystemBoxChunkedUploadCommitUrl,
                           net::HTTP_CREATED, kFileSystemBoxUploadResponseBody);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  ASSERT_TRUE(upload_success_);
}

class BoxChunkedUploaderFileFailureTest : public BoxChunkedUploaderTest {
 public:
  using BoxChunkedUploaderTest::BoxChunkedUploaderTest;

  void SetUp() override {
    testing::Test::SetUp();
    // Assume there is already a folder_id stored.
    InitFolderIdInPrefs(kFileSystemBoxFolderIdInPref);

    // No CreateTemporaryFile() to fail file open, but mock file size to create
    // the correct uploader.
    test_item_.SetTotalBytes(BoxApiCallFlow::kChunkFileUploadMinSize * 2);

    uploader_ = BoxUploader::Create(&test_item_);
    InitUploader(uploader_.get());
    ASSERT_EQ(uploader_->GetFolderIdForTesting(), kFileSystemBoxFolderIdInPref);

    // Assume preflight check passes, which is tested thoroughly separately.
    AddFetchResult(kFileSystemBoxPreflightCheckUrl, net::HTTP_OK);
  }

  void TearDown() override {
    EXPECT_FALSE(base::PathExists(GetFilePath())) << "No file should exist";
  }
};

TEST_F(BoxChunkedUploaderFileFailureTest, FailedToOpen) {
  // Create upload session:
  AddFetchResult(kFileSystemBoxChunkedUploadCreateSessionUrl, net::HTTP_CREATED,
                 kFileSystemBoxChunkedUploadCreateSessionResponseBody);
  // Abort upload session (empty body since not reading from body):
  AddSequentialFetchResult(kFileSystemBoxChunkedUploadSessionUrl,
                           net::HTTP_NO_CONTENT);

  InitQuitClosure();
  uploader_->TryTask(url_factory_, "test_token");
  RunWithQuitClosure();

  ASSERT_EQ(authentication_retry_, 0);
  EXPECT_TRUE(download_thread_cb_called_);
  ASSERT_FALSE(upload_success_);
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploaderForUma: DownloadsRouting UMA histograms test
////////////////////////////////////////////////////////////////////////////////

class BoxUploaderForUmaTest : public BoxUploader {
 public:
  // using BoxUploader::BoxUploader() doesn't work.
  explicit BoxUploaderForUmaTest(download::DownloadItem* download_item)
      : BoxUploader(download_item) {}

  using BoxUploader::OnApiCallFlowDone;

  // Overriding to skip API calls flow for this set of tests.
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override {
    return std::make_unique<MockApiCallFlow>();
  }
};

class BoxUploader_UmaTest : public BoxUploaderTestBase {
 public:
  BoxUploader_UmaTest()
      : BoxUploaderTestBase(FILE_PATH_LITERAL("box_uploader_uma_test.txt")),
        uploader_(std::make_unique<BoxUploaderForUmaTest>(&test_item_)) {}

 protected:
  void SetUp() override {
    BoxUploaderTestBase::SetUp();
    InitUploader(uploader_.get());
  }

  void TearDown() override {
    EXPECT_EQ(authentication_retry_, 0);
    EXPECT_EQ(progress_update_cb_called_ > 0, download_thread_cb_called_);
    EXPECT_FALSE(test_item_.GetFileExternallyRemoved());
  }

  std::unique_ptr<BoxUploaderForUmaTest> uploader_;
};

TEST_F(BoxUploader_UmaTest, Success) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(download::DOWNLOAD_INTERRUPT_REASON_NONE,
                               kFileSystemBoxUploadResponseFileId);
  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_SUCCEEDED, 1);
}

TEST_F(BoxUploader_UmaTest, BoxError) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH,
      kFileSystemBoxUploadResponseFileId);
  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::
          ROUTING_FAILED_SERVICE_PROVIDER_ERROR,
      3);
}

TEST_F(BoxUploader_UmaTest, BoxOutage) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
      kFileSystemBoxUploadResponseFileId);
  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::
          ROUTING_FAILED_SERVICE_PROVIDER_OUTAGE,
      1);
}

TEST_F(BoxUploader_UmaTest, BrowserError) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(download::DOWNLOAD_INTERRUPT_REASON_CRASH,
                               kFileSystemBoxUploadResponseFileId);
  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_FAILED_BROWSER_ERROR,
      1);
}

TEST_F(BoxUploader_UmaTest, Canceled) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED,
      kFileSystemBoxUploadResponseFileId);
  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_CANCELED, 1);
}

TEST_F(BoxUploader_UmaTest, FileError) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                               kFileSystemBoxUploadResponseFileId);

  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_FAILED_FILE_ERROR, 4);
}

TEST_F(BoxUploader_UmaTest, NetError) {
  base::HistogramTester histogram_tester;
  InitQuitClosure();
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
      kFileSystemBoxUploadResponseFileId);
  uploader_->OnApiCallFlowDone(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
      kFileSystemBoxUploadResponseFileId);

  RunWithQuitClosure();

  histogram_tester.ExpectUniqueSample(
      kBoxDownloadsRoutingStatusUmaLabel,
      EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_FAILED_NETWORK_ERROR,
      3);
}

}  // namespace enterprise_connectors
