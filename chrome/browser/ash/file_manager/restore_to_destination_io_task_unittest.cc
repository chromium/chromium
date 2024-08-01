// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_to_destination_io_task.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_unittest_base.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom-forward.h"
#include "chromeos/ash/components/trash_service/trash_service_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Field;

class RestoreToDestinationIOTaskTest : public TrashBaseTest {
 public:
  RestoreToDestinationIOTaskTest() = default;

  RestoreToDestinationIOTaskTest(const RestoreToDestinationIOTaskTest&) =
      delete;
  RestoreToDestinationIOTaskTest& operator=(
      const RestoreToDestinationIOTaskTest&) = delete;

  void SetUp() override {
    TrashBaseTest::SetUp();

    // The TrashService launches a sandboxed process to perform parsing in, in
    // unit tests this is not possible. So instead override the launcher to
    // start an in-process TrashService and have `LaunchTrashService` invoke it.
    ash::trash_service::SetTrashServiceLaunchOverrideForTesting(
        base::BindRepeating(
            &RestoreToDestinationIOTaskTest::CreateInProcessTrashService,
            base::Unretained(this)));

    // Setup the destination directory where the files will be restored to.
    destination_path_ = temp_dir_.GetPath().Append("dest_folder");
    ASSERT_TRUE(base::CreateDirectory(destination_path_));
  }

  // Writes `contents` to `file` in the trash directory and sets up required
  // trash info based on `restore_file_name`. Returns the FilePath of the
  // .trashinfo file.
  base::FilePath WriteFileInTrash(const std::string& file_name,
                                  const std::string& restore_file_name,
                                  const std::string& contents) {
    std::string foo_metadata_contents =
        GenerateTrashInfoContents(restore_file_name);
    const base::FilePath trash_path =
        downloads_dir_.Append(trash::kTrashFolderName);
    const base::FilePath info_file_path =
        trash_path.Append(trash::kInfoFolderName)
            .Append(base::StrCat({file_name, ".trashinfo"}));
    CHECK(base::WriteFile(info_file_path, foo_metadata_contents));

    const base::FilePath files_path =
        trash_path.Append(trash::kFilesFolderName).Append(file_name);
    CHECK(base::WriteFile(files_path, contents));

    return files_path;
  }

  // Returns the restore destination URL.
  storage::FileSystemURL GetDestination() {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        destination_path_.BaseName());
  }

  // Verifies that the `file` is restored to the `destination_path_` and that
  // its contents match `expected_contents`.
  void VerifyRestoredFile(const std::string& file,
                          const std::string& expected_contents) {
    auto file_path = destination_path_.Append(file);
    EXPECT_TRUE(base::PathExists(file_path));
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(file_path, &contents));
    EXPECT_EQ(expected_contents, contents);
  }

 private:
  mojo::PendingRemote<ash::trash_service::mojom::TrashService>
  CreateInProcessTrashService() {
    mojo::PendingRemote<ash::trash_service::mojom::TrashService> remote;
    trash_service_impl_ =
        std::make_unique<ash::trash_service::TrashServiceImpl>(
            remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  std::string GenerateTrashInfoContents(const std::string& restore_file) {
    return base::StrCat({"[Trash Info]\nPath=", "/Downloads/bar/", restore_file,
                         "\nDeletionDate=",
                         base::TimeFormatAsIso8601(base::Time::UnixEpoch())});
  }

  // Maintains ownership of the in-process parsing service.
  std::unique_ptr<ash::trash_service::TrashServiceImpl> trash_service_impl_;
  // Directory where the files will be restored to.
  base::FilePath destination_path_;
};

TEST_F(RestoreToDestinationIOTaskTest,
       RestorePathWithDifferentNameInTrashInfoSucceeds) {
  EnsureTrashDirectorySetup(downloads_dir_);

  const std::string file_name = "foo.txt";
  const std::string restore_file_name = "baz.txt";
  // Setup the contents for files in the Trash directory.
  const std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath info_file_path =
      WriteFileInTrash(file_name, restore_file_name, foo_contents);

  // Setup source and destination locations.
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreToDestinationIOTask task(source_urls, GetDestination(), profile_.get(),
                                  file_system_context_, temp_dir_.GetPath(),
                                  /*show_notification=*/true);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  // The underlying move task should have the same ID.
  ASSERT_TRUE(task.GetMoveTaskForTesting());
  EXPECT_EQ(task.progress().task_id,
            task.GetMoveTaskForTesting()->progress().task_id);

  VerifyRestoredFile(restore_file_name, foo_contents);
}

TEST_F(RestoreToDestinationIOTaskTest, PauseAndResume) {
  EnsureTrashDirectorySetup(downloads_dir_);

  const std::string file_name_1 = "foo1.txt";
  const std::string file_name_2 = "foo2.txt";
  const std::string restore_file_name_1 = "baz1.txt";
  const std::string restore_file_name_2 = "baz2.txt";
  // Setup the contents for files in the Trash directory.
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath info_file_path_1 =
      WriteFileInTrash(file_name_1, restore_file_name_1, foo_contents);
  const base::FilePath info_file_path_2 =
      WriteFileInTrash(file_name_2, restore_file_name_2, foo_contents);

  // Setup source and destination locations.
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path_1),
      CreateFileSystemURL(info_file_path_2),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  RestoreToDestinationIOTask task(source_urls, GetDestination(), profile_.get(),
                                  file_system_context_, temp_dir_.GetPath(),
                                  /*show_notification=*/true);

  // Expect an in progress status and pause.
  EXPECT_CALL(progress_callback,
              Run(Field(&ProgressStatus::state, State::kInProgress)))
      .WillOnce([&task]() {
        PauseParams pause_params;
        pause_params.conflict_params.emplace();
        task.Pause(std::move(pause_params));

        ASSERT_TRUE(task.GetMoveTaskForTesting());
        EXPECT_TRUE(task.progress().IsPaused());
        EXPECT_TRUE(task.GetMoveTaskForTesting()->progress().IsPaused());
      });
  // Expect a paused status and resume.
  EXPECT_CALL(progress_callback,
              Run(Field(&ProgressStatus::state, State::kPaused)))
      .WillOnce([&task]() {
        ResumeParams resume_params;
        resume_params.conflict_params.emplace();
        resume_params.conflict_params->conflict_resolve = "replace";
        task.Resume(std::move(resume_params));
      });
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  VerifyRestoredFile(restore_file_name_1, foo_contents);
  VerifyRestoredFile(restore_file_name_2, foo_contents);
}

// Tests RestoreToDestinationIOTasks with DLP enabled.
class RestoreToDestinationIOTaskWithDLPTest
    : public RestoreToDestinationIOTaskTest {
 public:
  RestoreToDestinationIOTaskWithDLPTest() = default;

  RestoreToDestinationIOTaskWithDLPTest(
      const RestoreToDestinationIOTaskWithDLPTest&) = delete;
  RestoreToDestinationIOTaskWithDLPTest& operator=(
      const RestoreToDestinationIOTaskWithDLPTest&) = delete;

  void SetUp() override {
    RestoreToDestinationIOTaskTest::SetUp();

    // Setup IOTaskController. Needed for FPNM to control the IO tasks.
    file_manager::VolumeManager* const volume_manager =
        file_manager::VolumeManager::Get(profile_.get());
    ASSERT_TRUE(volume_manager);
    io_task_controller_ = volume_manager->io_task_controller();
    ASSERT_TRUE(io_task_controller_);

    // Setup DLP.
    scoped_feature_list_.InitAndEnableFeature(features::kNewFilesPolicyUX);
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(
            &RestoreToDestinationIOTaskWithDLPTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_TRUE(mock_rules_manager_);
    ASSERT_NE(policy::DlpRulesManagerFactory::GetForPrimaryProfile()
                  ->GetDlpFilesController(),
              nullptr);

    fpnm_ = std::make_unique<policy::FilesPolicyNotificationManager>(
        profile_.get());
  }

  void TearDown() override {
    files_controller_.reset();
    io_task_controller_ = nullptr;
    mock_rules_manager_ = nullptr;
    RestoreToDestinationIOTaskTest::TearDown();
  }

 protected:
  std::unique_ptr<policy::MockDlpFilesControllerAsh> files_controller_;
  std::unique_ptr<policy::FilesPolicyNotificationManager> fpnm_;
  raw_ptr<file_manager::io_task::IOTaskController> io_task_controller_ =
      nullptr;

 private:
  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<policy::MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillRepeatedly(testing::Return(true));

    files_controller_ = std::make_unique<policy::MockDlpFilesControllerAsh>(
        *mock_rules_manager_, Profile::FromBrowserContext(context));
    EXPECT_CALL(*mock_rules_manager_, GetDlpFilesController())
        .WillRepeatedly(::testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<policy::MockDlpRulesManager> mock_rules_manager_ = nullptr;
};

TEST_F(RestoreToDestinationIOTaskWithDLPTest, PauseResume) {
  EnsureTrashDirectorySetup(downloads_dir_);

  const std::string file_name = "foo.txt";
  const std::string restore_file_name = "baz.txt";
  // Setup the contents for files in the Trash directory.
  const std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath info_file_path =
      WriteFileInTrash(file_name, restore_file_name, foo_contents);

  // Setup source and destination locations.
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback,
              Run(Field(&ProgressStatus::state, State::kPaused)));
  EXPECT_CALL(progress_callback,
              Run(Field(&ProgressStatus::state, State::kInProgress)));
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  auto task_ptr = std::make_unique<RestoreToDestinationIOTask>(
      source_urls, GetDestination(), profile_.get(), file_system_context_,
      temp_dir_.GetPath(),
      /*show_notification=*/true);
  auto* task = task_ptr.get();

  io_task_controller_->Add(std::move(task_ptr));
  ASSERT_TRUE(fpnm_->HasIOTask(task->progress().task_id));

  // Set the DLP to warn, which pauses the task, and then resume.
  EXPECT_CALL(*files_controller_, CheckIfTransferAllowed)
      .WillOnce(
          ([=, this](
               std::optional<file_manager::io_task::IOTaskId> task_id,
               const std::vector<storage::FileSystemURL>& transferred_files,
               storage::FileSystemURL destination, bool is_move,
               policy::DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                   result_callback) {
            std::vector<base::FilePath> warning_files;
            for (const auto& file : transferred_files) {
              warning_files.emplace_back(file.path());
            }
            fpnm_->ShowDlpWarning(
                base::DoNothing(), task_id, std::move(warning_files),
                policy::DlpFileDestination(), policy::dlp::FileAction::kMove);

            auto* move_task = task->GetMoveTaskForTesting();
            ASSERT_TRUE(move_task);
            EXPECT_TRUE(move_task->progress().IsPaused());
            EXPECT_TRUE(task->progress().IsPaused());
            // Resume.
            ResumeParams params;
            params.policy_params.emplace(policy::Policy::kDlp);
            io_task_controller_->Resume(task_id.value(), std::move(params));
            // Also run the callback with no blocked files, which will actually
            // start the transfer and set the correct state.
            std::move(result_callback).Run({});

            EXPECT_FALSE(move_task->progress().IsPaused());
            EXPECT_FALSE(task->progress().IsPaused());
          }));

  task->Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  VerifyRestoredFile(restore_file_name, foo_contents);
}

}  // namespace
}  // namespace file_manager::io_task
