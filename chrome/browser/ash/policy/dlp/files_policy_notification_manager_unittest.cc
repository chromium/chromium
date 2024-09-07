// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/test/files_policy_notification_manager_test_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

using testing::Field;

using policy::CreateDummyFile;
using policy::CreateFileSystemURL;
using policy::GetIOTaskController;
using policy::kNotificationId;

constexpr char kFile1[] = "test1.txt";
constexpr char kFile2[] = "test2.txt";
constexpr char kFile3[] = "test3.txt";

std::u16string GetWarningTitle(dlp::FileAction action) {
  switch (action) {
    case dlp::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_REVIEW_TITLE);
    case dlp::FileAction::kTransfer:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_REVIEW_TITLE);
    case dlp::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_REVIEW_TITLE);
    case dlp::FileAction::kCopy:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE);
    case dlp::FileAction::kMove:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_OPEN_REVIEW_TITLE);
    case dlp::FileAction::kUnknown:
      return u"";
  }
}

std::u16string GetWarningOkButton(dlp::FileAction action) {
  switch (action) {
    case dlp::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kTransfer:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kMove:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kUnknown:
      return u"";
  }
}

// Converts file_manager::io_task::PolicyErrorType to
// FilesPolicyDialog::BlockReason.
FilesPolicyDialog::BlockReason ConvertPolicy(
    file_manager::io_task::PolicyErrorType policy_error_type) {
  switch (policy_error_type) {
    case file_manager::io_task::PolicyErrorType::kDlp:
      return FilesPolicyDialog::BlockReason::kDlp;
    case file_manager::io_task::PolicyErrorType::kEnterpriseConnectors:
      // We don't have elements to identify a specific enterprise connectors
      // block reason from a PolicyErrorType. For testing purposes, we simply
      // return a generic reason.
      return FilesPolicyDialog::BlockReason::kEnterpriseConnectors;
    case file_manager::io_task::PolicyErrorType::kDlpWarningTimeout:
      NOTREACHED();
  }
}

class IOTaskStatusObserver
    : public file_manager::io_task::IOTaskController::Observer {
 public:
  MOCK_METHOD(void,
              OnIOTaskStatus,
              (const file_manager::io_task::ProgressStatus&),
              (override));
};

}  // namespace

class FilesPolicyNotificationManagerTest : public testing::Test {
 public:
  FilesPolicyNotificationManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  FilesPolicyNotificationManagerTest(
      const FilesPolicyNotificationManagerTest&) = delete;
  FilesPolicyNotificationManagerTest& operator=(
      const FilesPolicyNotificationManagerTest&) = delete;
  ~FilesPolicyNotificationManagerTest() override = default;
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kNewFilesPolicyUX);

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  ash::disks::DiskMountManager::GetInstance(), nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);

    io_task_controller_ = GetIOTaskController(profile_);
    ASSERT_TRUE(io_task_controller_);
    fpnm_ = std::make_unique<FilesPolicyNotificationManager>(profile_);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, temp_dir_.GetPath());
  }

  void TearDown() override {
    fpnm_ = nullptr;

    profile_manager_.DeleteAllTestingProfiles();
    ash::disks::DiskMountManager::Shutdown();
  }

  // Creates and adds a CopyOrMoveIOTask with `task_id` with type
  // `OperationType::kCopy` if `is_copy` is true, and `OperationType::kMove` if
  // false.
  base::FilePath AddCopyOrMoveIOTask(file_manager::io_task::IOTaskId id,
                                     bool is_copy) {
    return policy::AddCopyOrMoveIOTask(
        profile_, file_system_context_, id,
        is_copy ? file_manager::io_task::OperationType::kCopy
                : file_manager::io_task::OperationType::kMove,
        temp_dir_.GetPath(), kFile1, kTestStorageKey);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FilesPolicyNotificationManager> fpnm_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<file_manager::io_task::IOTaskController, DanglingUntriaged>
      io_task_controller_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  base::ScopedTempDir temp_dir_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
};

TEST_F(FilesPolicyNotificationManagerTest, AddCopyTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  ASSERT_FALSE(AddCopyOrMoveIOTask(task_id, /*is_copy=*/true).empty());

  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  // Pause the task. It shouldn't be removed.
  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params.emplace(Policy::kDlp, /*warning_files_count=*/1);
  io_task_controller_->Pause(task_id, std::move(pause_params));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  // Once the task is complete, it should be removed.
  io_task_controller_->Cancel(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
}

// Only Copy and move tasks are observed by FilesPolicyNotificationManager.
TEST_F(FilesPolicyNotificationManagerTest, AddTrashTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII(kFile1);
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(kTestStorageKey, src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());

  auto task = std::make_unique<file_manager::io_task::TrashIOTask>(
      std::vector<storage::FileSystemURL>({src_url}), profile_,
      file_system_context_, base::FilePath());

  io_task_controller_->Add(std::move(task));
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));

  io_task_controller_->Cancel(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
}

// FilesPolicyNotificationManager assigns new IDs for new notifications,
// regardless of the action and files.
TEST_F(FilesPolicyNotificationManagerTest, NotificationIdsAreUnique) {
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const auto histogram_tester = base::HistogramTester();

  std::string notification_id_1 = "dlp_files_0";
  std::string notification_id_2 = "dlp_files_1";
  std::string notification_id_3 = "dlp_files_2";

  std::vector<base::FilePath> files_1 = {
      base::FilePath(kFile1), base::FilePath(kFile2), base::FilePath(kFile3)};

  // None are shown.
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_3));
  // Show first notification for upload.
  fpnm_->ShowDlpBlockedFiles(/*task_id=*/std::nullopt, files_1,
                             dlp::FileAction::kUpload);
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_3));
  // Show another notification for the same action - should get a new ID.
  fpnm_->ShowDlpBlockedFiles(/*task_id=*/std::nullopt, files_1,
                             dlp::FileAction::kUpload);
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_3));
  // Show a notification for a different action & files - should still increment
  // the ID.
  fpnm_->ShowDlpBlockedFiles(
      /*task_id=*/std::nullopt,
      {base::FilePath(kFile1), base::FilePath(kFile2)}, dlp::FileAction::kOpen);
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_3));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockedUMA)),
              base::BucketsAre(base::Bucket(dlp::FileAction::kUpload, 2),
                               base::Bucket(dlp::FileAction::kOpen, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(data_controls::GetDlpHistogramPrefix() +
                                     data_controls::dlp::kFilesBlockedCountUMA),
      testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 2)));
}

class FPNMIOTaskTest : public FilesPolicyNotificationManagerTest {
 protected:
  // Depending on the block reason, calls FPNM::ShowDlpBlockedFiles() or
  // FPNM::AddConnectorsBlockedFiles(), both of which store all the info about
  // the task to later show notifications/dialogs.
  void AddBlockedFiles(
      FilesPolicyDialog::FilesPolicyDialog::BlockReason block_reason,
      file_manager::io_task::IOTaskId task_id,
      std::vector<base::FilePath> blocked_files,
      dlp::FileAction action,
      std::optional<FilesPolicyDialog::Info> dialog_info = std::nullopt) {
    if (block_reason == FilesPolicyDialog::BlockReason::kDlp) {
      fpnm_->ShowDlpBlockedFiles(task_id, std::move(blocked_files), action);
    } else {
      EXPECT_TRUE(dialog_info.has_value());
      fpnm_->SetConnectorsBlockedFiles(task_id, action, block_reason,
                                       std::move(dialog_info.value()));
    }
  }

  // Depending on the policy, calls FPNM::ShowDlpWarning() or
  // FPNM::ShowConnectorsWarning(), both of which store all the info about the
  // task to later show notifications/dialogs.
  void AddWarnedFiles(
      Policy policy,
      WarningWithJustificationCallback cb,
      file_manager::io_task::IOTaskId task_id,
      std::vector<base::FilePath> warned_files,
      dlp::FileAction action,
      std::optional<FilesPolicyDialog::Info> dialog_info = std::nullopt) {
    switch (policy) {
      case Policy::kDlp:
        fpnm_->ShowDlpWarning(std::move(cb), task_id, std::move(warned_files),
                              DlpFileDestination(), action);
        break;
      case Policy::kEnterpriseConnectors:
        if (!dialog_info.has_value()) {
          dialog_info = FilesPolicyDialog::Info::Warn(
              FilesPolicyDialog::BlockReason::
                  kEnterpriseConnectorsSensitiveData,
              warned_files);
        }
        fpnm_->ShowConnectorsWarning(std::move(cb), task_id, action,
                                     std::move(dialog_info.value()));
        break;
    }
  }

  const base::HistogramTester histogram_tester_;
};

// Tests that calling FPNM::ShowBlockedNotifications() correctly shows block
// notifications for a tracked IO task with blocked files.
TEST_F(FPNMIOTaskTest, ShowBlockedNotifications_ShowsWhenHasBlockedFiles) {
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "swa-file-operation-1";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  ASSERT_FALSE(AddCopyOrMoveIOTask(task_id, /*is_copy=*/true).empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));
  AddBlockedFiles(FilesPolicyDialog::BlockReason::kDlp, task_id,
                  {base::FilePath(kFile1), base::FilePath(kFile2)},
                  dlp::FileAction::kCopy);

  fpnm_->ShowBlockedNotifications();
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockedUMA)),
              base::BucketsAre(base::Bucket(dlp::FileAction::kCopy, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  data_controls::dlp::kFilesBlockedCountUMA),
              testing::ElementsAre(base::Bucket(2, 1)));
}

// Tests that calling FPNM::ShowBlockedNotifications() doesn't show any
// notifications for a tracked IO task with warning, but no blocked files.
TEST_F(FPNMIOTaskTest, ShowBlockedNotifications_IgnoresWarnedFiles) {
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "swa-file-operation-1";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  ASSERT_FALSE(AddCopyOrMoveIOTask(task_id, /*is_copy=*/true).empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));
  AddWarnedFiles(Policy::kDlp, base::DoNothing(), task_id,
                 {base::FilePath(kFile1), base::FilePath(kFile2)},
                 dlp::FileAction::kCopy);

  fpnm_->ShowBlockedNotifications();
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  VerifyFilesWarningUMAs(
      histogram_tester_,
      /*action_warned_buckets=*/{base::Bucket(dlp::FileAction::kCopy, 1)},
      /*warning_count_buckets=*/{base::Bucket(2, 1)},
      /*action_timedout_buckets=*/{});
}

// Tests that calling FPNM::OnErrorItemDismissed() removes all stored info when
// the task was tracked.
TEST_F(FPNMIOTaskTest, OnErrorItemDismissedClearsInfoForTrackedTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  ASSERT_FALSE(AddCopyOrMoveIOTask(task_id, /*is_copy=*/true).empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));
  AddBlockedFiles(FilesPolicyDialog::BlockReason::kDlp, task_id,
                  {base::FilePath(kFile1), base::FilePath(kFile2)},
                  dlp::FileAction::kCopy);
  fpnm_->OnErrorItemDismissed(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockedUMA)),
              base::BucketsAre(base::Bucket(dlp::FileAction::kCopy, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  data_controls::dlp::kFilesBlockedCountUMA),
              testing::ElementsAre(base::Bucket(2, 1)));
}

// Tests that calling FPNM::OnErrorItemDismissed() for a non-tracked task
// succeeds.
TEST_F(FPNMIOTaskTest, OnErrorItemDismissedIgnoresNonTrackedTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
  fpnm_->OnErrorItemDismissed(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
}

// Tests that if custom settings are provided, the notification shows the review
// button even for a single warned file.
TEST_F(FPNMIOTaskTest,
       EnterpriseConnectors_PausedShowsWarningNotification_SingleFile_Review) {
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  file_manager::io_task::OperationType type =
      file_manager::io_task::OperationType::kCopy;
  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  auto dialog_info = FilesPolicyDialog::Info::Warn(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      {base::FilePath(kFile1)});
  dialog_info.SetMessage(u"Custom message");
  dialog_info.SetLearnMoreURL(GURL("https://example.com"));
  AddWarnedFiles(Policy::kEnterpriseConnectors, base::DoNothing(), task_id,
                 {base::FilePath(kFile1)}, dlp::FileAction::kCopy, dialog_info);

  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  // Only the task_id field is important.
  file_manager::io_task::ProgressStatus status;
  status.task_id = task_id;
  status.state = file_manager::io_task::State::kPaused;
  status.type = type;
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path.value()),
      std::nullopt);
  status.pause_params.policy_params = file_manager::io_task::PolicyPauseParams(
      Policy::kEnterpriseConnectors, /*warning_files_count=*/1);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), GetWarningTitle(dlp::FileAction::kCopy));
  EXPECT_EQ(notification->message(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(
                    IDS_POLICY_DLP_FILES_WARN_MESSAGE, 1),
                src_file_path.BaseName().LossyDisplayName(),
                /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_REVIEW_BUTTON));

  EXPECT_TRUE(notification->never_timeout());
}

// Tests that if custom settings are provided, the notification shows the review
// button even for a single blocked file.
TEST_F(FPNMIOTaskTest,
       EnterpriseConnectors_ErrorShowsBlockNotification_SingleFile_Review) {
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  file_manager::io_task::OperationType type =
      file_manager::io_task::OperationType::kCopy;
  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  const std::vector<base::FilePath> files = {base::FilePath(kFile1)};
  auto dialog_info = FilesPolicyDialog::Info::Error(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      files);
  dialog_info.SetMessage(u"Custom message");
  dialog_info.SetLearnMoreURL(GURL("https://example.com"));
  AddBlockedFiles(
      FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
      task_id, std::move(files), dlp::FileAction::kCopy,
      std::move(dialog_info));

  // Only the task_id field is important.
  file_manager::io_task::ProgressStatus status;
  status.task_id = task_id;
  status.state = file_manager::io_task::State::kError;
  status.type = type;
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path.value()),
      std::nullopt);
  status.policy_error.emplace(
      file_manager::io_task::PolicyErrorType::kEnterpriseConnectors,
      /*blocked_files=*/1);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(),
            l10n_util::GetPluralStringFUTF16(
                IDS_POLICY_DLP_FILES_COPY_BLOCKED_TITLE, 1));
  EXPECT_EQ(notification->message(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(
                    IDS_POLICY_DLP_FILES_CONTENT_BLOCK_MESSAGE, 1),
                src_file_path.BaseName().LossyDisplayName(),
                /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_REVIEW_BUTTON));

  EXPECT_TRUE(notification->never_timeout());
}

class FilesPolicyNotificationManagerDlpAndConnectorsWarningTest
    : public FPNMIOTaskTest,
      public testing::WithParamInterface<Policy> {
 public:
  Policy GetPolicy() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    DLP,
    FilesPolicyNotificationManagerDlpAndConnectorsWarningTest,
    ::testing::Values(Policy::kDlp));

INSTANTIATE_TEST_SUITE_P(
    EnterpriseConnectors,
    FilesPolicyNotificationManagerDlpAndConnectorsWarningTest,
    ::testing::Values(Policy::kEnterpriseConnectors));

// Tests that passing task id to ShowDlpWarning will pause the corresponding
// IOTask. Completing the task with error should abort it and run the warning
// callback with false.
TEST_P(FilesPolicyNotificationManagerDlpAndConnectorsWarningTest,
       WarningPausesIOTask) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));
  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params.emplace(GetPolicy(), /*warning_files_count=*/1,
                                     src_file_path.BaseName().value());

  // Task is paused.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kPaused),
                Field(&file_manager::io_task::ProgressStatus::pause_params,
                      pause_params))))
      .Times(::testing::AtLeast(1));

  base::MockCallback<WarningWithJustificationCallback> mock_cb;

  AddWarnedFiles(GetPolicy(), mock_cb.Get(), task_id,
                 std::vector<base::FilePath>{src_file_path},
                 dlp::FileAction::kCopy);
  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  // Task is completed with error.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kError),
                Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::policy_error,
                      file_manager::io_task::PolicyError(
                          file_manager::io_task::PolicyErrorType::kDlp,
                          /*blocked_files=*/1)))))
      .Times(::testing::AtLeast(1));

  EXPECT_CALL(mock_cb,
              Run(/*user_justification=*/std::optional<std::u16string>(),
                  /*should_proceed=*/false));
  io_task_controller_->CompleteWithError(
      task_id,
      file_manager::io_task::PolicyError(
          file_manager::io_task::PolicyErrorType::kDlp, /*blocked_files=*/1));

  base::RunLoop().RunUntilIdle();
  io_task_controller_->RemoveObserver(&observer);
  EXPECT_FALSE(fpnm_->HasWarningTimerForTesting(task_id));

  if (GetPolicy() == Policy::kDlp) {
    VerifyFilesWarningUMAs(
        histogram_tester_,
        /*action_warned_buckets=*/{base::Bucket(dlp::FileAction::kCopy, 1)},
        /*warning_count_buckets=*/{base::Bucket(1, 1)},
        /*action_timedout_buckets=*/{});
  }
}

// Tests that cancelling a paused IO task will run the warning callback.
TEST_P(FilesPolicyNotificationManagerDlpAndConnectorsWarningTest,
       WarningCancelled) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));

  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params = file_manager::io_task::PolicyPauseParams(
      GetPolicy(), /*warning_files_count=*/1, src_file_path.BaseName().value());

  // Task is paused.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kPaused),
                Field(&file_manager::io_task::ProgressStatus::pause_params,
                      pause_params))))
      .Times(::testing::AtLeast(1));
  testing::StrictMock<base::MockCallback<WarningWithJustificationCallback>>
      mock_cb;
  AddWarnedFiles(GetPolicy(), mock_cb.Get(), task_id,
                 std::vector<base::FilePath>{src_file_path},
                 dlp::FileAction::kCopy);

  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  // Task is cancelled.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kCancelled))));
  // Warning callback is run with should_proceed set to false when the task is
  // cancelled.
  EXPECT_CALL(mock_cb,
              Run(/*user_justification=*/std::optional<std::u16string>(),
                  /*should_proceed=*/false))
      .Times(1);
  io_task_controller_->Cancel(task_id);

  base::RunLoop().RunUntilIdle();
  io_task_controller_->RemoveObserver(&observer);
  EXPECT_FALSE(fpnm_->HasWarningTimerForTesting(task_id));

  if (GetPolicy() == Policy::kDlp) {
    VerifyFilesWarningUMAs(
        histogram_tester_,
        /*action_warned_buckets=*/{base::Bucket(dlp::FileAction::kCopy, 1)},
        /*warning_count_buckets=*/{base::Bucket(1, 1)},
        /*action_timedout_buckets=*/{});
  }
}

// Tests that resuming a paused IO task will run the warning callback.
TEST_P(FilesPolicyNotificationManagerDlpAndConnectorsWarningTest,
       WarningResumed) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));

  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params = file_manager::io_task::PolicyPauseParams(
      GetPolicy(), /*warning_files_count=*/1, src_file_path.BaseName().value());

  // Task is paused.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kPaused),
                Field(&file_manager::io_task::ProgressStatus::pause_params,
                      pause_params))))
      .Times(::testing::AtLeast(1));

  testing::StrictMock<base::MockCallback<WarningWithJustificationCallback>>
      mock_cb;

  AddWarnedFiles(GetPolicy(), mock_cb.Get(), task_id,
                 std::vector<base::FilePath>{src_file_path},
                 dlp::FileAction::kCopy);

  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  // Warning callback is run with should_proceed set to true when the task is
  // resumed.
  EXPECT_CALL(mock_cb,
              Run(/*user_justification=*/std::optional<std::u16string>(),
                  /*should_proceed=*/true))
      .Times(1);
  fpnm_->OnIOTaskResumed(task_id);
  EXPECT_FALSE(fpnm_->HasWarningTimerForTesting(task_id));

  if (GetPolicy() == Policy::kDlp) {
    VerifyFilesWarningUMAs(
        histogram_tester_,
        /*action_warned_buckets=*/{base::Bucket(dlp::FileAction::kCopy, 1)},
        /*warning_count_buckets=*/{base::Bucket(1, 1)},
        /*action_timedout_buckets=*/{});
  }
}

// Tests that warning files from non-tracked IO task will add it to FPNM.
TEST_P(FilesPolicyNotificationManagerDlpAndConnectorsWarningTest,
       TaskWarnedNotTracked) {
  fpnm_->Shutdown();
  fpnm_.reset();

  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;
  auto dst_url =
      CreateFileSystemURL(kTestStorageKey, temp_dir_.GetPath().value());

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));

  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params = file_manager::io_task::PolicyPauseParams(
      GetPolicy(), /*warning_files_count=*/1, src_file_path.BaseName().value());

  // Task is paused.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kPaused),
                Field(&file_manager::io_task::ProgressStatus::pause_params,
                      pause_params))))
      .Times(::testing::AtLeast(1));

  testing::StrictMock<base::MockCallback<WarningWithJustificationCallback>>
      mock_cb;
  fpnm_ = std::make_unique<FilesPolicyNotificationManager>(profile_);
  ASSERT_FALSE(fpnm_->HasIOTask(task_id));

  AddWarnedFiles(GetPolicy(), mock_cb.Get(), task_id,
                 std::vector<base::FilePath>{src_file_path},
                 dlp::FileAction::kCopy);

  EXPECT_TRUE(fpnm_->HasIOTask(task_id));
  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  if (GetPolicy() == Policy::kDlp) {
    VerifyFilesWarningUMAs(
        histogram_tester_,
        /*action_warned_buckets=*/{base::Bucket(dlp::FileAction::kCopy, 1)},
        /*warning_count_buckets=*/{base::Bucket(1, 1)},
        /*action_timedout_buckets=*/{});
  }
}

class FilesPolicyNotificationManagerDlpAndConnectorsBlockTest
    : public FPNMIOTaskTest,
      public testing::WithParamInterface<FilesPolicyDialog::BlockReason> {
 public:
  FilesPolicyDialog::BlockReason GetBlockReason() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    DLP,
    FilesPolicyNotificationManagerDlpAndConnectorsBlockTest,
    ::testing::Values(
        FilesPolicyDialog::BlockReason::kDlp,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknownScanResult,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsScanFailed,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsEncryptedFile,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsLargeFile,
        FilesPolicyDialog::BlockReason::kEnterpriseConnectors));

// ShowDlpBlockedFiles/AddConnectorsBlockedFiles updates IO task info.
TEST_P(FilesPolicyNotificationManagerDlpAndConnectorsBlockTest,
       ShowDlpIOBlockedFiles) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));

  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  const std::vector<base::FilePath> paths = {src_file_path};
  auto dialog_info = FilesPolicyDialog::Info::Error(GetBlockReason(), paths);
  AddBlockedFiles(GetBlockReason(), task_id, paths, dlp::FileAction::kCopy,
                  dialog_info);

  // Task in progress.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kInProgress))));

  // Task completes successfully.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kSuccess))));

  base::RunLoop().RunUntilIdle();
  io_task_controller_->RemoveObserver(&observer);

  // Task is not removed after completion.
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>
      expected_dialog_info_map;
  auto confidential_files =
      std::vector<DlpConfidentialFile>(paths.begin(), paths.end());
  expected_dialog_info_map.insert({GetBlockReason(), dialog_info});

  EXPECT_EQ(fpnm_->GetIOTaskDialogInfoMapForTesting(task_id),
            expected_dialog_info_map);

  if (GetBlockReason() == FilesPolicyDialog::BlockReason::kDlp) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    data_controls::GetDlpHistogramPrefix() +
                    std::string(data_controls::dlp::kFileActionBlockedUMA)),
                base::BucketsAre(base::Bucket(dlp::FileAction::kCopy, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kFilesBlockedCountUMA),
                testing::ElementsAre(base::Bucket(1, 1)));
  }
}

// Tests that blocking files from non-tracked IO task will add it to FPNM.
TEST_P(FilesPolicyNotificationManagerDlpAndConnectorsBlockTest,
       TaskBlockedNotTracked) {
  fpnm_->Shutdown();
  fpnm_.reset();

  int task_id = 1;
  auto dst_url =
      CreateFileSystemURL(kTestStorageKey, temp_dir_.GetPath().value());

  auto src_file_path = AddCopyOrMoveIOTask(task_id, /*is_copy=*/true);
  ASSERT_FALSE(src_file_path.empty());

  fpnm_ = std::make_unique<FilesPolicyNotificationManager>(profile_);
  ASSERT_FALSE(fpnm_->HasIOTask(task_id));

  const std::vector<base::FilePath> paths = {src_file_path};
  auto dialog_info = FilesPolicyDialog::Info::Error(GetBlockReason(), paths);
  AddBlockedFiles(GetBlockReason(), task_id, paths, dlp::FileAction::kCopy,
                  dialog_info);

  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>
      expected_dialog_info_map;
  auto confidential_files =
      std::vector<DlpConfidentialFile>(paths.begin(), paths.end());
  expected_dialog_info_map.insert({GetBlockReason(), dialog_info});

  EXPECT_EQ(fpnm_->GetIOTaskDialogInfoMapForTesting(task_id),
            expected_dialog_info_map);

  if (GetBlockReason() == FilesPolicyDialog::BlockReason::kDlp) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    data_controls::GetDlpHistogramPrefix() +
                    std::string(data_controls::dlp::kFileActionBlockedUMA)),
                base::BucketsAre(base::Bucket(dlp::FileAction::kCopy, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kFilesBlockedCountUMA),
                testing::ElementsAre(base::Bucket(1, 1)));
  }
}

class FPNMPausedStatusNotification
    : public FPNMIOTaskTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType,
                     Policy,
                     dlp::FileAction>> {};

TEST_P(FPNMPausedStatusNotification, PausedShowsWarningNotification_Single) {
  auto [type, policy, action] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  bool is_copy = type == file_manager::io_task::OperationType::kCopy;
  auto src_file_path = AddCopyOrMoveIOTask(task_id, is_copy);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  AddWarnedFiles(policy, base::DoNothing(), task_id, {base::FilePath(kFile1)},
                 is_copy ? dlp::FileAction::kCopy : dlp::FileAction::kMove);

  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  // Only the task_id field is important.
  file_manager::io_task::ProgressStatus status;
  status.task_id = task_id;
  status.state = file_manager::io_task::State::kPaused;
  status.type = type;
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path.value()),
      std::nullopt);
  status.pause_params.policy_params = file_manager::io_task::PolicyPauseParams(
      policy, /*warning_files_count=*/1);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), GetWarningTitle(action));
  EXPECT_EQ(notification->message(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(
                    IDS_POLICY_DLP_FILES_WARN_MESSAGE, 1),
                src_file_path.BaseName().LossyDisplayName(),
                /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title, GetWarningOkButton(action));
  EXPECT_TRUE(notification->never_timeout());

  if (policy == Policy::kDlp) {
    VerifyFilesWarningUMAs(histogram_tester_,
                           /*action_warned_buckets=*/{base::Bucket(action, 1)},
                           /*warning_count_buckets=*/{base::Bucket(1, 1)},
                           /*action_timedout_buckets=*/{});
  }
}

TEST_P(FPNMPausedStatusNotification, PausedShowsWarningNotification_Multi) {
  auto [type, policy, action] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  bool is_copy = (type == file_manager::io_task::OperationType::kCopy);
  ASSERT_FALSE(AddCopyOrMoveIOTask(task_id, is_copy).empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  AddWarnedFiles(policy, base::DoNothing(), task_id,
                 {base::FilePath(kFile1), base::FilePath(kFile2)}, action);

  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  // Only the task_id field is important.
  file_manager::io_task::ProgressStatus status;
  status.task_id = task_id;
  status.state = file_manager::io_task::State::kPaused;
  status.type = type;
  base::FilePath src_file_path_1 = temp_dir_.GetPath().AppendASCII(kFile1);
  ASSERT_FALSE(src_file_path_1.empty());
  base::FilePath src_file_path_2 = temp_dir_.GetPath().AppendASCII(kFile2);
  ASSERT_FALSE(src_file_path_2.empty());
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path_1.value()),
      std::nullopt);
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path_2.value()),
      std::nullopt);
  status.pause_params.policy_params = file_manager::io_task::PolicyPauseParams(
      policy, /*warning_files_count=*/2);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), GetWarningTitle(action));
  EXPECT_EQ(
      notification->message(),
      base::ReplaceStringPlaceholders(l10n_util::GetPluralStringFUTF16(
                                          IDS_POLICY_DLP_FILES_WARN_MESSAGE, 2),
                                      u"2",
                                      /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_REVIEW_BUTTON));
  EXPECT_TRUE(notification->never_timeout());

  EXPECT_TRUE(fpnm_->HasWarningTimerForTesting(task_id));

  if (policy == Policy::kDlp) {
    VerifyFilesWarningUMAs(histogram_tester_,
                           /*action_warned_buckets=*/{base::Bucket(action, 1)},
                           /*warning_count_buckets=*/{base::Bucket(2, 1)},
                           /*action_timedout_buckets=*/{});
  }
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMPausedStatusNotification,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        Policy::kDlp,
                        dlp::FileAction::kCopy),
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        Policy::kEnterpriseConnectors,
                        dlp::FileAction::kCopy),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        Policy::kDlp,
                        dlp::FileAction::kMove),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        Policy::kEnterpriseConnectors,
                        dlp::FileAction::kMove)));

class FPNMErrorStatusNotification
    : public FPNMIOTaskTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType,
                     file_manager::io_task::PolicyErrorType,
                     int,
                     int>> {};

TEST_P(FPNMErrorStatusNotification, ErrorShowsBlockNotification_Single) {
  auto [type, policy, title_id, message_id] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  bool is_copy = type == file_manager::io_task::OperationType::kCopy;
  auto src_file_path = AddCopyOrMoveIOTask(task_id, is_copy);
  ASSERT_FALSE(src_file_path.empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  const std::vector<base::FilePath> files = {base::FilePath(kFile1)};
  auto dialog_info =
      FilesPolicyDialog::Info::Error(ConvertPolicy(policy), files);
  AddBlockedFiles(ConvertPolicy(policy), task_id, std::move(files),
                  is_copy ? dlp::FileAction::kCopy : dlp::FileAction::kMove,
                  std::move(dialog_info));

  // Only the task_id field is important.
  file_manager::io_task::ProgressStatus status;
  status.task_id = task_id;
  status.state = file_manager::io_task::State::kError;
  status.type = type;
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path.value()),
      std::nullopt);
  status.policy_error.emplace(policy, /*blocked_files=*/1);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(),
            l10n_util::GetPluralStringFUTF16(title_id, 1));
  EXPECT_EQ(notification->message(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(message_id, 1),
                src_file_path.BaseName().LossyDisplayName(),
                /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  EXPECT_TRUE(notification->never_timeout());

  if (policy == file_manager::io_task::PolicyErrorType::kDlp) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            data_controls::GetDlpHistogramPrefix() +
            std::string(data_controls::dlp::kFileActionBlockedUMA)),
        base::BucketsAre(base::Bucket(
            is_copy ? dlp::FileAction::kCopy : dlp::FileAction::kMove, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kFilesBlockedCountUMA),
                testing::ElementsAre(base::Bucket(1, 1)));
  }
}

TEST_P(FPNMErrorStatusNotification, ErrorShowsBlockNotification_Multi) {
  auto [type, policy, title_id, message_id] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  bool is_copy = type == file_manager::io_task::OperationType::kCopy;
  ASSERT_FALSE(AddCopyOrMoveIOTask(task_id, is_copy).empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  const std::vector<base::FilePath> files = {base::FilePath(kFile1),
                                             base::FilePath(kFile2)};
  auto dialog_info =
      FilesPolicyDialog::Info::Error(ConvertPolicy(policy), files);
  AddBlockedFiles(ConvertPolicy(policy), task_id, std::move(files),
                  is_copy ? dlp::FileAction::kCopy : dlp::FileAction::kMove,
                  std::move(dialog_info));

  // Only the task_id field is important.
  file_manager::io_task::ProgressStatus status;
  status.task_id = task_id;
  status.state = file_manager::io_task::State::kError;
  status.type = type;
  base::FilePath src_file_path_1 = temp_dir_.GetPath().AppendASCII(kFile1);
  ASSERT_FALSE(src_file_path_1.empty());
  base::FilePath src_file_path_2 = temp_dir_.GetPath().AppendASCII(kFile2);
  ASSERT_FALSE(src_file_path_2.empty());
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path_1.value()),
      std::nullopt);
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path_2.value()),
      std::nullopt);
  status.policy_error.emplace(policy, 2);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(title_id, 2), u"2",
                /*offset=*/nullptr));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_BLOCK_MESSAGE));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_REVIEW_BUTTON));
  EXPECT_TRUE(notification->never_timeout());

  if (policy == file_manager::io_task::PolicyErrorType::kDlp) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            data_controls::GetDlpHistogramPrefix() +
            std::string(data_controls::dlp::kFileActionBlockedUMA)),
        base::BucketsAre(base::Bucket(
            is_copy ? dlp::FileAction::kCopy : dlp::FileAction::kMove, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kFilesBlockedCountUMA),
                testing::ElementsAre(base::Bucket(2, 1)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMErrorStatusNotification,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        file_manager::io_task::PolicyErrorType::kDlp,
                        IDS_POLICY_DLP_FILES_COPY_BLOCKED_TITLE,
                        IDS_POLICY_DLP_FILES_POLICY_BLOCK_MESSAGE),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        file_manager::io_task::PolicyErrorType::kDlp,
                        IDS_POLICY_DLP_FILES_MOVE_BLOCKED_TITLE,
                        IDS_POLICY_DLP_FILES_POLICY_BLOCK_MESSAGE),
        std::make_tuple(
            file_manager::io_task::OperationType::kCopy,
            file_manager::io_task::PolicyErrorType::kEnterpriseConnectors,
            IDS_POLICY_DLP_FILES_COPY_BLOCKED_TITLE,
            IDS_POLICY_DLP_FILES_CONTENT_BLOCK_MESSAGE),
        std::make_tuple(
            file_manager::io_task::OperationType::kMove,
            file_manager::io_task::PolicyErrorType::kEnterpriseConnectors,
            IDS_POLICY_DLP_FILES_MOVE_BLOCKED_TITLE,
            IDS_POLICY_DLP_FILES_CONTENT_BLOCK_MESSAGE)));

class FPNMTimeoutStatusNotification
    : public FilesPolicyNotificationManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType,
                     dlp::FileAction,
                     int,
                     int>> {};

TEST_P(FPNMTimeoutStatusNotification, TimeoutErrorShowsTimeoutNotification) {
  auto [type, action, title_id, message_id] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::IOTaskId task_id = 1;
  ASSERT_FALSE(AddCopyOrMoveIOTask(
                   task_id, /*is_copy=*/type ==
                                file_manager::io_task::OperationType::kCopy)
                   .empty());
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kError;
  status.type = type;
  base::FilePath src_file_path_1 = temp_dir_.GetPath().AppendASCII(kFile1);
  ASSERT_FALSE(src_file_path_1.empty());
  base::FilePath src_file_path_2 = temp_dir_.GetPath().AppendASCII(kFile2);
  ASSERT_FALSE(src_file_path_2.empty());
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path_1.value()),
      std::nullopt);
  status.sources.emplace_back(
      CreateFileSystemURL(kTestStorageKey, src_file_path_2.value()),
      std::nullopt);
  status.policy_error.emplace(
      file_manager::io_task::PolicyErrorType::kDlpWarningTimeout);

  fpnm_->ShowFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), l10n_util::GetStringUTF16(title_id));
  EXPECT_EQ(notification->message(), l10n_util::GetStringUTF16(message_id));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_TRUE(notification->never_timeout());
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMTimeoutStatusNotification,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        dlp::FileAction::kCopy,
                        IDS_POLICY_DLP_FILES_COPY_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_COPY_TIMEOUT_MESSAGE),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        dlp::FileAction::kMove,
                        IDS_POLICY_DLP_FILES_MOVE_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_MOVE_TIMEOUT_MESSAGE)));

class FPNMShowBlockTest
    : public FilesPolicyNotificationManagerTest,
      public ::testing::WithParamInterface<std::tuple<dlp::FileAction, int>> {
 protected:
  void SetUp() override { FilesPolicyNotificationManagerTest::SetUp(); }

  const base::HistogramTester histogram_tester_;
};

TEST_P(FPNMShowBlockTest, ShowDlpBlockNotification_Single) {
  auto [action, title_id] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  auto src_file_path = base::FilePath(kFile1);
  fpnm_->ShowDlpBlockedFiles(/*task_id=*/std::nullopt, {src_file_path}, action);
  std::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(),
            l10n_util::GetPluralStringFUTF16(title_id, 1));
  EXPECT_EQ(notification->message(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(
                    IDS_POLICY_DLP_FILES_POLICY_BLOCK_MESSAGE, 1),
                src_file_path.BaseName().LossyDisplayName(),
                /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  data_controls::dlp::kFilesBlockedCountUMA),
              testing::ElementsAre(base::Bucket(1, 1)));
}

TEST_P(FPNMShowBlockTest, ShowDlpBlockNotification_Multi) {
  auto [action, title_id] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  fpnm_->ShowDlpBlockedFiles(
      /*task_id=*/std::nullopt,
      {base::FilePath(kFile1), base::FilePath(kFile2), base::FilePath(kFile3)},
      action);
  std::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(title_id, 3), u"3",
                /*offset=*/nullptr));
  EXPECT_EQ(notification->message(),
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_BLOCK_MESSAGE));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_REVIEW_BUTTON));

  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockedUMA)),
              base::BucketsAre(base::Bucket(action, 1)));
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  data_controls::dlp::kFilesBlockedCountUMA),
              testing::ElementsAre(base::Bucket(3, 1)));
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMShowBlockTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kDownload,
                        IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCKED_TITLE),
        std::make_tuple(dlp::FileAction::kUpload,
                        IDS_POLICY_DLP_FILES_UPLOAD_BLOCKED_TITLE),
        std::make_tuple(dlp::FileAction::kOpen,
                        IDS_POLICY_DLP_FILES_OPEN_BLOCKED_TITLE),
        std::make_tuple(dlp::FileAction::kShare,
                        IDS_POLICY_DLP_FILES_OPEN_BLOCKED_TITLE),
        std::make_tuple(dlp::FileAction::kCopy,
                        IDS_POLICY_DLP_FILES_COPY_BLOCKED_TITLE),
        std::make_tuple(dlp::FileAction::kMove,
                        IDS_POLICY_DLP_FILES_MOVE_BLOCKED_TITLE),
        std::make_tuple(dlp::FileAction::kTransfer,
                        IDS_POLICY_DLP_FILES_TRANSFER_BLOCKED_TITLE)));

class FPNMShowWarningTest
    : public FilesPolicyNotificationManagerTest,
      public ::testing::WithParamInterface<dlp::FileAction> {
 protected:
  void SetUp() override { FilesPolicyNotificationManagerTest::SetUp(); }

  const base::HistogramTester histogram_tester_;
};

TEST_P(FPNMShowWarningTest, ShowDlpWarningNotification_Single) {
  auto action = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  auto src_file_path = base::FilePath(kFile1);
  testing::StrictMock<base::MockCallback<WarningWithJustificationCallback>>
      mock_cb;
  fpnm_->ShowDlpWarning(
      mock_cb.Get(), /*task_id=*/std::nullopt, {src_file_path},
      DlpFileDestination(GURL("https://example.com")), action);

  std::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), GetWarningTitle(action));
  EXPECT_EQ(notification->message(),
            base::ReplaceStringPlaceholders(
                l10n_util::GetPluralStringFUTF16(
                    IDS_POLICY_DLP_FILES_WARN_MESSAGE, 1),
                src_file_path.BaseName().LossyDisplayName(),
                /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title, GetWarningOkButton(action));

  // Warning callback is run with should_proceed set to false when the warning
  // times out.
  EXPECT_CALL(mock_cb,
              Run(/*user_justification=*/std::optional<std::u16string>(),
                  /*should_proceed=*/false))
      .Times(1);
  task_environment_.FastForwardBy(base::Minutes(5));

  VerifyFilesWarningUMAs(histogram_tester_,
                         /*action_warned_buckets=*/{base::Bucket(action, 1)},
                         /*warning_count_buckets=*/{base::Bucket(1, 1)},
                         /*action_timedout_buckets=*/{});
}

TEST_P(FPNMShowWarningTest, ShowDlpWarningNotification_Multi) {
  auto action = GetParam();

  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  testing::StrictMock<base::MockCallback<WarningWithJustificationCallback>>
      mock_cb;
  fpnm_->ShowDlpWarning(mock_cb.Get(), /*task_id=*/std::nullopt,
                        {base::FilePath(kFile1), base::FilePath(kFile2)},
                        DlpFileDestination(GURL("https://example.com")),
                        action);

  std::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), GetWarningTitle(action));
  EXPECT_EQ(
      notification->message(),
      base::ReplaceStringPlaceholders(l10n_util::GetPluralStringFUTF16(
                                          IDS_POLICY_DLP_FILES_WARN_MESSAGE, 2),
                                      u"2",
                                      /*offset=*/nullptr));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON));
  EXPECT_EQ(notification->buttons()[1].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_REVIEW_BUTTON));

  // Warning callback is run with should_proceed set to false when the warning
  // times out.
  EXPECT_CALL(mock_cb,
              Run(/*user_justification=*/std::optional<std::u16string>(),
                  /*should_proceed=*/false))
      .Times(1);
  task_environment_.FastForwardBy(base::Minutes(5));

  VerifyFilesWarningUMAs(histogram_tester_,
                         /*action_warned_buckets=*/{base::Bucket(action, 1)},
                         /*warning_count_buckets=*/{base::Bucket(2, 1)},
                         /*action_timedout_buckets=*/{});
}

INSTANTIATE_TEST_SUITE_P(PolicyFilesNotify,
                         FPNMShowWarningTest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kTransfer));

class FPNMShowTimeoutTest : public FilesPolicyNotificationManagerTest,
                            public ::testing::WithParamInterface<
                                std::tuple<dlp::FileAction, int, int>> {};

TEST_P(FPNMShowTimeoutTest, TimeoutErrorShowsTimeoutNotification) {
  auto [action, title_id, message_id] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  fpnm_->ShowDlpWarningTimeoutNotification(action,
                                           /*notification_id=*/std::nullopt);
  auto notification = display_service_tester.GetNotification(kNotificationId);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), l10n_util::GetStringUTF16(title_id));
  EXPECT_EQ(notification->message(), l10n_util::GetStringUTF16(message_id));
  EXPECT_EQ(notification->buttons()[0].title,
            l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DISMISS_BUTTON));
  EXPECT_TRUE(notification->never_timeout());
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMShowTimeoutTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kDownload,
                        IDS_POLICY_DLP_FILES_DOWNLOAD_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_DOWNLOAD_TIMEOUT_MESSAGE),
        std::make_tuple(dlp::FileAction::kTransfer,
                        IDS_POLICY_DLP_FILES_TRANSFER_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_TRANSFER_TIMEOUT_MESSAGE),
        std::make_tuple(dlp::FileAction::kUnknown,
                        IDS_POLICY_DLP_FILES_TRANSFER_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_TRANSFER_TIMEOUT_MESSAGE),
        std::make_tuple(dlp::FileAction::kUpload,
                        IDS_POLICY_DLP_FILES_UPLOAD_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_UPLOAD_TIMEOUT_MESSAGE),
        std::make_tuple(dlp::FileAction::kOpen,
                        IDS_POLICY_DLP_FILES_OPEN_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_OPEN_TIMEOUT_MESSAGE),
        std::make_tuple(dlp::FileAction::kShare,
                        IDS_POLICY_DLP_FILES_OPEN_TIMEOUT_TITLE,
                        IDS_POLICY_DLP_FILES_OPEN_TIMEOUT_MESSAGE)));

}  // namespace policy
