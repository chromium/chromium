// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_export_import.h"

#include <cstdint>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_service.pb.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace crostini {

struct ExportProgressOptionalArguments {
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
  base::WeakPtr<CrostiniExportImportNotificationController> GetController(
      const guest_os::GuestId& container_id) {
    return crostini_export_import_->GetNotificationControllerForTesting(
        container_id);
  }

  const message_center::Notification& GetNotification(
      const guest_os::GuestId& container_id) {
    // Assertions in this function are wrap in IILEs because you cannot assert
    // in a function with a non-void return type.
    const base::WeakPtr<CrostiniExportImportNotificationController>&
        controller = GetController(container_id);
    [&] { ASSERT_NE(controller, nullptr); }();
    const message_center::Notification* controller_notification =
        controller->get_notification();
    [&] { ASSERT_NE(controller_notification, nullptr); }();
    const std::optional<message_center::Notification>& ui_notification =
        notification_display_service_->GetNotification(
            controller_notification->id());
    [&] { ASSERT_NE(ui_notification, std::nullopt); }();
    // The controller notification is stored on the
    // CrostiniExportImportNotificationController, but copied into the
    // message_center's storage whenever it changes. If they could share the
    // same instance of the notification then this function wouldn't be
    // necessary.
    [&] { ASSERT_NE(controller_notification, &*ui_notification); }();
    [&] {
      ASSERT_TRUE(
          controller_notification->type() == ui_notification->type() &&
          controller_notification->id() == ui_notification->id() &&
          controller_notification->title() == ui_notification->title() &&
          controller_notification->message() == ui_notification->message() &&
          controller_notification->timestamp() ==
              ui_notification->timestamp() &&
          controller_notification->progress() == ui_notification->progress() &&
          controller_notification->never_timeout() ==
              ui_notification->never_timeout() &&
          controller_notification->delegate() == ui_notification->delegate());
    }();
    // Either notification could be returned here, they are fungible.
    return *controller_notification;
  }

  void SendExportProgress(
      const guest_os::GuestId& container_id,
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status status,
      const ExportProgressOptionalArguments& arguments = {}) {
    vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(container_id.vm_name);
    signal.set_container_name(container_id.container_name);
    signal.set_status(status);
    signal.set_total_input_files(arguments.total_files);
    signal.set_total_input_bytes(arguments.total_bytes);
    signal.set_input_files_streamed(arguments.files_streamed);
    signal.set_input_bytes_streamed(arguments.bytes_streamed);
    ash::FakeCiceroneClient::Get()->NotifyExportLxdContainerProgress(signal);
  }

  void SendDiskImageProgress(const guest_os::GuestId& container_id,
                             vm_tools::concierge::DiskImageStatus status,
                             uint32_t progress) {
    vm_tools::concierge::DiskImageStatusResponse signal;
    signal.set_status(status);
    signal.set_progress(progress);
    ash::FakeConciergeClient::Get()->NotifyDiskImageProgress(signal);
  }

  void SetImportResponse() {
    vm_tools::concierge::ImportDiskImageResponse response;
    response.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
    ash::FakeConciergeClient::Get()->set_import_disk_image_response(response);
  }

  void SendImportProgress(
      const guest_os::GuestId& container_id,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status status,
      const ImportProgressOptionalArguments& arguments = {}) {
    vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
    signal.set_owner_id(CryptohomeIdForProfile(profile()));
    signal.set_vm_name(container_id.vm_name);
    signal.set_container_name(container_id.container_name);
    signal.set_status(status);
    signal.set_progress_percent(arguments.progress_percent);
    signal.set_architecture_device("arch_dev");
    signal.set_architecture_container("arch_con");
    signal.set_available_space(arguments.available_space);
    signal.set_min_required_space(arguments.min_required_space);
    ash::FakeCiceroneClient::Get()->NotifyImportLxdContainerProgress(signal);
  }

  CrostiniExportImportTest()
      : default_container_id_(DefaultContainerId()),
        custom_container_id_(kCrostiniDefaultVmType, "MyVM", "MyContainer") {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
  }

  CrostiniExportImportTest(const CrostiniExportImportTest&) = delete;
  CrostiniExportImportTest& operator=(const CrostiniExportImportTest&) = delete;

  ~CrostiniExportImportTest() override {
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    crostini_export_import_ = std::make_unique<CrostiniExportImport>(profile());
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    notification_display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    notification_display_service_ =
        static_cast<StubNotificationDisplayService*>(
            NotificationDisplayServiceFactory::GetForProfile(profile()));
    ASSERT_NE(notification_display_service_, nullptr);
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        default_container_id_.vm_name);
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        custom_container_id_.vm_name);
    CrostiniManager::GetForProfile(profile())->set_skip_restart_for_testing();
    profile()->GetPrefs()->SetBoolean(
        crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, true);

    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        profile()->GetPath());
    tarball_ = file_manager::util::GetMyFilesFolderForProfile(profile()).Append(
        "crostini_export_import_unittest_tarball.tar.gz");
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()));
    crostini_export_import_.reset();
    // If the file has been created (by an export), then delete it, but first
    // shutdown GuestOsSharePath to ensure watchers are destroyed, otherwise
    // they can trigger and execute against a destroyed service.
    guest_os::GuestOsSharePathFactory::GetForProfile(profile())->Shutdown();
    task_environment_.RunUntilIdle();
    base::DeleteFile(tarball_);
    test_helper_.reset();
    profile_.reset();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniExportImport> crostini_export_import_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_tester_;
  raw_ptr<StubNotificationDisplayService, DanglingUntriaged>
      notification_display_service_;

  guest_os::GuestId default_container_id_;
  guest_os::GuestId custom_container_id_;
  base::FilePath tarball_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CrostiniExportImportTest, TestNotAllowed) {
  profile()->GetPrefs()->SetBoolean(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy, false);
  crostini_export_import_->ExportContainer(
      default_container_id_, tarball_,
      base::BindOnce([](CrostiniResult result) {
        EXPECT_EQ(result, CrostiniResult::NOT_ALLOWED);
      }));
  crostini_export_import_->ImportContainer(
      default_container_id_, tarball_,
      base::BindOnce([](CrostiniResult result) {
        EXPECT_EQ(result, CrostiniResult::NOT_ALLOWED);
      }));
}

TEST_F(CrostiniExportImportTest, TestExportDiskImageSuccess) {
  crostini_export_import_->FillOperationData(
      ExportImportType::EXPORT_DISK_IMAGE);
  // Technically only need a temp path here, but the file will just be
  // recreated.
  base::ScopedTempFile zipfile;
  EXPECT_TRUE(zipfile.Create());
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(zipfile.path()),
                                        0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);

  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // 50% done.
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_IN_PROGRESS, 50);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 50);
    EXPECT_TRUE(notification.pinned());
  }

  // Close notification and update progress. Should not update notification.
  controller->get_delegate()->Close(false);
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_IN_PROGRESS, 60);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 50);
    EXPECT_TRUE(notification.pinned());
  }

  // Done.
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_CREATED, 100);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully backed up");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestExportDiskImageFail) {
  crostini_export_import_->FillOperationData(
      ExportImportType::EXPORT_DISK_IMAGE);
  base::ScopedTempFile zipfile;
  EXPECT_TRUE(zipfile.Create());
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(zipfile.path()),
                                        0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);

  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // Fails.
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_FAILED, 0);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Backup couldn't be completed due to an error");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestExportDiskImageCancelled) {
  base::ScopedTempFile zipfile;
  EXPECT_TRUE(zipfile.Create());
  crostini_export_import_->FillOperationData(
      ExportImportType::EXPORT_DISK_IMAGE, custom_container_id_);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(zipfile.path()),
                                        0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);

  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // CANCEL:
  crostini_export_import_->CancelOperation(ExportImportType::EXPORT_DISK_IMAGE,
                                           custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // Should not be displayed as cancel is in progress
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_IN_PROGRESS, 7);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // CANCELLED:
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetController(custom_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    EXPECT_EQ(ui_notification, std::nullopt);
  }
}

TEST_F(CrostiniExportImportTest, TestImportDiskImageSuccess) {
  SetImportResponse();
  crostini_export_import_->FillOperationData(
      ExportImportType::IMPORT_DISK_IMAGE);
  base::ScopedTempFile zipfile;
  EXPECT_TRUE(zipfile.Create());
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(zipfile.path()),
                                        0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);

  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // 50% done.
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_IN_PROGRESS, 50);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 50);
    EXPECT_TRUE(notification.pinned());
  }

  // Close notification and update progress. Should not update notification.
  controller->get_delegate()->Close(false);
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_IN_PROGRESS, 60);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 50);
    EXPECT_TRUE(notification.pinned());
  }

  // Done.
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_CREATED, 100);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully replaced");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportDiskImageFail) {
  SetImportResponse();
  crostini_export_import_->FillOperationData(
      ExportImportType::IMPORT_DISK_IMAGE);
  base::ScopedTempFile zipfile;
  EXPECT_TRUE(zipfile.Create());
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(zipfile.path()),
                                        0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);

  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // Fails.
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_FAILED, 0);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Restoring couldn't be completed due to an error");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportDiskImageCancelled) {
  SetImportResponse();
  base::ScopedTempFile zipfile;
  EXPECT_TRUE(zipfile.Create());
  crostini_export_import_->FillOperationData(
      ExportImportType::IMPORT_DISK_IMAGE, custom_container_id_);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(zipfile.path()),
                                        0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);

  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // CANCEL:
  crostini_export_import_->CancelOperation(ExportImportType::IMPORT_DISK_IMAGE,
                                           custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // Should not be displayed as cancel is in progress
  SendDiskImageProgress(default_container_id_,
                        vm_tools::concierge::DISK_STATUS_IN_PROGRESS, 7);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // CANCELLED:
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetController(custom_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    EXPECT_EQ(ui_notification, std::nullopt);
  }
}

TEST_F(CrostiniExportImportTest, TestExportSuccess) {
  crostini_export_import_->FillOperationData(ExportImportType::EXPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ash::FakeSeneschalClient::Get()->share_path_called());
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // STREAMING 10% bytes done + 30% files done = 20% overall.
  SendExportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 30,
       .bytes_streamed = 10});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 20);
    EXPECT_TRUE(notification.pinned());
  }

  // STREAMING 66% bytes done + 55% files done then floored = 60% overall.
  SendExportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 55,
       .bytes_streamed = 66});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Close notification and update progress. Should not update notification.
  controller->get_delegate()->Close(false);
  SendExportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 90,
       .bytes_streamed = 85});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Done.
  SendExportProgress(
      default_container_id_,
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully backed up");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }

  // CrostiniExportImport should've created the exported file.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportCustomVmContainerSuccess) {
  crostini_export_import_->FillOperationData(ExportImportType::EXPORT,
                                             custom_container_id_);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ash::FakeSeneschalClient::Get()->share_path_called());
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // STREAMING 66% bytes done + 55% files done then floored = 60% overall.
  SendExportProgress(
      custom_container_id_,
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 55,
       .bytes_streamed = 66});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Close notification and update progress. Should not update notification.
  controller->get_delegate()->Close(false);
  SendExportProgress(
      custom_container_id_,
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 90,
       .bytes_streamed = 85});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Done.
  SendExportProgress(
      custom_container_id_,
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(GetController(custom_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully backed up");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }

  // CrostiniExportImport should've created the exported file.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportFail) {
  crostini_export_import_->FillOperationData(ExportImportType::EXPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // Failed.
  SendExportProgress(
      default_container_id_,
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Backup couldn't be completed due to an error");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }

  // CrostiniExportImport should cleanup the file if an export fails.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportCancelled) {
  crostini_export_import_->FillOperationData(ExportImportType::EXPORT,
                                             custom_container_id_);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::EXPORT,
                                           custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }
  EXPECT_TRUE(base::PathExists(tarball_));

  // STREAMING: should not be displayed as cancel is in progress
  SendExportProgress(
      custom_container_id_,
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING,
      {.total_files = 100,
       .total_bytes = 100,
       .files_streamed = 50,
       .bytes_streamed = 50});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }
  EXPECT_TRUE(base::PathExists(tarball_));

  // CANCELLED:
  SendExportProgress(
      custom_container_id_,
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_CANCELLED);
  EXPECT_EQ(GetController(custom_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    EXPECT_EQ(ui_notification, std::nullopt);
  }

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestExportDoneBeforeCancelled) {
  crostini_export_import_->FillOperationData(ExportImportType::EXPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::EXPORT,
                                           default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }
  EXPECT_TRUE(base::PathExists(tarball_));

  // DONE: Completed before cancel processed, file should be deleted.
  SendExportProgress(
      default_container_id_,
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    EXPECT_EQ(ui_notification, std::nullopt);
  }

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(tarball_));
}

TEST_F(CrostiniExportImportTest, TestImportSuccess) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ash::FakeSeneschalClient::Get()->share_path_called());
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // 20% UPLOAD = 10% overall.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD,
      {.progress_percent = 20});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 10);
    EXPECT_TRUE(notification.pinned());
  }

  // 20% UNPACK = 60% overall.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      {.progress_percent = 20});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Close notification and update progress. Should not update notification.
  controller->get_delegate()->Close(false);
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      {.progress_percent = 40});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Done.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully replaced");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportCustomVmContainerSuccess) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT,
                                             custom_container_id_);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ash::FakeSeneschalClient::Get()->share_path_called());
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(custom_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // 20% UNPACK = 60% overall.
  SendImportProgress(
      custom_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      {.progress_percent = 20});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Close notification and update progress. Should not update notification.
  controller->get_delegate()->Close(false);
  SendImportProgress(
      custom_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK,
      {.progress_percent = 40});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  {
    const message_center::Notification& notification =
        GetNotification(custom_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), 60);
    EXPECT_TRUE(notification.pinned());
  }

  // Done.
  SendImportProgress(
      custom_container_id_,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(GetController(custom_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully replaced");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportFail) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // Failed.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Restoring couldn't be completed due to an error");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportCancelled) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::IMPORT,
                                           default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // STREAMING: should not be displayed as cancel is in progress
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD,
      {.progress_percent = 50});
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // CANCELLED:
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_CANCELLED);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    EXPECT_EQ(ui_notification, std::nullopt);
  }
}

TEST_F(CrostiniExportImportTest, TestImportDoneBeforeCancelled) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // CANCELLING:
  crostini_export_import_->CancelOperation(ExportImportType::IMPORT,
                                           default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::CANCELLING);
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    EXPECT_EQ(notification.id(), notification_id);
    EXPECT_EQ(notification.progress(), -1);
    EXPECT_FALSE(notification.pinned());
  }

  // DONE: Cancel couldn't be processed in time, done is displayed instead.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg("Linux apps & files have been successfully replaced");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportFailArchitecture) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // Failed Architecture.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_FAILED_ARCHITECTURE);
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg(
        "Cannot import container architecture type arch_con with this device "
        "which is arch_dev. You can try restoring this container into a "
        "different device, or you can access the files inside this container "
        "image by opening in Files app.");
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

TEST_F(CrostiniExportImportTest, TestImportFailSpace) {
  crostini_export_import_->FillOperationData(ExportImportType::IMPORT);
  crostini_export_import_->FileSelected(ui::SelectedFileInfo(tarball_), 0);
  task_environment_.RunUntilIdle();
  base::WeakPtr<CrostiniExportImportNotificationController> controller =
      GetController(default_container_id_);
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->status(),
            CrostiniExportImportStatusTracker::Status::RUNNING);
  std::string notification_id;
  {
    const message_center::Notification& notification =
        GetNotification(default_container_id_);
    notification_id = notification.id();
    EXPECT_EQ(notification.progress(), 0);
    EXPECT_TRUE(notification.pinned());
  }

  // Failed Space.
  SendImportProgress(
      default_container_id_,
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED_SPACE,
      {
          .available_space = 20ul * 1'024 * 1'024 * 1'024,    // 20Gb
          .min_required_space = 35ul * 1'024 * 1'024 * 1'024  // 35Gb
      });
  EXPECT_EQ(GetController(default_container_id_), nullptr);
  EXPECT_EQ(controller, nullptr);
  {
    const std::optional<message_center::Notification> ui_notification =
        notification_display_service_->GetNotification(notification_id);
    ASSERT_NE(ui_notification, std::nullopt);
    EXPECT_FALSE(ui_notification->pinned());
    std::string msg =
        "Cannot restore due to lack of storage space. Free up 15.0 GB from the "
        "device and try again.";
    EXPECT_EQ(ui_notification->message(), base::UTF8ToUTF16(msg));
  }
}

}  // namespace crostini
