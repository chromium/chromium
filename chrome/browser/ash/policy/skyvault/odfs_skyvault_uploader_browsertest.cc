// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"
#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/mojom/network_change_manager.mojom-shared.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/test/message_center_waiter.h"

using policy::local_user_files::kUploadRootPrefix;
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
    CHECK(network::TestNetworkConnectionTracker::HasInstance());
  }

 protected:
  std::string GetNotificationId(const std::string& notification_id) {
    const user_manager::User& user = CHECK_DEREF(
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile()));
    return ash::CreateUserScopedNotificationId(notification_id,
                                               user.username_hash());
  }

  const message_center::Notification* GetNotification(
      const std::string& notification_id) {
    return message_center::MessageCenter::Get()->FindNotificationById(
        GetNotificationId(notification_id));
  }
  base::HistogramTester histogram_tester_;

  // Used to observe skyvault notifications during tests.
  base::RepeatingCallback<void(const message_center::Notification&)>
      on_notification_displayed_callback_;
};

class OdfsSkyvaultUploaderParamTest
    : public OdfsSkyvaultUploaderTest,
      public ::testing::WithParamInterface<UploadTrigger> {
 public:
  OdfsSkyvaultUploaderParamTest() = default;
  ~OdfsSkyvaultUploaderParamTest() override = default;

  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    switch (info.param) {
      case UploadTrigger::kDownload:
        return "download";
      case UploadTrigger::kScreenCapture:
        return "screen_capture";
      case UploadTrigger::kMigration:
        return "migration";
      case UploadTrigger::kCamera:
        return "camera";
    }
  }

 protected:
  UploadTrigger GetTrigger() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(OdfsSkyvaultUploaderParamTest, SuccessfulUpload) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and end the test once the upload callback is run.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  EXPECT_CALL(progress_callback, Run(/*bytes_transferred=*/230096));
  OdfsSkyvaultUploader::Upload(profile(), source_file_path, GetTrigger(),
                               progress_callback.Get(),
                               upload_callback.GetCallback());
  EXPECT_EQ(upload_callback.Get<bool>(), true);

  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

// Tests an upload to a specific folder and relative path on OneDrive.
IN_PROC_BROWSER_TEST_P(OdfsSkyvaultUploaderParamTest,
                       SuccessfulUploadWithRelativePath) {
  SetUpMyFiles();
  SetUpODFS();

  const std::string test_dir = "TestFolder";
  base::FilePath test_dir_path = CreateTestDir(test_dir, my_files_dir());
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path = CopyTestFile(test_file_name, test_dir_path);

  // Start the upload workflow and end the test once the upload callback is run.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<
      storage::FileSystemURL,
      std::optional<policy::local_user_files::MigrationUploadError>,
      base::FilePath>
      upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path,
      /*relative_source_path=*/base::FilePath(test_dir),
      /*upload_root=*/kUploadRootPrefix, GetTrigger(), progress_callback.Get(),
      upload_callback.GetCallback());

  auto [url, error, upload_root_path] = upload_callback.Get();
  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(GetODFS(profile())->GetFileSystemInfo().mount_path().Append(
                kUploadRootPrefix),
            upload_root_path);
  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(base::FilePath("/")
                            .AppendASCII(kUploadRootPrefix)
                            .AppendASCII(test_dir)
                            .AppendASCII(test_file_name));
}

IN_PROC_BROWSER_TEST_P(OdfsSkyvaultUploaderParamTest, CancelledUpload) {
  SetUpMyFiles();
  SetUpODFS();
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path =
      CopyTestFile(test_file_name, my_files_dir());

  // Start the upload workflow and cancel the upload immediately.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  base::WeakPtr<OdfsSkyvaultUploader> uploader = OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, GetTrigger(), progress_callback.Get(),
      upload_callback.GetCallback());
  uploader->Cancel();
  EXPECT_EQ(upload_callback.Get<bool>(), false);

  // Check that the source file has not been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

IN_PROC_BROWSER_TEST_P(OdfsSkyvaultUploaderParamTest,
                       FailToUploadDueToMemoryError) {
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
  OdfsSkyvaultUploader::Upload(profile(), source_file_path, GetTrigger(),
                               progress_callback.Get(),
                               upload_callback.GetCallback());
  EXPECT_EQ(upload_callback.Get<bool>(), false);

  // Check that the source file has not been moved to OneDrive.
  CheckPathNotFoundOnODFS(base::FilePath("/").AppendASCII(test_file_name));
}

INSTANTIATE_TEST_SUITE_P(SkyVault,
                         OdfsSkyvaultUploaderParamTest,
                         ::testing::Values(UploadTrigger::kDownload,
                                           UploadTrigger::kScreenCapture,
                                           UploadTrigger::kMigration,
                                           UploadTrigger::kCamera),
                         OdfsSkyvaultUploaderParamTest::ParamToName);

// Tests that if triggered because of migration, an upload will wait for
// connectivity instead of failing quickly.
IN_PROC_BROWSER_TEST_F(OdfsSkyvaultUploaderTest,
                       SuccessfulUploadAfterWaitingForNetwork) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  SetUpMyFiles();
  SetUpODFS();

  const std::string test_dir = "TestFolder";
  base::FilePath test_dir_path = CreateTestDir(test_dir, my_files_dir());
  const std::string test_file_name = "video_long.ogv";
  base::FilePath source_file_path = CopyTestFile(test_file_name, test_dir_path);

  // Start the upload workflow and end the test once the upload callback is run.
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<
      storage::FileSystemURL,
      std::optional<policy::local_user_files::MigrationUploadError>,
      base::FilePath>
      upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path,
      /*relative_source_path=*/base::FilePath(test_dir),
      /*upload_root=*/kUploadRootPrefix, UploadTrigger::kMigration,
      progress_callback.Get(), upload_callback.GetCallback());

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET);

  auto [url, error, upload_root_path] = upload_callback.Get();
  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(GetODFS(profile())->GetFileSystemInfo().mount_path().Append(
                kUploadRootPrefix),
            upload_root_path);
  // Check that the source file has been moved to OneDrive.
  CheckPathExistsOnODFS(base::FilePath("/")
                            .AppendASCII(kUploadRootPrefix)
                            .AppendASCII(test_dir)
                            .AppendASCII(test_file_name));
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
  const auto notification_id = base::StrCat(
      {policy::skyvault_ui_utils::kDownloadSignInNotificationPrefix, "1"});
  const std::string backend_notification_id =
      GetNotificationId(notification_id);
  message_center::MessageCenterWaiter waiter(backend_notification_id);
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  waiter.WaitUntilAdded();

  // Click on the sign-in button to initiate the auth flow.
  ASSERT_TRUE(GetNotification(notification_id));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      backend_notification_id,
      policy::skyvault_ui_utils::NotificationButtonIndex::kSignInButton);

  EXPECT_EQ(upload_callback.Get<bool>(), true);
  ASSERT_FALSE(GetNotification(notification_id));

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
  const auto notification_id = base::StrCat(
      {policy::skyvault_ui_utils::kDownloadSignInNotificationPrefix, "1"});
  const std::string backend_notification_id =
      GetNotificationId(notification_id);
  message_center::MessageCenterWaiter waiter(backend_notification_id);
  base::MockCallback<base::RepeatingCallback<void(int64_t)>> progress_callback;
  base::test::TestFuture<bool, storage::FileSystemURL> upload_callback;
  OdfsSkyvaultUploader::Upload(
      profile(), source_file_path, UploadTrigger::kDownload,
      progress_callback.Get(), upload_callback.GetCallback());
  waiter.WaitUntilAdded();

  // Click on the cancel so the upload will fail.
  ASSERT_TRUE(GetNotification(notification_id));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      backend_notification_id,
      policy::skyvault_ui_utils::NotificationButtonIndex::kCancelButton);

  EXPECT_EQ(upload_callback.Get<bool>(), false);
  ASSERT_FALSE(GetNotification(notification_id));

  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Download.OneDrive.SignInError", false, 0);
  histogram_tester_.ExpectBucketCount(
      "Enterprise.SkyVault.Download.OneDrive.SignInError", true, 1);
}

}  // namespace ash::cloud_upload
