// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc_event_logging {

using ::testing::_;
using ::testing::StrictMock;
using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;

namespace {
class UploadObserver {
 public:
  explicit UploadObserver(base::OnceClosure on_complete_callback)
      : on_complete_callback_(std::move(on_complete_callback)) {}

  // Combines the mock functionality via a helper (CompletionCallback),
  // as well as unblocks its owner through |on_complete_callback_|.
  void OnWebRtcEventLogUploadComplete(const base::FilePath& log_file,
                                      bool upload_successful) {
    CompletionCallback(log_file, upload_successful);
    std::move(on_complete_callback_).Run();
  }

  MOCK_METHOD2(CompletionCallback, void(const base::FilePath&, bool));

 private:
  base::OnceClosure on_complete_callback_;
};

#if BUILDFLAG(IS_POSIX)
void RemovePermissions(const base::FilePath& path, int removed_permissions) {
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &permissions));
  permissions &= ~removed_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(path, permissions));
}

void RemoveReadPermissions(const base::FilePath& path) {
  constexpr int read_permissions = base::FILE_PERMISSION_READ_BY_USER |
                                   base::FILE_PERMISSION_READ_BY_GROUP |
                                   base::FILE_PERMISSION_READ_BY_OTHERS;
  RemovePermissions(path, read_permissions);
}
#endif  // BUILDFLAG(IS_POSIX)
}  // namespace

class WebRtcEventLogUploaderImplTest : public ::testing::Test {
 public:
  WebRtcEventLogUploaderImplTest()
      : test_shared_url_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()),
        observer_run_loop_(),
        observer_(observer_run_loop_.QuitWhenIdleClosure()) {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_url_loader_factory_);

    EXPECT_TRUE(base::Time::FromString("30 Dec 1983", &kReasonableTime));

    uploader_factory_ = std::make_unique<WebRtcEventLogUploaderImpl::Factory>(
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  ~WebRtcEventLogUploaderImplTest() override {
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profiles_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(testing_profile_manager_->SetUp(profiles_dir_.GetPath()));

    testing_profile_ =
        testing_profile_manager_->CreateTestingProfile("arbitrary_name");

    browser_context_id_ = GetBrowserContextId(testing_profile_);

    // Create the sub-dir for the remote-bound logs that would have been set
    // up by WebRtcEventLogManager, if WebRtcEventLogManager were instantiated.
    // Note that the testing profile's overall directory is a temporary one.
    const base::FilePath logs_dir =
        GetRemoteBoundWebRtcEventLogsDir(testing_profile_->GetPath());
    ASSERT_TRUE(base::CreateDirectory(logs_dir));

    // Create a log file and put some arbitrary data in it.
    // Note that the testing profile's overall directory is a temporary one.
    ASSERT_TRUE(base::CreateTemporaryFileInDir(logs_dir, &log_file_));
    constexpr size_t kLogFileSizeBytes = 100u;
    const std::string file_contents(kLogFileSizeBytes, 'A');
    ASSERT_TRUE(base::WriteFile(log_file_, file_contents));
  }

  // For tests which imitate a response (or several).
  void SetURLLoaderResponse(net::HttpStatusCode http_code, int net_error) {
    DCHECK(test_shared_url_loader_factory_);
    const std::string kResponseId = "ec1ed029734b8f7e";  // Arbitrary.
    test_url_loader_factory_.AddResponse(
        GURL(WebRtcEventLogUploaderImpl::kUploadURL),
        network::CreateURLResponseHead(http_code), kResponseId,
        network::URLLoaderCompletionStatus(net_error));
  }

  void StartAndWaitForUpload(
      BrowserContextId browser_context_id = BrowserContextId(),
      base::Time last_modified_time = base::Time()) {
    DCHECK(test_shared_url_loader_factory_);

    if (last_modified_time.is_null()) {
      last_modified_time = kReasonableTime;
    }

    const WebRtcLogFileInfo log_file_info(browser_context_id, log_file_,
                                          last_modified_time);

    uploader_ = uploader_factory_->Create(log_file_info, ResultCallback());

    observer_run_loop_.Run();  // Observer was given quit-closure by ctor.
  }

  void StartAndWaitForUploadWithCustomMaxSize(
      size_t max_log_size_bytes,
      BrowserContextId browser_context_id = BrowserContextId(),
      base::Time last_modified_time = base::Time()) {
    DCHECK(test_shared_url_loader_factory_);

    if (last_modified_time.is_null()) {
      last_modified_time = kReasonableTime;
    }

    const WebRtcLogFileInfo log_file_info(browser_context_id, log_file_,
                                          last_modified_time);

    uploader_ = uploader_factory_->CreateWithCustomMaxSizeForTesting(
        log_file_info, ResultCallback(), max_log_size_bytes);

    observer_run_loop_.Run();  // Observer was given quit-closure by ctor.
  }

  void StartUploadThatWillNotTerminate(
      BrowserContextId browser_context_id = BrowserContextId(),
      base::Time last_modified_time = base::Time()) {
    DCHECK(test_shared_url_loader_factory_);

    if (last_modified_time.is_null()) {
      last_modified_time = kReasonableTime;
    }

    const WebRtcLogFileInfo log_file_info(browser_context_id, log_file_,
                                          last_modified_time);

    uploader_ = uploader_factory_->Create(log_file_info, ResultCallback());
  }

  WebRtcEventLogUploader::UploadResultCallback ResultCallback() {
    return base::BindOnce(&UploadObserver::OnWebRtcEventLogUploadComplete,
                          base::Unretained(&observer_));
  }

  content::BrowserTaskEnvironment task_environment_;

  base::Time kReasonableTime;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  base::RunLoop observer_run_loop_;

  base::ScopedTempDir profiles_dir_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> testing_profile_;  // |testing_profile_manager_| owns.
  BrowserContextId browser_context_id_;

  base::FilePath log_file_;

  StrictMock<UploadObserver> observer_;

  // These (uploader-factory and uploader) are the units under test.
  std::unique_ptr<WebRtcEventLogUploaderImpl::Factory> uploader_factory_;
  std::unique_ptr<WebRtcEventLogUploader> uploader_;
};

TEST_F(WebRtcEventLogUploaderImplTest, SuccessfulUploadReportedToObserver) {
  SetURLLoaderResponse(net::HTTP_OK, net::OK);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUpload();
  EXPECT_FALSE(base::PathExists(log_file_));
}

// Version #1 - request reported as successful, but got an error (404) as the
// HTTP return code.
// Due to the simplicitly of both tests, this also tests the scenario
// FileDeletedAfterUnsuccessfulUpload, rather than giving each its own test.
TEST_F(WebRtcEventLogUploaderImplTest, UnsuccessfulUploadReportedToObserver1) {
  SetURLLoaderResponse(net::HTTP_NOT_FOUND, net::OK);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
  EXPECT_FALSE(base::PathExists(log_file_));
}

// Version #2 - request reported as failed; HTTP return code ignored, even
// if it's a purported success.
TEST_F(WebRtcEventLogUploaderImplTest, UnsuccessfulUploadReportedToObserver2) {
  SetURLLoaderResponse(net::HTTP_NOT_FOUND, net::ERR_FAILED);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
  EXPECT_FALSE(base::PathExists(log_file_));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(WebRtcEventLogUploaderImplTest, FailureToReadFileReportedToObserver) {
  // Show the failure was independent of the URLLoaderFactory's primed return
  // value.
  SetURLLoaderResponse(net::HTTP_OK, net::OK);

  RemoveReadPermissions(log_file_);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
}

TEST_F(WebRtcEventLogUploaderImplTest, NonExistentFileReportedToObserver) {
  // Show the failure was independent of the URLLoaderFactory's primed return
  // value.
  SetURLLoaderResponse(net::HTTP_OK, net::OK);

  log_file_ = log_file_.Append(FILE_PATH_LITERAL("garbage"));
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(WebRtcEventLogUploaderImplTest, FilesUpToMaxSizeUploaded) {
  int64_t log_file_size_bytes;
  ASSERT_TRUE(base::GetFileSize(log_file_, &log_file_size_bytes));

  SetURLLoaderResponse(net::HTTP_OK, net::OK);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUploadWithCustomMaxSize(log_file_size_bytes);
  EXPECT_FALSE(base::PathExists(log_file_));
}

TEST_F(WebRtcEventLogUploaderImplTest, ExcessivelyLargeFilesNotUploaded) {
  int64_t log_file_size_bytes;
  ASSERT_TRUE(base::GetFileSize(log_file_, &log_file_size_bytes));

  SetURLLoaderResponse(net::HTTP_OK, net::OK);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUploadWithCustomMaxSize(log_file_size_bytes - 1);
  EXPECT_FALSE(base::PathExists(log_file_));
}

TEST_F(WebRtcEventLogUploaderImplTest,
       CancelBeforeUploadCompletionCallsCallbackWithFalse) {
  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  uploader_->Cancel();
}

TEST_F(WebRtcEventLogUploaderImplTest, SecondCallToCancelHasNoEffect) {
  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);

  EXPECT_CALL(observer_, CompletionCallback(log_file_, _)).Times(1);

  uploader_->Cancel();
  uploader_->Cancel();
}

TEST_F(WebRtcEventLogUploaderImplTest,
       CancelAfterUploadCompletionCallbackWasCalledHasNoEffect) {
  SetURLLoaderResponse(net::HTTP_OK, net::OK);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);
  StartAndWaitForUpload();

  EXPECT_CALL(observer_, CompletionCallback(_, _)).Times(0);
  uploader_->Cancel();
}

TEST_F(WebRtcEventLogUploaderImplTest, CancelOnAbortedUploadHasNoEffect) {
  // Show the failure was independent of the URLLoaderFactory's primed return
  // value.
  SetURLLoaderResponse(net::HTTP_OK, net::OK);

  log_file_ = log_file_.Append(FILE_PATH_LITERAL("garbage"));
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  StartAndWaitForUpload();

  EXPECT_CALL(observer_, CompletionCallback(_, _)).Times(0);
  uploader_->Cancel();
}

TEST_F(WebRtcEventLogUploaderImplTest, CancelOnOngoingUploadDeletesFile) {
  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);

  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  uploader_->Cancel();
  observer_run_loop_.Run();

  EXPECT_FALSE(base::PathExists(log_file_));
}

TEST_F(WebRtcEventLogUploaderImplTest,
       GetWebRtcLogFileInfoReturnsCorrectInfoBeforeUploadDone) {
  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);

  const WebRtcLogFileInfo info = uploader_->GetWebRtcLogFileInfo();
  EXPECT_EQ(info.browser_context_id, browser_context_id_);
  EXPECT_EQ(info.path, log_file_);
  EXPECT_EQ(info.last_modified, last_modified);

  // Test tear-down.
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  uploader_->Cancel();
}

TEST_F(WebRtcEventLogUploaderImplTest,
       GetWebRtcLogFileInfoReturnsCorrectInfoAfterUploadSucceeded) {
  SetURLLoaderResponse(net::HTTP_OK, net::OK);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, true)).Times(1);

  const base::Time last_modified = base::Time::Now();
  StartAndWaitForUpload(browser_context_id_, last_modified);

  const WebRtcLogFileInfo info = uploader_->GetWebRtcLogFileInfo();
  EXPECT_EQ(info.browser_context_id, browser_context_id_);
  EXPECT_EQ(info.path, log_file_);
  EXPECT_EQ(info.last_modified, last_modified);
}

TEST_F(WebRtcEventLogUploaderImplTest,
       GetWebRtcLogFileInfoReturnsCorrectInfoWhenCalledOnCancelledUpload) {
  const base::Time last_modified = base::Time::Now();
  StartUploadThatWillNotTerminate(browser_context_id_, last_modified);
  EXPECT_CALL(observer_, CompletionCallback(log_file_, false)).Times(1);
  uploader_->Cancel();

  const WebRtcLogFileInfo info = uploader_->GetWebRtcLogFileInfo();
  EXPECT_EQ(info.browser_context_id, browser_context_id_);
  EXPECT_EQ(info.path, log_file_);
  EXPECT_EQ(info.last_modified, last_modified);
}

// TODO(crbug.com/40545136): Add a unit test that shows that files with
// non-ASCII filenames are discard. (Or, alternatively, add support for them.)

}  // namespace webrtc_event_logging
