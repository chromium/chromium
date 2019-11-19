// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

namespace crostini {

struct ExportProgressOptionalArguments {
  int progress_percent{};  // TODO(juwa): remove this once tremplin has been
                           // shipped.
  uint32_t total_files{};
  uint64_t total_bytes{};
  uint32_t files_streamed{};
  uint64_t bytes_streamed{};
};

struct ImportProgressOptionalArguments {
  int progress_percent{};
  uint64_t available_space{};
  uint64_t min_required_space{};
};

class CrostiniExportImportTest : public testing::Test {
 public:
  void SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status status,
      const ExportProgressOptionalArguments& arguments = {}) {
    vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(kCrostiniDefaultVmName);
    signal.set_container_name(kCrostiniDefaultContainerName);
    signal.set_status(status);
    signal.set_progress_percent(arguments.progress_percent);
    signal.set_total_input_files(arguments.total_files);
    signal.set_total_input_bytes(arguments.total_bytes);
    signal.set_input_files_streamed(arguments.files_streamed);
    signal.set_input_bytes_streamed(arguments.bytes_streamed);
    fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);
  }

  void SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status status,
      const ImportProgressOptionalArguments& arguments = {}) {
    vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(kCrostiniDefaultVmName);
    signal.set_container_name(kCrostiniDefaultContainerName);
    signal.set_status(status);
    signal.set_progress_percent(arguments.progress_percent);
    signal.set_architecture_device("arch_dev");
    signal.set_architecture_container("arch_con");
    signal.set_available_space(arguments.available_space);
    signal.set_min_required_space(arguments.min_required_space);
    fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);
  }

  CrostiniExportImportTest()
      : container_id_(kCrostiniDefaultVmName, kCrostiniDefaultContainerName) {
    chromeos::DBusThreadManager::Initialize();
    fake_seneschal_client_ = static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
  }

  ~CrostiniExportImportTest() override {
    chromeos::DBusThreadManager::Shutdown();
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    crostini_export_import_ = std::make_unique<CrostiniExportImport>(profile());
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        kCrostiniDefaultVmName);
    CrostiniManager::GetForProfile(profile())->set_skip_restart_for_testing();
    profile()->GetPrefs()->SetBoolean(
        crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, true);

    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    mount_points->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        file_manager::util::GetMyFilesFolderForProfile(profile()));
    tarball_ = file_manager::util::GetMyFilesFolderForProfile(profile()).Append(
        "crostini_export_import_unittest_tarball.tar.gz");
  }

  void TearDown() override {
    crostini_export_import_.reset();
    // If the file has been created (by an export), then delete it, but first
    // shutdown GuestOsSharePath to ensure watchers are destroyed, otherwise
    // they can trigger and execute against a destroyed service.
    guest_os::GuestOsSharePath::GetForProfile(profile())->Shutdown();
    task_environment_.RunUntilIdle();
    base::DeleteFile(tarball_, false);
    test_helper_.reset();
    profile_.reset();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;
  chromeos::FakeSeneschalClient* fake_seneschal_client_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniExportImport> crostini_export_import_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;

  ContainerId container_id_;
  base::FilePath tarball_;

  content::BrowserTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniExportImportTest);
};

TEST_F(CrostiniExportImportTest, TestNotAllowed) {
  profile()->GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, false);
  crostini_export_import_->ExportContainer(
      container_id_, tarball_, base::BindOnce([](CrostiniResult result) {
        EXPECT_EQ(result, CrostiniResult::NOT_ALLOWED);
      }));
  crostini_export_import_->ImportContainer(
      container_id_, tarball_, base::BindOnce([](CrostiniResult result) {
        EXPECT_EQ(result, CrostiniResult::NOT_ALLOWED);
      }));
}

// TODO(juwa): remove this once tremplin has been shipped.
TEST_F(CrostiniExportImportTest, TestDeprecatedExportSuccess) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // 20% PACK = 10% overall.
  SendExportProgress(vm_tools::cicerone::
                         ExportLxdContainerProgressSignal_Status_EXPORTING_PACK,
                     {.progress_percent = 20});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 10);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // 20% DOWNLOAD = 60% overall.
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_DOWNLOAD,
      {.progress_percent = 20});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // Close notification and update progress. Should not update notification.
  notification->Close(false);
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_DOWNLOAD,
      {.progress_percent = 40});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // Done.
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::DONE);
  EXPECT_FALSE(notification->get_notification()->pinned());
  // CrostiniExportImport should've created the exported file.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportSuccess) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // STREAMING 10% bytes done + 30% files done = 20% overall.
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 30,
       .bytes_streamed = 10});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 20);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // STREAMING 66% bytes done + 55% files done then floored = 60% overall.
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 55,
       .bytes_streamed = 66});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // Close notification and update progress. Should not update notification.
  notification->Close(false);
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 90,
       .bytes_streamed = 85});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // Done.
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::DONE);
  EXPECT_FALSE(notification->get_notification()->pinned());
  // CrostiniExportImport should've created the exported file.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportFail) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);

  // Failed.
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::FAILED_UNKNOWN_REASON);
  EXPECT_FALSE(notification->get_notification()->pinned());
  // CrostiniExportImport should cleanup the file if an export fails.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportCancelled) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::EXPORT,
                                           container_id_);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::CANCELLING);
  EXPECT_EQ(notification->get_notification()->progress(), -1);
  EXPECT_FALSE(notification->get_notification()->pinned());
  EXPECT_TRUE(base::PathExists(tarball_));

  // STREAMING: should not be displayed as cancel is in progress
  SendExportProgress(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 50,
       .bytes_streamed = 50});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::CANCELLING);
  EXPECT_EQ(notification->get_notification()->progress(), -1);
  EXPECT_FALSE(notification->get_notification()->pinned());
  EXPECT_TRUE(base::PathExists(tarball_));

  // CANCELLED:
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_CANCELLED);
  EXPECT_FALSE(
      crostini_export_import_->GetNotificationForTesting(container_id_));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportDoneBeforeCancelled) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::EXPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::EXPORT,
                                           container_id_);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::CANCELLING);
  EXPECT_EQ(notification->get_notification()->progress(), -1);
  EXPECT_FALSE(notification->get_notification()->pinned());
  EXPECT_TRUE(base::PathExists(tarball_));

  // DONE: Completed before cancel processed, file should be deleted.
  SendExportProgress(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  EXPECT_FALSE(
      crostini_export_import_->GetNotificationForTesting(container_id_));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestImportSuccess) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // 20% UPLOAD = 10% overall.
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD,
      {.progress_percent = 20});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 10);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // 20% UNPACK = 60% overall.
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      {.progress_percent = 20});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // Close notification and update progress. Should not update notification.
  notification->Close(false);
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      {.progress_percent = 40});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 60);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // Done.
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::DONE);
  EXPECT_FALSE(notification->get_notification()->pinned());
}

TEST_F(CrostiniExportImportTest, TestImportFail) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);

  // Failed.
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::FAILED_UNKNOWN_REASON);
  EXPECT_FALSE(notification->get_notification()->pinned());
  std::string msg("Restoring couldn't be completed due to an error");
  EXPECT_EQ(notification->get_notification()->message(),
            base::UTF8ToUTF16(msg));
}

TEST_F(CrostiniExportImportTest, TestImportCancelled) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::IMPORT,
                                           container_id_);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::CANCELLING);
  EXPECT_EQ(notification->get_notification()->progress(), -1);
  EXPECT_FALSE(notification->get_notification()->pinned());

  // STREAMING: should not be displayed as cancel is in progress
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD,
      {.progress_percent = 50});
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::CANCELLING);
  EXPECT_EQ(notification->get_notification()->progress(), -1);
  EXPECT_FALSE(notification->get_notification()->pinned());

  // CANCELLED:
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_CANCELLED);
  EXPECT_FALSE(
      crostini_export_import_->GetNotificationForTesting(container_id_));
}

TEST_F(CrostiniExportImportTest, TestImportDoneBeforeCancelled) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::RUNNING);
  EXPECT_EQ(notification->get_notification()->progress(), 0);
  EXPECT_TRUE(notification->get_notification()->pinned());

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::IMPORT,
                                           container_id_);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::CANCELLING);
  EXPECT_EQ(notification->get_notification()->progress(), -1);
  EXPECT_FALSE(notification->get_notification()->pinned());

  // DONE: Cancel couldn't be processed in time, done is displayed instead.
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(notification->status(),
            CrostiniExportImportNotification::Status::DONE);
  EXPECT_FALSE(notification->get_notification()->pinned());
}

TEST_F(CrostiniExportImportTest, TestImportFailArchitecture) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);

  // Failed Architecture.
  SendImportProgress(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_FAILED_ARCHITECTURE);
  EXPECT_EQ(
      notification->status(),
      CrostiniExportImportNotification::Status::FAILED_ARCHITECTURE_MISMATCH);
  EXPECT_FALSE(notification->get_notification()->pinned());
  std::string msg(
      "Cannot import container architecture type arch_con with this device "
      "which is arch_dev. You can try restoring this container into a "
      "different device, or you can access the files inside this container "
      "image by opening in Files app.");
  EXPECT_EQ(notification->get_notification()->message(),
            base::UTF8ToUTF16(msg));
}

TEST_F(CrostiniExportImportTest, TestImportFailSpace) {
  crostini_export_import_->FileSelected(
      tarball_, 0, reinterpret_cast<void*>(ExportImportType::IMPORT));
  task_environment_.RunUntilIdle();
  CrostiniExportImportNotification* notification =
      crostini_export_import_->GetNotificationForTesting(container_id_);

  // Failed Space.
  SendImportProgress(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED_SPACE,
      {
          .available_space = 20ul * 1'024 * 1'024 * 1'024,    // 20Gb
          .min_required_space = 35ul * 1'024 * 1'024 * 1'024  // 35Gb
      });
  EXPECT_EQ(
      notification->status(),
      CrostiniExportImportNotification::Status::FAILED_INSUFFICIENT_SPACE);
  EXPECT_FALSE(notification->get_notification()->pinned());
  std::string msg =
      "Cannot restore due to lack of storage space. Free up 15.0 GB from the "
      "device and try again.";
  EXPECT_EQ(notification->get_notification()->message(),
            base::UTF8ToUTF16(msg));
}

}  // namespace crostini
