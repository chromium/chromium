// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include <string>

#include "ash/webui/file_manager/url_constants.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using testing::Field;

// The id of the first notification FPNM shows.
constexpr char kNotificationId[] = "dlp_files_0";

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42", sizeof("42")) == sizeof("42");
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
        new file_manager::FakeDiskMountManager);
    file_manager::VolumeManager* const volume_manager =
        file_manager::VolumeManager::Get(profile_);
    ASSERT_TRUE(volume_manager);
    io_task_controller_ = volume_manager->io_task_controller();
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

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal,
        base::FilePath::FromUTF8Unsafe(path));
  }

  // Creates and adds a CopyOrMoveIOTask with `task_id` with type
  // `OperationType::kCopy` if `is_copy` is true, and `OperationType::kMove` if
  // false.
  base::FilePath AddCopyOrMoveIOTask(file_manager::io_task::IOTaskId id,
                                     bool is_copy) {
    base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
    if (!CreateDummyFile(src_file_path)) {
      return base::FilePath();
    }
    auto src_url = CreateFileSystemURL(src_file_path.value());
    if (!src_url.is_valid()) {
      return base::FilePath();
    }
    auto dst_url = CreateFileSystemURL(temp_dir_.GetPath().value());

    auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
        is_copy ? file_manager::io_task::OperationType::kCopy
                : file_manager::io_task::OperationType::kCopy,
        std::vector<storage::FileSystemURL>({src_url}), dst_url, profile_,
        file_system_context_);

    io_task_controller_->Add(std::move(task));

    return src_file_path;
  }

 protected:
  std::unique_ptr<FilesPolicyNotificationManager> fpnm_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<file_manager::io_task::IOTaskController, DanglingUntriaged>
      io_task_controller_;
  content::BrowserTaskEnvironment task_environment_;
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
  pause_params.policy_params.emplace(Policy::kDlp);
  io_task_controller_->Pause(task_id, std::move(pause_params));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  // Once the task is complete, it should be removed.
  io_task_controller_->Cancel(task_id);
  EXPECT_FALSE(fpnm_->HasIOTask(task_id));
}

// Only Copy and move tasks are observed by FilesPolicyNotificationManager.
TEST_F(FilesPolicyNotificationManagerTest, AddTrashTask) {
  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
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

  std::string notification_id_1 = "dlp_files_0";
  std::string notification_id_2 = "dlp_files_1";
  std::string notification_id_3 = "dlp_files_2";

  std::vector<base::FilePath> files_1 = {base::FilePath("file1.txt"),
                                         base::FilePath("file2.txt"),
                                         base::FilePath("file3.txt")};

  // None are shown.
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_3));
  // Show first notification for upload.
  fpnm_->ShowDlpBlockedFiles(/*task_id=*/absl::nullopt, files_1,
                             dlp::FileAction::kUpload);
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_3));
  // Show another notification for the same action - should get a new ID.
  fpnm_->ShowDlpBlockedFiles(/*task_id=*/absl::nullopt, files_1,
                             dlp::FileAction::kUpload);
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id_3));
  // Show a notification for a different action & files - should still increment
  // the ID.
  fpnm_->ShowDlpBlockedFiles(
      /*task_id=*/absl::nullopt,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")},
      dlp::FileAction::kOpen);
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_1));
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_2));
  EXPECT_TRUE(display_service_tester.GetNotification(notification_id_3));
}

// Tests that passing task id to ShowDlpWarning will pause the corresponding
// IOTask.
TEST_F(FilesPolicyNotificationManagerTest, WarningPausesIOTask) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());
  auto dst_url = CreateFileSystemURL(temp_dir_.GetPath().value());

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src_url}), dst_url, profile_,
      file_system_context_);

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));
  io_task_controller_->Add(std::move(task));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(Policy::kDlp);

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

  fpnm_->ShowDlpWarning(
      base::DoNothing(), task_id, std::vector<base::FilePath>{src_file_path},
      DlpFileDestination(dst_url.path().value()), dlp::FileAction::kCopy);

  // Task is completed with error.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kError),
                Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::policy_error,
                      file_manager::io_task::PolicyErrorType::kDlp))))
      .Times(::testing::AtLeast(1));

  io_task_controller_->CompleteWithError(
      task_id, file_manager::io_task::PolicyErrorType::kDlp);

  base::RunLoop().RunUntilIdle();
  io_task_controller_->RemoveObserver(&observer);
}

// ShowDlpBlockedFiles updates IO task info.
TEST_F(FilesPolicyNotificationManagerTest, ShowDlpIOBlockedFiles) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());
  auto dst_url = CreateFileSystemURL(temp_dir_.GetPath().value());

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src_url}), dst_url, profile_,
      file_system_context_);

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));
  io_task_controller_->Add(std::move(task));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  fpnm_->ShowDlpBlockedFiles(task_id,
                             std::vector<base::FilePath>{src_file_path},
                             dlp::FileAction::kCopy);

  std::map<DlpConfidentialFile, Policy> expected_blocked_files{
      {DlpConfidentialFile(src_file_path), Policy::kDlp}};

  EXPECT_EQ(fpnm_->GetIOTaskBlockedFilesForTesting(task_id),
            expected_blocked_files);

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
}

// Tests that cancelling a paused IO task will run the warning callback.
TEST_F(FilesPolicyNotificationManagerTest, WarningCancelled) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());
  auto dst_url = CreateFileSystemURL(temp_dir_.GetPath().value());

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src_url}), dst_url, profile_,
      file_system_context_);

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));
  io_task_controller_->Add(std::move(task));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(Policy::kDlp);

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
  testing::StrictMock<base::MockCallback<OnDlpRestrictionCheckedCallback>>
      mock_cb;
  fpnm_->ShowDlpWarning(
      mock_cb.Get(), task_id, std::vector<base::FilePath>{src_file_path},
      DlpFileDestination(dst_url.path().value()), dlp::FileAction::kCopy);

  // Task is cancelled.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kCancelled))));
  // Warning callback is run with should_proceed set to false when the task is
  // cancelled.
  EXPECT_CALL(mock_cb, Run(/*should_proceed=*/false)).Times(1);
  io_task_controller_->Cancel(task_id);

  base::RunLoop().RunUntilIdle();
  io_task_controller_->RemoveObserver(&observer);
}

// Tests that resuming a paused IO task will run the warning callback.
TEST_F(FilesPolicyNotificationManagerTest, WarningResumed) {
  IOTaskStatusObserver observer;
  io_task_controller_->AddObserver(&observer);

  file_manager::io_task::IOTaskId task_id = 1;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(src_file_path));
  auto src_url = CreateFileSystemURL(src_file_path.value());
  ASSERT_TRUE(src_url.is_valid());
  auto dst_url = CreateFileSystemURL(temp_dir_.GetPath().value());

  auto task = std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
      file_manager::io_task::OperationType::kCopy,
      std::vector<storage::FileSystemURL>({src_url}), dst_url, profile_,
      file_system_context_);

  // Task is queued.
  EXPECT_CALL(
      observer,
      OnIOTaskStatus(
          AllOf(Field(&file_manager::io_task::ProgressStatus::task_id, task_id),
                Field(&file_manager::io_task::ProgressStatus::state,
                      file_manager::io_task::State::kQueued))));
  io_task_controller_->Add(std::move(task));
  EXPECT_TRUE(fpnm_->HasIOTask(task_id));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(Policy::kDlp);

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

  testing::StrictMock<base::MockCallback<OnDlpRestrictionCheckedCallback>>
      mock_cb;
  fpnm_->ShowDlpWarning(
      mock_cb.Get(), task_id, std::vector<base::FilePath>{src_file_path},
      DlpFileDestination(dst_url.path().value()), dlp::FileAction::kCopy);

  // Warning callback is run with should_proceed set to true when the task is
  // resumed.
  EXPECT_CALL(mock_cb, Run(/*should_proceed=*/true)).Times(1);
  fpnm_->OnIOTaskResumed(task_id);
}

class FPNMPausedStatusNotification
    : public FilesPolicyNotificationManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType,
                     Policy,
                     std::u16string,
                     std::u16string>> {};

TEST_P(FPNMPausedStatusNotification, PausedShowsWarningNotification_Single) {
  auto [type, policy, title, ok_button] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kPaused;
  status.type = type;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_FALSE(src_file_path.empty());
  status.sources.emplace_back(CreateFileSystemURL(src_file_path.value()),
                              absl::nullopt);
  status.pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(policy);

  fpnm_->ShowsFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"File may contain sensitive content");
  EXPECT_EQ(notification->buttons()[0].title, u"Cancel");
  EXPECT_EQ(notification->buttons()[1].title, ok_button);
  EXPECT_TRUE(notification->never_timeout());
}

TEST_P(FPNMPausedStatusNotification, PausedShowsWarningNotification_Multi) {
  auto [type, policy, title, ok_button] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kPaused;
  status.type = type;
  base::FilePath src_file_path_1 = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_FALSE(src_file_path_1.empty());
  base::FilePath src_file_path_2 = temp_dir_.GetPath().AppendASCII("test2.txt");
  ASSERT_FALSE(src_file_path_2.empty());
  status.sources.emplace_back(CreateFileSystemURL(src_file_path_1.value()),
                              absl::nullopt);
  status.sources.emplace_back(CreateFileSystemURL(src_file_path_2.value()),
                              absl::nullopt);
  status.pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(policy);

  fpnm_->ShowsFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"Files may contain sensitive content");
  EXPECT_EQ(notification->buttons()[0].title, u"Cancel");
  EXPECT_EQ(notification->buttons()[1].title, u"Review");
  EXPECT_TRUE(notification->never_timeout());
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMPausedStatusNotification,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        Policy::kDlp,
                        u"Review is required before copying",
                        u"Copy anyway"),
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        Policy::kEnterpriseConnectors,
                        u"Review is required before copying",
                        u"Copy anyway"),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        Policy::kDlp,
                        u"Review is required before moving",
                        u"Move anyway"),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        Policy::kEnterpriseConnectors,
                        u"Review is required before moving",
                        u"Move anyway")));

class FPNMErrorStatusNotification
    : public FilesPolicyNotificationManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<file_manager::io_task::OperationType,
                     file_manager::io_task::PolicyErrorType,
                     std::u16string>> {};

TEST_P(FPNMErrorStatusNotification, ErrorShowsBlockNotification_Single) {
  auto [type, policy, title] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kError;
  status.type = type;
  base::FilePath src_file_path = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_FALSE(src_file_path.empty());
  status.sources.emplace_back(CreateFileSystemURL(src_file_path.value()),
                              absl::nullopt);
  status.policy_error = policy;

  fpnm_->ShowsFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"File was blocked");
  EXPECT_EQ(notification->buttons()[0].title, u"Dismiss");
  EXPECT_EQ(notification->buttons()[1].title, u"Learn more");
  EXPECT_TRUE(notification->never_timeout());
}

TEST_P(FPNMErrorStatusNotification, ErrorShowsBlockNotification_Multi) {
  auto [type, policy, title] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());
  const std::string notification_id = "notification_id";
  EXPECT_FALSE(display_service_tester.GetNotification(notification_id));

  file_manager::io_task::ProgressStatus status;
  status.task_id = 1;
  status.state = file_manager::io_task::State::kError;
  status.type = type;
  base::FilePath src_file_path_1 = temp_dir_.GetPath().AppendASCII("test1.txt");
  ASSERT_FALSE(src_file_path_1.empty());
  base::FilePath src_file_path_2 = temp_dir_.GetPath().AppendASCII("test2.txt");
  ASSERT_FALSE(src_file_path_2.empty());
  status.sources.emplace_back(CreateFileSystemURL(src_file_path_1.value()),
                              absl::nullopt);
  status.sources.emplace_back(CreateFileSystemURL(src_file_path_2.value()),
                              absl::nullopt);
  status.policy_error = policy;

  fpnm_->ShowsFilesPolicyNotification(notification_id, status);
  auto notification = display_service_tester.GetNotification(notification_id);
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"Review for further details");
  EXPECT_EQ(notification->buttons()[0].title, u"Dismiss");
  EXPECT_EQ(notification->buttons()[1].title, u"Review");
  EXPECT_TRUE(notification->never_timeout());
}

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMErrorStatusNotification,
    ::testing::Values(
        std::make_tuple(file_manager::io_task::OperationType::kCopy,
                        file_manager::io_task::PolicyErrorType::kDlp,
                        u"Blocked copy"),
        std::make_tuple(
            file_manager::io_task::OperationType::kCopy,
            file_manager::io_task::PolicyErrorType::kEnterpriseConnectors,
            u"Blocked copy"),
        std::make_tuple(file_manager::io_task::OperationType::kMove,
                        file_manager::io_task::PolicyErrorType::kDlp,
                        u"Blocked move"),
        std::make_tuple(
            file_manager::io_task::OperationType::kMove,
            file_manager::io_task::PolicyErrorType::kEnterpriseConnectors,
            u"Blocked move")));

class FPNMShowBlockTest : public FilesPolicyNotificationManagerTest,
                          public ::testing::WithParamInterface<
                              std::tuple<dlp::FileAction, std::u16string>> {
  void SetUp() override {
    FilesPolicyNotificationManagerTest::SetUp();
    DlpFilesController::SetNewFilesPolicyUXEnabledForTesting(
        /*is_enabled=*/true);
  }
};

class FPNMShowWarningTest
    : public FilesPolicyNotificationManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<dlp::FileAction, std::u16string, std::u16string>> {
  void SetUp() override {
    FilesPolicyNotificationManagerTest::SetUp();
    DlpFilesController::SetNewFilesPolicyUXEnabledForTesting(
        /*is_enabled=*/true);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMShowBlockTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kDownload, u"Blocked download"),
        std::make_tuple(dlp::FileAction::kUpload, u"Blocked upload"),
        std::make_tuple(dlp::FileAction::kOpen, u"Blocked open"),
        std::make_tuple(dlp::FileAction::kShare, u"Blocked open"),
        std::make_tuple(dlp::FileAction::kCopy, u"Blocked copy"),
        std::make_tuple(dlp::FileAction::kMove, u"Blocked move"),
        std::make_tuple(dlp::FileAction::kTransfer, u"Blocked transfer")));

INSTANTIATE_TEST_SUITE_P(
    PolicyFilesNotify,
    FPNMShowWarningTest,
    ::testing::Values(std::make_tuple(dlp::FileAction::kDownload,
                                      u"Review is required before downloading",
                                      u"Download anyway"),
                      std::make_tuple(dlp::FileAction::kUpload,
                                      u"Review is required before uploading",
                                      u"Upload anyway"),
                      std::make_tuple(dlp::FileAction::kOpen,
                                      u"Review is required before opening",
                                      u"Open anyway"),
                      std::make_tuple(dlp::FileAction::kShare,
                                      u"Review is required before opening",
                                      u"Open anyway"),
                      std::make_tuple(dlp::FileAction::kCopy,
                                      u"Review is required before copying",
                                      u"Copy anyway"),
                      std::make_tuple(dlp::FileAction::kMove,
                                      u"Review is required before moving",
                                      u"Move anyway"),
                      std::make_tuple(dlp::FileAction::kTransfer,
                                      u"Review is required before transferring",
                                      u"Transfer anyway")));

TEST_P(FPNMShowBlockTest, ShowDlpBlockNotification_Single) {
  auto [action, title] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  fpnm_->ShowDlpBlockedFiles(/*task_id=*/absl::nullopt,
                             {base::FilePath("file1.txt")}, action);
  absl::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"File was blocked");
  EXPECT_EQ(notification->buttons()[0].title, u"Dismiss");
  EXPECT_EQ(notification->buttons()[1].title, u"Learn more");
}

TEST_P(FPNMShowBlockTest, ShowDlpBlockNotification_Multi) {
  auto [action, title] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  fpnm_->ShowDlpBlockedFiles(
      /*task_id=*/absl::nullopt,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt"),
       base::FilePath("file3.txt")},
      action);
  absl::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"Review for further details");
  EXPECT_EQ(notification->buttons()[0].title, u"Dismiss");
  EXPECT_EQ(notification->buttons()[1].title, u"Review");
}

TEST_P(FPNMShowWarningTest, ShowDlpWarningNotification_Single) {
  auto [action, title, ok_button] = GetParam();
  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  fpnm_->ShowDlpWarning(base::DoNothing(), /*task_id=*/absl::nullopt,
                        {base::FilePath("file1.txt")},
                        DlpFileDestination("https://example.com"), action);
  absl::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"File may contain sensitive content");
  EXPECT_EQ(notification->buttons()[0].title, u"Cancel");
  EXPECT_EQ(notification->buttons()[1].title, ok_button);
}

TEST_P(FPNMShowWarningTest, ShowDlpWarningNotification_Multi) {
  auto [action, title, ok_button] = GetParam();

  NotificationDisplayServiceTester display_service_tester(profile_.get());

  EXPECT_FALSE(display_service_tester.GetNotification(kNotificationId));
  fpnm_->ShowDlpWarning(
      base::DoNothing(), /*task_id=*/absl::nullopt,
      {base::FilePath("file1.txt"), base::FilePath("file2.txt")},
      DlpFileDestination("https://example.com"), action);
  absl::optional<message_center::Notification> notification =
      display_service_tester.GetNotification(kNotificationId);
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), title);
  EXPECT_EQ(notification->message(), u"Files may contain sensitive content");
  EXPECT_EQ(notification->buttons()[0].title, u"Cancel");
  EXPECT_EQ(notification->buttons()[1].title, u"Review");
}

}  // namespace policy
