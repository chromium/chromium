// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {

namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;

// Test class for `SaveToDriveRecorder`.
class SaveToDriveRecorderTest : public testing::Test {
 public:
  SaveToDriveRecorderTest()
      : profile_(IdentityTestEnvironmentProfileAdaptor::
                     CreateProfileForIdentityTestEnvironment()),
        identity_test_env_profile_adaptor_(
            std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
                profile_.get())),
        recorder_(profile_.get()) {}
  ~SaveToDriveRecorderTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  SaveToDriveRecorder recorder_;

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }
};

TEST_F(SaveToDriveRecorderTest, RecordsStatusAndLatencyOnCompleted) {
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadStarted;
  recorder_.Record(progress);

  task_environment_.FastForwardBy(base::Seconds(2));
  progress.status = SaveToDriveStatus::kUploadCompleted;
  progress.file_size_bytes = 2048;
  progress.account_email = "test@example.com";
  recorder_.Record(progress);

  histogram_tester_.ExpectUniqueTimeSample("PDF.SaveToDrive.UploadLatency",
                                           base::Seconds(2), 1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kUploadStarted, 1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kUploadCompleted, 1);
  histogram_tester_.ExpectUniqueSample("PDF.SaveToDrive.FileSize", 2, 1);
}

TEST_F(SaveToDriveRecorderTest, RecordsStatusAndErrorOnFailed) {
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadStarted;
  recorder_.Record(progress);

  progress.status = SaveToDriveStatus::kUploadFailed;
  progress.error_type = SaveToDriveErrorType::kOffline;
  recorder_.Record(progress);

  histogram_tester_.ExpectTotalCount("PDF.SaveToDrive.UploadLatency", 0);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kUploadStarted, 1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kUploadFailed, 1);
  histogram_tester_.ExpectUniqueSample("PDF.SaveToDrive.UploadError",
                                       SaveToDriveErrorType::kOffline, 1);
}

TEST_F(SaveToDriveRecorderTest, DoesNotRecordLatencyWithoutStart) {
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadCompleted;
  progress.file_size_bytes = 2048;
  progress.account_email = "test@example.com";
  recorder_.Record(progress);

  histogram_tester_.ExpectTotalCount("PDF.SaveToDrive.UploadLatency", 0);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kUploadCompleted, 1);
}

TEST_F(SaveToDriveRecorderTest, RecordsIntermediateStates) {
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kInitiated;
  recorder_.Record(progress);
  progress.status = SaveToDriveStatus::kAccountChooserShown;
  recorder_.Record(progress);
  progress.status = SaveToDriveStatus::kAccountSelected;
  recorder_.Record(progress);
  progress.status = SaveToDriveStatus::kFetchOauth;
  recorder_.Record(progress);
  progress.status = SaveToDriveStatus::kFetchParentFolder;
  recorder_.Record(progress);

  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kInitiated, 1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kAccountChooserShown,
                                      1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kAccountSelected, 1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kFetchOauth, 1);
  histogram_tester_.ExpectBucketCount("PDF.SaveToDrive.UploadStatus",
                                      SaveToDriveStatus::kFetchParentFolder, 1);
}

TEST_F(SaveToDriveRecorderTest,
       RecordsPrimaryAccountSelectedWhenPrimaryAccountExists) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadCompleted;
  progress.file_size_bytes = 1024;
  progress.account_email = "test@example.com";
  recorder_.Record(progress);
  histogram_tester_.ExpectUniqueSample(
      "PDF.SaveToDrive.PrimaryAccountSelectionStatus", 1, 1);
}

TEST_F(SaveToDriveRecorderTest,
       RecordsNonPrimaryAccountSelectedWhenPrimaryAccountExists) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadCompleted;
  progress.file_size_bytes = 1024;
  progress.account_email = "other@example.com";
  recorder_.Record(progress);
  histogram_tester_.ExpectUniqueSample(
      "PDF.SaveToDrive.PrimaryAccountSelectionStatus", 2, 1);
}

TEST_F(SaveToDriveRecorderTest,
       RecordsNonPrimaryAccountSelectedWhenNoPrimaryAccount) {
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadCompleted;
  progress.file_size_bytes = 1024;
  progress.account_email = "test@example.com";
  recorder_.Record(progress);
  histogram_tester_.ExpectUniqueSample(
      "PDF.SaveToDrive.PrimaryAccountSelectionStatus", 3, 1);
}

}  // namespace

}  // namespace save_to_drive
