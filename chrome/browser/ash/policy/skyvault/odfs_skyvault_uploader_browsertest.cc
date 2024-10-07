// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"
#include "chrome/browser/ash/policy/skyvault/skyvault_test_base.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/message_center/public/cpp/notification.h"

using policy::local_user_files::SkyvaultOneDriveTest;
using policy::local_user_files::UploadTrigger;

namespace ash::cloud_upload {

// Tests the OneDrive upload workflow using the static
// `OdfsSkyvaultUploader::Upload` method. Ensures that the upload completes
// with the expected results.
class OdfsSkyvaultUploaderTest : public SkyvaultOneDriveTest {
 public:
  OdfsSkyvaultUploaderTest() = default;

  OdfsSkyvaultUploaderTest(const OdfsSkyvaultUploaderTest&) = delete;
  OdfsSkyvaultUploaderTest& operator=(const OdfsSkyvaultUploaderTest&) = delete;

  void SetUpOnMainThread() override {
    SkyvaultOneDriveTest::SetUpOnMainThread();

    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

 protected:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  base::HistogramTester histogram_tester_;

  // Used to observe skyvault notifications during tests.
  base::RepeatingCallback<void(const message_center::Notification&)>
      on_notification_displayed_callback_;
};

IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest, SuccessfulUpload) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and end the test once the upload callback is run.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  EXPECT_CALL(progress_callback, Run(/*bytes_transferred=*/230096));
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  EXPECT_EQ(upload_callback.Get<bool>(), true);

  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest, SuccessfulUploadWithTarget) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());
  const std::string target_path = "ChromeOS Device";

  // Start the upload workflow and end the test once the upload callback is run.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<
      storage::FileSystemURL,
      std::optional<policy::local_user_files::MigrationUploadError>>
      upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kMigration,
      progress_callback.Get(), upload_callback.GetCallback(),
      base::FilePath(target_path));

  auto [url, error] = upload_callback.Get();
  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(url.is_valid());
  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(
      base::FilePath("/").AppendASCII(target_path).AppendASCII(test_file_name));
}

IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest, CancelledUpload) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and cancel the upload immediately.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  base::WeakPtr<OdfsSkyvaultUploader> uploader = OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  uploader->Cancel();
  EXPECT_EQ(upload_callback.Get<bool>(), false);

  // Check that the source file has not been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest, FailToUploadDueToMemoryError) {
  SetUpMyFiles();
  SetUpODFS();
  // Ensure Upload fails due to memory error and that reauthentication to
  // OneDrive is not required.
  provided_file_system_->SetCreateFileError(
      base::File::Error::FILE_ERROR_NO_MEMORY);
  provided_file_system_->SetReauthenticationRequired(false);
  const std::string test_file_name = "id3Audio.mp3";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and end the test once the upload callback is run.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  EXPECT_EQ(upload_callback.Get<bool>(), false);

  // Check that the source file has not been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

// Test that when the reauthentication to ODFS is required, the sign-in required
// notification is shown. When the sign-in is complete, the upload is continued.
IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest,
                       UploadAfterReauthenticationRequired) {
  SetUpMyFiles();
  SetUpODFS();
  provided_file_system_->SetReauthenticationRequired(true);
  const std::string test_file_name = "text.docx";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and simulate a successful mount() request
  // (indicating interactive auth has succeeded).
  file_manager::test::GetFakeProviderOneDrive(profile())->SetRequestMountImpl(
      base::BindLambdaForTesting(
          [&](ash::file_system_provider::RequestMountCallback callback) {
            // The second check of reauth required after the mount succeeds
            // should be OK so we attempt upload.
            provided_file_system_->SetReauthenticationRequired(false);
            std::move(callback).Run(base::File::Error::FILE_OK);
          }));

  // Start the upload workflow and wait till the sign-in notification is shown.
  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  added_run_loop.Run();

  // Click on the sign-in button to initiate the auth flow.
  auto notification_id = base::StrCat(
      {policy::skyvault_ui_utils::kDownloadSignInNotificationPrefix, "1"});
  ASSERT_TRUE(
      display_service_tester_->GetNotification(notification_id).has_value());

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification_id,
      policy::skyvault_ui_utils::NotificationButtonIndex::kSignInButton,
      /*reply=*/std::nullopt);

  EXPECT_EQ(upload_callback.Get<bool>(), true);
  ASSERT_FALSE(
      display_service_tester_->GetNotification(notification_id).has_value());

  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Download.OneDrive.SignInError", false, 1);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Download.OneDrive.SignInError", true, 0);
}

// Test that when the OneDrive file system isn't mounted, the sign-in required
// notification is shown. When the sign-in notification is cancelled, the upload
// fails.
IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest,
                       FailToUploadDueToReauthenticationRequired) {
  SetUpMyFiles();
  const std::string test_file_name = "text.docx";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and wait till the sign-in notification is shown.
  base::RunLoop added_run_loop;
  display_service_tester_->SetNotificationAddedClosure(
      added_run_loop.QuitClosure());
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  added_run_loop.Run();

  // Click on the cancel so the upload will fail.
  auto notification_id = base::StrCat(
      {policy::skyvault_ui_utils::kDownloadSignInNotificationPrefix, "1"});
  ASSERT_TRUE(
      display_service_tester_->GetNotification(notification_id).has_value());

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification_id,
      policy::skyvault_ui_utils::NotificationButtonIndex::kCancelButton,
      /*reply=*/std::nullopt);

  EXPECT_EQ(upload_callback.Get<bool>(), false);
  ASSERT_FALSE(
      display_service_tester_->GetNotification(notification_id).has_value());

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Download.OneDrive.SignInError", false, 0);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Download.OneDrive.SignInError", true, 1);
}

}  // namespace ash::cloud_upload
