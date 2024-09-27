// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_misc.h"
#include "chrome/browser/ash/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/ash/file_manager/file_watcher.h"
#include "chrome/browser/ash/file_manager/mount_test_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/install_warning.h"
#include "google_apis/common/test_util.h"
#include "storage/browser/file_system/external_mount_points.h"

using ::testing::_;
using ::testing::ReturnRef;

namespace {

using ::ash::disks::Disk;
using ::ash::disks::DiskMountManager;
using ::ash::disks::FormatFileSystemType;

struct TestDiskInfo {
  const char* file_path;
  bool write_disabled_by_policy;
  const char* device_label;
  const char* drive_label;
  const char* vendor_id;
  const char* vendor_name;
  const char* product_id;
  const char* product_name;
  const char* fs_uuid;
  const char* storage_device_path;
  ash::DeviceType device_type;
  uint64_t size_in_bytes;
  bool is_parent;
  bool is_read_only_hardware;
  bool has_media;
  bool on_boot_device;
  bool on_removable_device;
  bool is_hidden;
  const char* file_system_type;
  const char* base_mount_path;
};

struct TestMountPoint {
  std::string source_path;
  std::string mount_path;
  ash::MountType mount_type;
  ash::MountError mount_error;

  // -1 if there is no disk info.
  int disk_info_index;
};

TestDiskInfo kTestDisks[] = {{"file_path1",
                              false,
                              "device_label1",
                              "drive_label1",
                              "0123",
                              "vendor1",
                              "abcd",
                              "product1",
                              "FFFF-FFFF",
                              "storage_device_path1",
                              ash::DeviceType::kUSB,
                              1073741824,
                              false,
                              false,
                              false,
                              false,
                              false,
                              false,
                              "exfat",
                              ""},
                             {"file_path2",
                              false,
                              "device_label2",
                              "drive_label2",
                              "4567",
                              "vendor2",
                              "cdef",
                              "product2",
                              "0FFF-FFFF",
                              "storage_device_path2",
                              ash::DeviceType::kMobile,
                              47723,
                              true,
                              true,
                              true,
                              true,
                              false,
                              false,
                              "exfat",
                              ""},
                             {"file_path3",
                              true,  // write_disabled_by_policy
                              "device_label3",
                              "drive_label3",
                              "89ab",
                              "vendor3",
                              "ef01",
                              "product3",
                              "00FF-FFFF",
                              "storage_device_path3",
                              ash::DeviceType::kOpticalDisc,
                              0,
                              true,
                              false,  // is_hardware_read_only
                              false,
                              true,
                              false,
                              false,
                              "exfat",
                              ""}};

const char kLocalMountPointName[] = "local";

void AddLocalFileSystem(Profile* profile, base::FilePath root) {
  const char kTestFileContent[] = "The five boxing wizards jumped quickly";

  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::CreateDirectory(root.AppendASCII("test_dir")));
    ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
        root.AppendASCII("test_dir").AppendASCII("test_file.txt"),
        kTestFileContent));
  }

  ASSERT_TRUE(profile->GetMountPoints()->RegisterFileSystem(
      kLocalMountPointName, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), root));
  file_manager::VolumeManager::Get(profile)->AddVolumeForTesting(
      root, file_manager::VOLUME_TYPE_TESTING, ash::DeviceType::kUnknown,
      false /* read_only */);
}

}  // namespace

class FileManagerPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  FileManagerPrivateApiTest() : disk_mount_manager_mock_(nullptr) {
    InitMountPoints();
  }

  ~FileManagerPrivateApiTest() override {
    DCHECK(!disk_mount_manager_mock_);
    DCHECK(!event_router_);
  }

  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(non_watchable_dir_.CreateUniqueTempDir());

    event_router_ = file_manager::EventRouterFactory::GetForProfile(profile());
  }

  void TearDownOnMainThread() override {
    event_router_ = nullptr;
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  // ExtensionApiTest override
  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    disk_mount_manager_mock_ = new ash::disks::MockDiskMountManager;
    DiskMountManager::InitializeForTesting(disk_mount_manager_mock_);
    disk_mount_manager_mock_->SetupDefaultReplies();

    // override mock functions.
    ON_CALL(*disk_mount_manager_mock_, FindDiskBySourcePath(_))
        .WillByDefault(
            Invoke(this, &FileManagerPrivateApiTest::FindVolumeBySourcePath));
    EXPECT_CALL(*disk_mount_manager_mock_, disks())
        .WillRepeatedly(ReturnRef(volumes_));
    EXPECT_CALL(*disk_mount_manager_mock_, mount_points())
        .WillRepeatedly(ReturnRef(mount_points_));
  }

  // ExtensionApiTest override
  void TearDownInProcessBrowserTestFixture() override {
    DiskMountManager::Shutdown();
    disk_mount_manager_mock_ = nullptr;

    extensions::ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

 private:
  void InitMountPoints() {
    const TestMountPoint kTestMountPoints[] = {
        {"device_path1",
         ash::CrosDisksClient::GetRemovableDiskMountPoint()
             .AppendASCII("mount_path1")
             .AsUTF8Unsafe(),
         ash::MountType::kDevice, ash::MountError::kSuccess, 0},
        {"device_path2",
         ash::CrosDisksClient::GetRemovableDiskMountPoint()
             .AppendASCII("mount_path2")
             .AsUTF8Unsafe(),
         ash::MountType::kDevice, ash::MountError::kSuccess, 1},
        {"device_path3",
         ash::CrosDisksClient::GetRemovableDiskMountPoint()
             .AppendASCII("mount_path3")
             .AsUTF8Unsafe(),
         ash::MountType::kDevice, ash::MountError::kSuccess, 2},
        {// Set source path inside another mounted volume.
         ash::CrosDisksClient::GetRemovableDiskMountPoint()
             .AppendASCII("mount_path3/archive.zip")
             .AsUTF8Unsafe(),
         ash::CrosDisksClient::GetArchiveMountPoint()
             .AppendASCII("archive_mount_path")
             .AsUTF8Unsafe(),
         ash::MountType::kArchive, ash::MountError::kSuccess, -1}};

    for (const auto& mp : kTestMountPoints) {
      mount_points_.insert(
          {mp.source_path, mp.mount_path, mp.mount_type, mp.mount_error});
      int disk_info_index = mp.disk_info_index;
      if (mp.disk_info_index >= 0) {
        EXPECT_GT(std::size(kTestDisks), static_cast<size_t>(disk_info_index));
        if (static_cast<size_t>(disk_info_index) >= std::size(kTestDisks)) {
          return;
        }

        std::unique_ptr<Disk> disk =
            Disk::Builder()
                .SetDevicePath(mp.source_path)
                .SetMountPath(mp.mount_path)
                .SetWriteDisabledByPolicy(
                    kTestDisks[disk_info_index].write_disabled_by_policy)
                .SetFilePath(kTestDisks[disk_info_index].file_path)
                .SetDeviceLabel(kTestDisks[disk_info_index].device_label)
                .SetDriveLabel(kTestDisks[disk_info_index].drive_label)
                .SetVendorId(kTestDisks[disk_info_index].vendor_id)
                .SetVendorName(kTestDisks[disk_info_index].vendor_name)
                .SetProductId(kTestDisks[disk_info_index].product_id)
                .SetProductName(kTestDisks[disk_info_index].product_name)
                .SetFileSystemUUID(kTestDisks[disk_info_index].fs_uuid)
                .SetStorageDevicePath(
                    kTestDisks[disk_info_index].storage_device_path)
                .SetDeviceType(kTestDisks[disk_info_index].device_type)
                .SetSizeInBytes(kTestDisks[disk_info_index].size_in_bytes)
                .SetIsParent(kTestDisks[disk_info_index].is_parent)
                .SetIsReadOnlyHardware(
                    kTestDisks[disk_info_index].is_read_only_hardware)
                .SetHasMedia(kTestDisks[disk_info_index].has_media)
                .SetOnBootDevice(kTestDisks[disk_info_index].on_boot_device)
                .SetOnRemovableDevice(
                    kTestDisks[disk_info_index].on_removable_device)
                .SetIsHidden(kTestDisks[disk_info_index].is_hidden)
                .SetFileSystemType(kTestDisks[disk_info_index].file_system_type)
                .SetBaseMountPath(kTestDisks[disk_info_index].base_mount_path)
                .Build();

        volumes_.insert(std::move(disk));
      }
    }
  }

  const Disk* FindVolumeBySourcePath(const std::string& source_path) {
    auto volume_it = volumes_.find(source_path);
    return (volume_it == volumes_.end()) ? nullptr : volume_it->get();
  }

 protected:
  void SshfsMount(const std::string& source_path,
                  const std::string& source_format,
                  const std::string& mount_label,
                  const std::vector<std::string>& mount_options,
                  ash::MountType type,
                  ash::MountAccessMode access_mode,
                  DiskMountManager::MountPathCallback callback) {
    DiskMountManager::MountPoint mount_point_info{
        source_path, "/media/fuse/" + mount_label,
        ash::MountType::kNetworkStorage};
    disk_mount_manager_mock_->NotifyMountEvent(
        DiskMountManager::MountEvent::MOUNTING, ash::MountError::kSuccess,
        mount_point_info);
    std::move(callback).Run(ash::MountError::kSuccess, mount_point_info);
  }

  void ExpectCrostiniMount() {
    std::vector<std::string> mount_options;
    EXPECT_CALL(*disk_mount_manager_mock_,
                MountPath("sftp://3:1234", "", "crostini_user_termina_penguin",
                          mount_options, ash::MountType::kNetworkStorage,
                          ash::MountAccessMode::kReadWrite, _))
        .WillOnce(Invoke(this, &FileManagerPrivateApiTest::SshfsMount));
  }

  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir non_watchable_dir_;
  raw_ptr<ash::disks::MockDiskMountManager, DanglingUntriaged>
      disk_mount_manager_mock_ = nullptr;
  DiskMountManager::Disks volumes_;
  DiskMountManager::MountPoints mount_points_;
  raw_ptr<file_manager::EventRouter> event_router_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Mount) {
  using ash::file_system_provider::IconSet;
  profile()->GetPrefs()->SetBoolean(drive::prefs::kDisableDrive, true);

  // Add a provided file system, to test passing the |configurable| and
  // |source| flags properly down to Files app.
  IconSet icon_set;
  icon_set.SetIcon(IconSet::IconSize::SIZE_16x16,
                   GURL("chrome://resources/testing-provider-id-16.jpg"));
  icon_set.SetIcon(IconSet::IconSize::SIZE_32x32,
                   GURL("chrome://resources/testing-provider-id-32.jpg"));
  ash::file_system_provider::ProvidedFileSystemInfo info(
      "testing-provider-id", ash::file_system_provider::MountOptions(),
      base::FilePath(), true /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, icon_set);

  file_manager::VolumeManager::Get(browser()->profile())
      ->AddVolumeForTesting(file_manager::Volume::CreateForProvidedFileSystem(
          info, file_manager::MOUNT_CONTEXT_AUTO));

  // The test.js calls fileManagerPrivate.removeMount(name) twice for each name
  // "mount_path1" and "archive_mount_path". The first call should unmount with
  // no error, and the second should fail with an error. Record the events, and
  // verify them by name and occurrence count.
  int events[4] = {0, 0, 0, 0};

  EXPECT_CALL(*disk_mount_manager_mock_,
              UnmountPath(ash::CrosDisksClient::GetRemovableDiskMountPoint()
                              .AppendASCII("mount_path1")
                              .AsUTF8Unsafe(),
                          _))
      .WillOnce(testing::Invoke(
          [&events](const std::string& path,
                    DiskMountManager::UnmountPathCallback callback) {
            const auto name = base::FilePath(path).BaseName();
            EXPECT_EQ("mount_path1", name.value());
            ++events[0];
            EXPECT_EQ(1, events[0]);
            std::move(callback).Run(ash::MountError::kSuccess);
          }))
      .WillOnce(testing::Invoke(
          [&events](const std::string& path,
                    DiskMountManager::UnmountPathCallback callback) {
            const auto name = base::FilePath(path).BaseName();
            EXPECT_EQ("mount_path1", name.value());
            ++events[1];
            EXPECT_EQ(1, events[1]);
            std::move(callback).Run(ash::MountError::kCancelled);
          }));

  EXPECT_CALL(*disk_mount_manager_mock_,
              UnmountPath(ash::CrosDisksClient::GetArchiveMountPoint()
                              .AppendASCII("archive_mount_path")
                              .AsUTF8Unsafe(),
                          _))
      .WillOnce(testing::Invoke(
          [&events](const std::string& path,
                    DiskMountManager::UnmountPathCallback callback) {
            const auto name = base::FilePath(path).BaseName();
            EXPECT_EQ("archive_mount_path", name.value());
            ++events[2];
            EXPECT_EQ(1, events[2]);
            std::move(callback).Run(ash::MountError::kSuccess);
          }))
      .WillOnce(testing::Invoke(
          [&events](const std::string& path,
                    DiskMountManager::UnmountPathCallback callback) {
            const auto name = base::FilePath(path).BaseName();
            EXPECT_EQ("archive_mount_path", name.value());
            ++events[3];
            EXPECT_EQ(1, events[3]);
            std::move(callback).Run(ash::MountError::kNeedPassword);
          }));

  ASSERT_TRUE(RunExtensionTest("file_browser/mount_test", {},
                               {.load_as_component = true}))
      << message_;

  ASSERT_EQ(1, events[0]);
  ASSERT_EQ(1, events[1]);
  ASSERT_EQ(1, events[2]);
  ASSERT_EQ(1, events[3]);
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, FormatVolume) {
  // Catch-all rule to ensure that FormatMountedDevice does not get called with
  // other arguments that don't match the rules below.
  EXPECT_CALL(*disk_mount_manager_mock_, FormatMountedDevice(_, _, _)).Times(0);

  EXPECT_CALL(
      *disk_mount_manager_mock_,
      FormatMountedDevice(ash::CrosDisksClient::GetRemovableDiskMountPoint()
                              .AppendASCII("mount_path1")
                              .AsUTF8Unsafe(),
                          FormatFileSystemType::kVfat, "NEWLABEL1"))
      .Times(1);
  EXPECT_CALL(
      *disk_mount_manager_mock_,
      FormatMountedDevice(ash::CrosDisksClient::GetRemovableDiskMountPoint()
                              .AppendASCII("mount_path2")
                              .AsUTF8Unsafe(),
                          FormatFileSystemType::kExfat, "NEWLABEL2"))
      .Times(1);
  EXPECT_CALL(
      *disk_mount_manager_mock_,
      FormatMountedDevice(ash::CrosDisksClient::GetRemovableDiskMountPoint()
                              .AppendASCII("mount_path3")
                              .AsUTF8Unsafe(),
                          FormatFileSystemType::kNtfs, "NEWLABEL3"))
      .Times(1);

  ASSERT_TRUE(RunExtensionTest("file_browser/format_test", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Permissions) {
  EXPECT_TRUE(RunExtensionTest("file_browser/permissions", {},
                               {.ignore_manifest_warnings = true}));
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, extension->install_warnings().size());
  const extensions::InstallWarning& warning = extension->install_warnings()[0];
  EXPECT_EQ("fileManagerPrivate", warning.key);
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, AddFileWatch) {
  // Add a filesystem and Volume that is not watchable.
  AddLocalFileSystem(browser()->profile(), non_watchable_dir_.GetPath());

  // Add a filesystem and Volume that is watchable.
  const base::FilePath downloads_dir = temp_dir_.GetPath();
  ASSERT_TRUE(file_manager::VolumeManager::Get(browser()->profile())
                  ->RegisterDownloadsDirectoryForTesting(downloads_dir));

  ASSERT_TRUE(RunExtensionTest("file_browser/add_file_watch", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Recent) {
  const base::FilePath downloads_dir = temp_dir_.GetPath();

  ASSERT_TRUE(file_manager::VolumeManager::Get(browser()->profile())
                  ->RegisterDownloadsDirectoryForTesting(downloads_dir));

  // Create test files.
  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File image_file(downloads_dir.Append("all-justice.jpg"),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(image_file.IsValid());
    base::File audio_file(downloads_dir.Append("all-justice.mp3"),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(audio_file.IsValid());
    base::File video_file(downloads_dir.Append("all-justice.mp4"),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(video_file.IsValid());
    base::File text_file(downloads_dir.Append("all-justice.txt"),
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(text_file.IsValid());
    ASSERT_TRUE(text_file.SetTimes(base::Time::Now(),
                                   base::Time::Now() - base::Days(60)));
  }

  ASSERT_TRUE(RunExtensionTest("file_browser/recent_test", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, MediaMetadata) {
  const base::FilePath test_dir = temp_dir_.GetPath();
  AddLocalFileSystem(browser()->profile(), test_dir);

  // Get source media/test/data directory path.
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir));
  const base::FilePath media_test_data_dir =
      root_dir.AppendASCII("media").AppendASCII("test").AppendASCII("data");

  // Returns a path to a media/test/data test file.
  auto get_media_test_data_file = [&](const std::string& file) {
    return media_test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file));
  };

  // Create media test files.
  {
    base::ScopedAllowBlockingForTesting allow_io;

    const base::FilePath video = get_media_test_data_file("90rotation.mp4");
    ASSERT_TRUE(base::CopyFile(video, test_dir.Append(video.BaseName())));

    const base::FilePath audio = get_media_test_data_file("id3_png_test.mp3");
    ASSERT_TRUE(base::CopyFile(audio, test_dir.Append(audio.BaseName())));
  }

  // Get source chrome/test/data/chromeos/file_manager directory path.
  const base::FilePath files_test_data_dir = root_dir.AppendASCII("chrome")
                                                 .AppendASCII("test")
                                                 .AppendASCII("data")
                                                 .AppendASCII("chromeos")
                                                 .AppendASCII("file_manager");

  // Returns a path to a chrome/test/data/chromeos/file_manager test file.
  auto get_files_test_data_dir = [&](const std::string& file) {
    return files_test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file));
  };

  // Create files test files.
  {
    base::ScopedAllowBlockingForTesting allow_io;

    const base::FilePath broke = get_files_test_data_dir("broken.jpg");
    ASSERT_TRUE(base::CopyFile(broke, test_dir.Append(broke.BaseName())));

    const base::FilePath empty = get_files_test_data_dir("empty.txt");
    ASSERT_TRUE(base::CopyFile(empty, test_dir.Append(empty.BaseName())));

    const base::FilePath image = get_files_test_data_dir("image3.jpg");
    ASSERT_TRUE(base::CopyFile(image, test_dir.Append(image.BaseName())));
  }

  ASSERT_TRUE(RunExtensionTest("file_browser/media_metadata", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Crostini) {
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_is_allowed_now(true);
  crostini_features.set_enabled(true);

  // Setup CrostiniManager for testing.
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(browser()->profile());
  crostini_manager->set_skip_restart_for_testing();
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName, 3);
  crostini_manager->AddRunningContainerForTesting(
      crostini::kCrostiniDefaultVmName,
      crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                              "testuser", "/home/testuser", "PLACEHOLDER_IP",
                              1234));

  ExpectCrostiniMount();

  // Add 'testing' volume with 'test_dir', create 'share_dir' in Downloads.
  AddLocalFileSystem(browser()->profile(), temp_dir_.GetPath());
  base::FilePath downloads;
  ASSERT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(browser()->profile()),
          &downloads));
  // Setup prefs guest_os.paths_shared_to_vms.
  base::FilePath shared1 = downloads.AppendASCII("shared1");
  base::FilePath shared2 = downloads.AppendASCII("shared2");
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::CreateDirectory(downloads.AppendASCII("share_dir")));
    ASSERT_TRUE(base::CreateDirectory(shared1));
    ASSERT_TRUE(base::CreateDirectory(shared2));
  }
  guest_os::GuestOsSharePath* guest_os_share_path =
      guest_os::GuestOsSharePathFactory::GetForProfile(browser()->profile());
  guest_os_share_path->RegisterPersistedPaths(crostini::kCrostiniDefaultVmName,
                                              {shared1});
  guest_os_share_path->RegisterPersistedPaths(crostini::kCrostiniDefaultVmName,
                                              {shared2});

  ASSERT_TRUE(RunExtensionTest("file_browser/crostini_test", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, CrostiniIncognito) {
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_is_allowed_now(true);
  crostini_features.set_enabled(true);

  // Setup CrostiniManager for testing.
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(browser()->profile());
  crostini_manager->set_skip_restart_for_testing();
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName, 3);
  crostini_manager->AddRunningContainerForTesting(
      crostini::kCrostiniDefaultVmName,
      crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                              "testuser", "/home/testuser", "PLACEHOLDER_IP",
                              1234));

  ExpectCrostiniMount();

  scoped_refptr<extensions::FileManagerPrivateMountCrostiniFunction> function(
      new extensions::FileManagerPrivateMountCrostiniFunction());
  // Use incognito profile.
  function->SetBrowserContextForTesting(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  extensions::api_test_utils::SendResponseHelper response_helper(
      function.get());
  function->RunWithValidation().Execute();
  response_helper.WaitForResponse();
  EXPECT_TRUE(response_helper.GetResponse());
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, HoldingSpace) {
  const base::FilePath test_dir = temp_dir_.GetPath();
  AddLocalFileSystem(browser()->profile(), test_dir);

  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File image_file(test_dir.Append("test_image.jpg"),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(image_file.IsValid());
    base::File audio_file(test_dir.Append("test_audio.mp3"),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(audio_file.IsValid());
    base::File video_file(test_dir.Append("test_video.mp4"),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(video_file.IsValid());
  }

  EXPECT_TRUE(RunExtensionTest("file_browser/holding_space", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, GetVolumeRoot) {
  AddLocalFileSystem(browser()->profile(), temp_dir_.GetPath());

  ASSERT_TRUE(RunExtensionTest("file_browser/get_volume_root", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, OpenURL) {
  const char* target_url = "https://www.google.com/";
  content::TestNavigationObserver navigation_observer(GURL{target_url});
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(RunExtensionTest("file_browser/open_url",
                               {.custom_arg = target_url},
                               {.load_as_component = true}));
  // Wait for navigation to finish.
  navigation_observer.Wait();

  // Check that the current active web contents points to the expected URL.
  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* browser = browser_list->GetLastActive();
  content::WebContents* active_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_STREQ(target_url, active_web_contents->GetVisibleURL().spec().c_str());
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, SearchFiles) {
  const base::FilePath downloads_dir = temp_dir_.GetPath();
  ASSERT_TRUE(file_manager::VolumeManager::Get(browser()->profile())
                  ->RegisterDownloadsDirectoryForTesting(downloads_dir));

  {
    base::ScopedAllowBlockingForTesting allow_io;
    // Creates two files with the same prefix, in different locations.
    base::File root_image_file(
        downloads_dir.Append("foo.jpg"),
        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(root_image_file.IsValid());

    ASSERT_TRUE(base::CreateDirectory(downloads_dir.AppendASCII("images")));
    base::File nested_image_file(
        downloads_dir.Append("images").Append("foo.jpg"),
        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(nested_image_file.IsValid());

    // Creates two files with the same prefix, and different modified dates.
    base::File jan_15_file(downloads_dir.Append("bar_15012020.jpg"),
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(jan_15_file.IsValid());
    static constexpr base::Time::Exploded kJan152020Noon = {.year = 2020,
                                                            .month = 1,
                                                            .day_of_week = 3,
                                                            .day_of_month = 15,
                                                            .hour = 12};
    base::Time jan_15_2020_noon;
    ASSERT_TRUE(base::Time::FromUTCExploded(kJan152020Noon, &jan_15_2020_noon));
    jan_15_file.SetTimes(jan_15_2020_noon, jan_15_2020_noon);

    base::File jan_01_file(downloads_dir.Append("bar_01012020.jpg"),
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(jan_01_file.IsValid());
    static constexpr base::Time::Exploded kJan012020Noon = {.year = 2020,
                                                            .month = 1,
                                                            .day_of_week = 3,
                                                            .day_of_month = 1,
                                                            .hour = 12};
    base::Time jan_01_2020_noon;
    ASSERT_TRUE(base::Time::FromUTCExploded(kJan012020Noon, &jan_01_2020_noon));
    jan_01_file.SetTimes(jan_01_2020_noon, jan_01_2020_noon);
  }

  ASSERT_TRUE(RunExtensionTest("file_browser/search_files", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, GetPdfThumbnail) {
  AddLocalFileSystem(browser()->profile(), temp_dir_.GetPath());
  base::FilePath downloads;
  ASSERT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(browser()->profile()),
          &downloads));

  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath source_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA).AppendASCII("pdf");
    ASSERT_TRUE(base::CopyFile(source_dir.AppendASCII("test.pdf"),
                               downloads.AppendASCII("test.pdf")));
    ASSERT_TRUE(base::CopyFile(source_dir.AppendASCII("combobox_form.pdf"),
                               downloads.AppendASCII("combobox_form.pdf")));
  }

  EXPECT_TRUE(RunExtensionTest("image_loader_private/get_pdf_thumbnail",
                               /*run_options=*/{},
                               /*load_options=*/{.load_as_component = true}));
}

class FileManagerPrivateApiDlpTest : public FileManagerPrivateApiTest {
 public:
  FileManagerPrivateApiDlpTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDataLeakPreventionFilesRestriction);
  }

  FileManagerPrivateApiDlpTest(const FileManagerPrivateApiDlpTest&) = delete;
  void operator=(const FileManagerPrivateApiDlpTest&) = delete;

  ~FileManagerPrivateApiDlpTest() override = default;

  void SetUpOnMainThread() override {
    FileManagerPrivateApiTest::SetUpOnMainThread();
    ASSERT_TRUE(drive_path_.CreateUniqueTempDir());
  }

  void TearDownOnMainThread() override {
    // The files controller must be destroyed before the profile since it's
    // holding a pointer to it.
    files_controller_.reset();
    mock_rules_manager_ = nullptr;
    fpnm_ = nullptr;
    FileManagerPrivateApiTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<policy::MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ = std::make_unique<policy::DlpFilesControllerAsh>(
        *mock_rules_manager_, Profile::FromBrowserContext(context));
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  std::unique_ptr<KeyedService> SetFPNM(content::BrowserContext* context) {
    auto fpnm = std::make_unique<policy::MockFilesPolicyNotificationManager>(
        browser()->profile());
    fpnm_ = fpnm.get();
    return fpnm;
  }

 protected:
  base::ScopedTempDir drive_path_;
  raw_ptr<policy::MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_ =
      nullptr;
  std::unique_ptr<policy::DlpFilesControllerAsh> files_controller_;
  raw_ptr<policy::MockFilesPolicyNotificationManager, DanglingUntriaged> fpnm_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FileManagerPrivateApiDlpOldUXTest : public FileManagerPrivateApiDlpTest {
 protected:
  FileManagerPrivateApiDlpOldUXTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDataLeakPreventionFilesRestriction},
        /*disabled_features=*/{features::kNewFilesPolicyUX});
  }

  FileManagerPrivateApiDlpOldUXTest(const FileManagerPrivateApiDlpOldUXTest&) =
      delete;
  void operator=(const FileManagerPrivateApiDlpOldUXTest&) = delete;

  ~FileManagerPrivateApiDlpOldUXTest() override = default;
};

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpOldUXTest, DlpBlockCopy) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

  base::FilePath my_files_dir_ =
      file_manager::util::GetMyFilesFolderForProfile(browser()->profile());
  {
    base::ScopedAllowBlockingForTesting allow_io;

    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
  }
  AddLocalFileSystem(browser()->profile(), my_files_dir_);

  const char kTestFileName[] = "dlp_test_file.txt";
  const base::FilePath test_file_path = my_files_dir_.Append(kTestFileName);

  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File test_file(test_file_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(test_file.IsValid());

    ASSERT_TRUE(base::CreateDirectory(drive_path_.GetPath().Append("subdir")));
  }

  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(browser()->profile());
  ash::FileSystemBackend::Get(*file_system_context)
      ->GrantFileAccessToOrigin(
          url::Origin::Create(
              GURL("chrome-extension://"
                   "pkplfbidichfdicaijlchgnapepdginl/")),  // Testing
                                                           // extension
          base::FilePath(
              base::StrCat({kLocalMountPointName, "/", kTestFileName})));

  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      "drivefs-delayed_mount_2", storage::kFileSystemTypeDriveFs,
      storage::FileSystemMountOption(), drive_path_.GetPath());
  file_manager::VolumeManager::Get(browser()->profile())
      ->AddVolumeForTesting(  // IN-TEST
          drive_path_.GetPath(), file_manager::VOLUME_TYPE_GOOGLE_DRIVE,
          ash::DeviceType::kUnknown,
          /*read_only=*/false);

  dlp::CheckFilesTransferResponse check_files_response;
  check_files_response.add_files_paths(test_file_path.value());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      std::move(check_files_response));

  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_rules_manager_, GetDlpFilesController)
      .Times(testing::AnyNumber());

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_block", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest, DlpMetadata) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(1);

  base::FilePath my_files_dir_ =
      file_manager::util::GetMyFilesFolderForProfile(browser()->profile());
  {
    base::ScopedAllowBlockingForTesting allow_io;

    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
  }
  AddLocalFileSystem(browser()->profile(), my_files_dir_);

  const base::FilePath blocked_file_path =
      my_files_dir_.Append("blocked_file.txt");
  const base::FilePath unrestricted_file_path =
      my_files_dir_.Append("unrestricted_file.txt");
  const base::FilePath untracked_file_path =
      my_files_dir_.Append("untracked_file.txt");

  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File blocked_test_file(
        blocked_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(blocked_test_file.IsValid());

    base::File unrestricted_test_file(
        unrestricted_file_path,
        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(unrestricted_test_file.IsValid());

    base::File untracked_test_file(
        untracked_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(untracked_test_file.IsValid());
  }

  base::MockCallback<chromeos::DlpClient::AddFilesCallback> add_files_cb;
  EXPECT_CALL(add_files_cb, Run).Times(1);
  dlp::AddFilesRequest request;
  dlp::AddFileRequest* file_request1 = request.add_add_file_requests();
  file_request1->set_file_path(blocked_file_path.value());
  file_request1->set_source_url("https://example1.com");
  dlp::AddFileRequest* file_request2 = request.add_add_file_requests();
  file_request2->set_file_path(unrestricted_file_path.value());
  file_request2->set_source_url("https://example2.com");
  chromeos::DlpClient::Get()->AddFiles(request, add_files_cb.Get());

  EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(policy::DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(policy::DlpRulesManager::Level::kAllow))
      .RetiresOnSaturation();

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "default"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest, DlpRestrictionDetails) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(1);

  policy::DlpRulesManager::AggregatedDestinations destinations;
  destinations[policy::DlpRulesManager::Level::kBlock].insert(
      "https://external.com");
  destinations[policy::DlpRulesManager::Level::kAllow].insert(
      "https://internal.com");
  policy::DlpRulesManager::AggregatedComponents components;
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kArc);
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kCrostini);
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kPluginVm);
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kUsb);
  components[policy::DlpRulesManager::Level::kAllow].insert(
      data_controls::Component::kDrive);
  EXPECT_CALL(*mock_rules_manager_, GetAggregatedDestinations)
      .WillOnce(testing::Return(destinations));
  EXPECT_CALL(*mock_rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "restriction_details"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest, DlpBlockedComponents) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(1);

  policy::DlpRulesManager::AggregatedComponents components;
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kArc);
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kCrostini);
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kPluginVm);
  components[policy::DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kUsb);
  components[policy::DlpRulesManager::Level::kAllow].insert(
      data_controls::Component::kDrive);
  EXPECT_CALL(*mock_rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "blocked_components"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest, DlpMetadata_Disabled) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
      .WillByDefault(testing::Return(false));
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(3);
  // We should not get to the point of checking DLP.
  EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule).Times(0);
  EXPECT_CALL(*mock_rules_manager_, GetAggregatedComponents).Times(0);
  EXPECT_CALL(*mock_rules_manager_, GetAggregatedDestinations).Times(0);

  AddLocalFileSystem(browser()->profile(), temp_dir_.GetPath());

  const base::FilePath blocked_file_path =
      temp_dir_.GetPath().Append("blocked_file.txt");

  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File blocked_test_file(
        blocked_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(blocked_test_file.IsValid());
  }

  base::MockCallback<chromeos::DlpClient::AddFilesCallback> add_files_cb;
  EXPECT_CALL(add_files_cb, Run).Times(1);
  dlp::AddFilesRequest request;
  dlp::AddFileRequest* file_request = request.add_add_file_requests();
  file_request->set_file_path(blocked_file_path.value());
  file_request->set_source_url("https://example1.com");
  chromeos::DlpClient::Get()->AddFiles(request, add_files_cb.Get());

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "disabled"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest, DlpMetadata_Error) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(2);
  // We should not get to the point of checking DLP.
  EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule).Times(0);

  AddLocalFileSystem(browser()->profile(), temp_dir_.GetPath());

  const base::FilePath blocked_file_path =
      temp_dir_.GetPath().Append("blocked_file.txt");

  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File blocked_test_file(
        blocked_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(blocked_test_file.IsValid());
  }

  base::MockCallback<chromeos::DlpClient::AddFilesCallback> add_files_cb;
  EXPECT_CALL(add_files_cb, Run).Times(1);
  dlp::AddFilesRequest request;
  dlp::AddFileRequest* file_request = request.add_add_file_requests();
  file_request->set_file_path(blocked_file_path.value());
  file_request->set_source_url("https://example1.com");
  chromeos::DlpClient::Get()->AddFiles(request, add_files_cb.Get());

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "error"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest,
                       DlpMetadata_DismissIOTask) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(0);
  // We should not get to the point of checking DLP.
  EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule).Times(0);

  policy::FilesPolicyNotificationManagerFactory::GetInstance()
      ->SetTestingFactory(
          browser()->profile(),
          base::BindRepeating(&FileManagerPrivateApiDlpTest::SetFPNM,
                              base::Unretained(this)));
  // FPNM is created lazily so initialize it before running the test.
  ASSERT_TRUE(
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          browser()->profile()));
  // Expect only the valid task id.
  EXPECT_CALL(*fpnm_, OnErrorItemDismissed(1u));

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "dismissIOTask"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest,
                       DlpMetadata_ProgressPausedTasks) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(0);
  // We should not get to the point of checking DLP.
  EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule).Times(0);

  std::vector<storage::FileSystemURL> source_urls{
      storage::FileSystemURL::CreateForTest(
          GURL("filesystem:chrome-extension://abc/external/foo/src")),
  };
  auto dest = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/foo/dest"));

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(browser()->profile());
  ASSERT_TRUE(volume_manager);
  file_manager::io_task::IOTaskController* io_task_controller =
      volume_manager->io_task_controller();
  ASSERT_TRUE(io_task_controller);

  // Create and pause the task.
  auto task_id = io_task_controller->Add(
      std::make_unique<file_manager::io_task::DummyIOTask>(
          source_urls, dest, file_manager::io_task::OperationType::kCopy,
          /*show_notification=*/true, /*progress_succeeds=*/false));
  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(policy::Policy::kDlp);
  io_task_controller->Pause(task_id, pause_params);

  // Set up FPNM, but only after creating and pausing the task so that it would
  // only get notified by the API call.
  policy::FilesPolicyNotificationManagerFactory::GetInstance()
      ->SetTestingFactory(
          browser()->profile(),
          base::BindRepeating(&FileManagerPrivateApiDlpTest::SetFPNM,
                              base::Unretained(this)));
  // FPNM is created lazily so initialize it before running the test.
  ASSERT_TRUE(
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          browser()->profile()));
  // Expect the pause status from ProgressPausedTasks.
  EXPECT_CALL(
      *fpnm_,
      OnIOTaskStatus(AllOf(
          testing::Field(&file_manager::io_task::ProgressStatus::task_id,
                         task_id),
          testing::Field(&file_manager::io_task::ProgressStatus::state,
                         file_manager::io_task::State::kPaused),
          testing::Field(&file_manager::io_task::ProgressStatus::pause_params,
                         pause_params))));
  // FPNM should also get notified to show the notification by this call.
  EXPECT_CALL(
      *fpnm_,
      ShowFilesPolicyNotification(
          "swa-file-operation-1",
          AllOf(testing::Field(&file_manager::io_task::ProgressStatus::task_id,
                               task_id),
                testing::Field(&file_manager::io_task::ProgressStatus::state,
                               file_manager::io_task::State::kPaused),
                testing::Field(
                    &file_manager::io_task::ProgressStatus::pause_params,
                    pause_params))));

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "progressPausedTasks"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest,
                       DlpMetadata_ShowPolicyDialog) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  EXPECT_CALL(*mock_rules_manager_, IsFilesPolicyEnabled).Times(0);
  // We should not get to the point of checking DLP.
  EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule).Times(0);

  // Set up FPNM.
  policy::FilesPolicyNotificationManagerFactory::GetInstance()
      ->SetTestingFactory(
          browser()->profile(),
          base::BindRepeating(&FileManagerPrivateApiDlpTest::SetFPNM,
                              base::Unretained(this)));
  // FPNM is created lazily so initialize it before running the test.
  ASSERT_TRUE(
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          browser()->profile()));

  // Expect only the calls with valid parameters.
  testing::InSequence s;
  EXPECT_CALL(*fpnm_, ShowDialog(1u, policy::FilesDialogType::kWarning));
  EXPECT_CALL(*fpnm_, ShowDialog(2u, policy::FilesDialogType::kError));

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "showPolicyDialog"},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiDlpTest,
                       DlpMetadata_GetDialogCaller) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&FileManagerPrivateApiDlpTest::SetDlpRulesManager,
                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

  policy::DlpFileDestination caller(GURL("https://example.com"));
  SelectFileDialogExtensionUserData::SetDialogCallerForTesting(&caller);

  EXPECT_TRUE(RunExtensionTest("file_browser/dlp_metadata",
                               {.custom_arg = "getDialogCaller"},
                               {.load_as_component = true}));
}
