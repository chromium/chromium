// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base64.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/drive/drivefs_test_support.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_misc.h"
#include "chrome/browser/chromeos/file_manager/file_watcher.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/file_change.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "extensions/common/extension.h"
#include "extensions/common/install_warning.h"
#include "google_apis/drive/test_util.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "storage/browser/fileapi/external_mount_points.h"

using ::testing::_;
using ::testing::ReturnRef;

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;

namespace {

struct TestDiskInfo {
  const char* system_path;
  const char* file_path;
  bool write_disabled_by_policy;
  const char* device_label;
  const char* drive_label;
  const char* vendor_id;
  const char* vendor_name;
  const char* product_id;
  const char* product_name;
  const char* fs_uuid;
  const char* system_path_prefix;
  chromeos::DeviceType device_type;
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
  chromeos::MountType mount_type;
  chromeos::disks::MountCondition mount_condition;

  // -1 if there is no disk info.
  int disk_info_index;
};

TestDiskInfo kTestDisks[] = {{"system_path1",
                              "file_path1",
                              false,
                              "device_label1",
                              "drive_label1",
                              "0123",
                              "vendor1",
                              "abcd",
                              "product1",
                              "FFFF-FFFF",
                              "system_path_prefix1",
                              chromeos::DEVICE_TYPE_USB,
                              1073741824,
                              false,
                              false,
                              false,
                              false,
                              false,
                              false,
                              "exfat",
                              ""},
                             {"system_path2",
                              "file_path2",
                              false,
                              "device_label2",
                              "drive_label2",
                              "4567",
                              "vendor2",
                              "cdef",
                              "product2",
                              "0FFF-FFFF",
                              "system_path_prefix2",
                              chromeos::DEVICE_TYPE_MOBILE,
                              47723,
                              true,
                              true,
                              true,
                              true,
                              false,
                              false,
                              "exfat",
                              ""},
                             {"system_path3",
                              "file_path3",
                              true,  // write_disabled_by_policy
                              "device_label3",
                              "drive_label3",
                              "89ab",
                              "vendor3",
                              "ef01",
                              "product3",
                              "00FF-FFFF",
                              "system_path_prefix3",
                              chromeos::DEVICE_TYPE_OPTICAL_DISC,
                              0,
                              true,
                              false,  // is_hardware_read_only
                              false,
                              true,
                              false,
                              false,
                              "exfat",
                              ""}};

void DispatchDirectoryChangeEventImpl(
    int* counter,
    const base::FilePath& virtual_path,
    const drive::FileChange* list,
    bool got_error,
    const std::vector<std::string>& extension_ids) {
  ++(*counter);
}

void AddFileWatchCallback(bool success) {}

void AddLocalFileSystem(Profile* profile, base::FilePath root) {
  const char kLocalMountPointName[] = "local";
  const char kTestFileContent[] = "The five boxing wizards jumped quickly";

  ASSERT_TRUE(base::CreateDirectory(root.AppendASCII("test_dir")));
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      root.AppendASCII("test_dir").AppendASCII("test_file.txt"),
      kTestFileContent));

  ASSERT_TRUE(
      content::BrowserContext::GetMountPoints(profile)->RegisterFileSystem(
          kLocalMountPointName, storage::kFileSystemTypeNativeLocal,
          storage::FileSystemMountOption(), root));
  file_manager::VolumeManager::Get(profile)->AddVolumeForTesting(
      root, file_manager::VOLUME_TYPE_TESTING, chromeos::DEVICE_TYPE_UNKNOWN,
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
    DCHECK(!testing_profile_);
    DCHECK(!event_router_);
  }

  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    testing_profile_ = std::make_unique<TestingProfile>();
    event_router_ =
        std::make_unique<file_manager::EventRouter>(testing_profile_.get());
  }

  void TearDownOnMainThread() override {
    event_router_->Shutdown();

    event_router_.reset();
    testing_profile_.reset();

    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  // ExtensionApiTest override
  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_mock_);
    disk_mount_manager_mock_->SetupDefaultReplies();

    // override mock functions.
    ON_CALL(*disk_mount_manager_mock_, FindDiskBySourcePath(_)).WillByDefault(
        Invoke(this, &FileManagerPrivateApiTest::FindVolumeBySourcePath));
    EXPECT_CALL(*disk_mount_manager_mock_, disks())
        .WillRepeatedly(ReturnRef(volumes_));
    EXPECT_CALL(*disk_mount_manager_mock_, mount_points())
        .WillRepeatedly(ReturnRef(mount_points_));
  }

  // ExtensionApiTest override
  void TearDownInProcessBrowserTestFixture() override {
    chromeos::disks::DiskMountManager::Shutdown();
    disk_mount_manager_mock_ = nullptr;

    extensions::ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

 private:
  void InitMountPoints() {
    const TestMountPoint kTestMountPoints[] = {
      {
        "device_path1",
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path1").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_DEVICE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        0
      },
      {
        "device_path2",
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path2").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_DEVICE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        1
      },
      {
        "device_path3",
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path3").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_DEVICE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        2
      },
      {
        // Set source path inside another mounted volume.
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path3/archive.zip").AsUTF8Unsafe(),
        chromeos::CrosDisksClient::GetArchiveMountPoint().AppendASCII(
            "archive_mount_path").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_ARCHIVE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        -1
      }
    };

    for (size_t i = 0; i < arraysize(kTestMountPoints); i++) {
      mount_points_.insert(DiskMountManager::MountPointMap::value_type(
          kTestMountPoints[i].mount_path,
          DiskMountManager::MountPointInfo(kTestMountPoints[i].source_path,
                                           kTestMountPoints[i].mount_path,
                                           kTestMountPoints[i].mount_type,
                                           kTestMountPoints[i].mount_condition)
      ));
      int disk_info_index = kTestMountPoints[i].disk_info_index;
      if (kTestMountPoints[i].disk_info_index >= 0) {
        EXPECT_GT(arraysize(kTestDisks), static_cast<size_t>(disk_info_index));
        if (static_cast<size_t>(disk_info_index) >= arraysize(kTestDisks))
          return;

        std::unique_ptr<Disk> disk =
            Disk::Builder()
                .SetDevicePath(kTestMountPoints[i].source_path)
                .SetMountPath(kTestMountPoints[i].mount_path)
                .SetWriteDisabledByPolicy(
                    kTestDisks[disk_info_index].write_disabled_by_policy)
                .SetSystemPath(kTestDisks[disk_info_index].system_path)
                .SetFilePath(kTestDisks[disk_info_index].file_path)
                .SetDeviceLabel(kTestDisks[disk_info_index].device_label)
                .SetDriveLabel(kTestDisks[disk_info_index].drive_label)
                .SetVendorId(kTestDisks[disk_info_index].vendor_id)
                .SetVendorName(kTestDisks[disk_info_index].vendor_name)
                .SetProductId(kTestDisks[disk_info_index].product_id)
                .SetProductName(kTestDisks[disk_info_index].product_name)
                .SetFileSystemUUID(kTestDisks[disk_info_index].fs_uuid)
                .SetSystemPathPrefix(
                    kTestDisks[disk_info_index].system_path_prefix)
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

        volumes_.insert(DiskMountManager::DiskMap::value_type(
            kTestMountPoints[i].source_path, std::move(disk)));
      }
    }
  }

  const Disk* FindVolumeBySourcePath(const std::string& source_path) {
    auto volume_it = volumes_.find(source_path);
    return (volume_it == volumes_.end()) ? nullptr : volume_it->second.get();
  }

 protected:
  void SshfsMount(const std::string& source_path,
                  const std::string& source_format,
                  const std::string& mount_label,
                  const std::vector<std::string>& mount_options,
                  chromeos::MountType type,
                  chromeos::MountAccessMode access_mode) {
    disk_mount_manager_mock_->NotifyMountEvent(
        chromeos::disks::DiskMountManager::MountEvent::MOUNTING,
        chromeos::MountError::MOUNT_ERROR_NONE,
        chromeos::disks::DiskMountManager::MountPointInfo(
            source_path, "/media/fuse/" + mount_label,
            chromeos::MountType::MOUNT_TYPE_NETWORK_STORAGE,
            chromeos::disks::MountCondition::MOUNT_CONDITION_NONE));
  }

  void EnableCrostiniForProfile(
      base::test::ScopedFeatureList* scoped_feature_list) {
    // TODO(joelhockey): Setting prefs and features to allow crostini is not
    // ideal.  It would be better if the crostini interface allowed for testing
    // without such tight coupling.
    browser()->profile()->GetPrefs()->SetBoolean(
        crostini::prefs::kCrostiniEnabled, true);
    scoped_feature_list->InitWithFeatures(
        {features::kCrostini, features::kExperimentalCrostiniUI}, {});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kCrostiniFiles);
    // Profile must be signed in with email for crostini.
    identity::SetPrimaryAccount(
        SigninManagerFactory::GetForProfileIfExists(browser()->profile()),
        IdentityManagerFactory::GetForProfileIfExists(browser()->profile()),
        "testuser@gmail.com");
  }

  void ExpectCrostiniMount() {
    std::string known_hosts;
    base::Base64Encode("[hostname]:2222 pubkey", &known_hosts);
    std::string identity;
    base::Base64Encode("privkey", &identity);
    std::vector<std::string> mount_options = {
        "UserKnownHostsBase64=" + known_hosts, "IdentityBase64=" + identity,
        "Port=2222"};
    EXPECT_CALL(*disk_mount_manager_mock_,
                MountPath("sshfs://testuser@hostname:", "",
                          "crostini_user_termina_penguin", mount_options,
                          chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                          chromeos::MOUNT_ACCESS_MODE_READ_WRITE))
        .WillOnce(Invoke(this, &FileManagerPrivateApiTest::SshfsMount));
  }

  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_;
  DiskMountManager::DiskMap volumes_;
  DiskMountManager::MountPointMap mount_points_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<file_manager::EventRouter> event_router_;
};

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Mount) {
  using chromeos::file_system_provider::IconSet;
  profile()->GetPrefs()->SetBoolean(drive::prefs::kDisableDrive, true);

  // Add a provided file system, to test passing the |configurable| and
  // |source| flags properly down to Files app.
  IconSet icon_set;
  icon_set.SetIcon(IconSet::IconSize::SIZE_16x16,
                   GURL("chrome://resources/testing-provider-id-16.jpg"));
  icon_set.SetIcon(IconSet::IconSize::SIZE_32x32,
                   GURL("chrome://resources/testing-provider-id-32.jpg"));
  chromeos::file_system_provider::ProvidedFileSystemInfo info(
      "testing-provider-id", chromeos::file_system_provider::MountOptions(),
      base::FilePath(), true /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, icon_set);

  file_manager::VolumeManager::Get(browser()->profile())
      ->AddVolumeForTesting(file_manager::Volume::CreateForProvidedFileSystem(
          info, file_manager::MOUNT_CONTEXT_AUTO));

  // We will call fileManagerPrivate.unmountVolume once. To test that method, we
  // check that UnmountPath is really called with the same value.
  EXPECT_CALL(*disk_mount_manager_mock_, UnmountPath(_, _, _))
      .Times(0);
  EXPECT_CALL(
      *disk_mount_manager_mock_,
      UnmountPath(chromeos::CrosDisksClient::GetRemovableDiskMountPoint()
                      .AppendASCII("mount_path1")
                      .AsUTF8Unsafe(),
                  chromeos::UNMOUNT_OPTIONS_NONE, _))
      .Times(1);
  EXPECT_CALL(*disk_mount_manager_mock_,
              UnmountPath(chromeos::CrosDisksClient::GetArchiveMountPoint()
                              .AppendASCII("archive_mount_path")
                              .AsUTF8Unsafe(),
                          chromeos::UNMOUNT_OPTIONS_LAZY, _))
      .Times(1);

  ASSERT_TRUE(RunComponentExtensionTest("file_browser/mount_test"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Permissions) {
  EXPECT_TRUE(
      RunExtensionTestIgnoreManifestWarnings("file_browser/permissions"));
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, extension->install_warnings().size());
  const extensions::InstallWarning& warning = extension->install_warnings()[0];
  EXPECT_EQ("fileManagerPrivate", warning.key);
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, OnFileChanged) {
  // In drive volume, deletion of a directory is notified via OnFileChanged.
  // Local changes directly come to HandleFileWatchNotification from
  // FileWatcher.
  typedef drive::FileChange FileChange;
  typedef drive::FileChange::FileType FileType;
  typedef drive::FileChange::ChangeType ChangeType;

  int counter = 0;
  event_router_->SetDispatchDirectoryChangeEventImplForTesting(
      base::Bind(&DispatchDirectoryChangeEventImpl, &counter));

  // /a/b/c and /a/d/e are being watched.
  event_router_->AddFileWatch(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/a/b/c")),
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs-virtual/root/a/b/c")),
      "extension_1", base::Bind(&AddFileWatchCallback));

  event_router_->AddFileWatch(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/a/d/e")),
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs-hash/root/a/d/e")),
      "extension_2", base::Bind(&AddFileWatchCallback));

  event_router_->AddFileWatch(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/aaa")),
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs-hash/root/aaa")),
      "extension_3", base::Bind(&AddFileWatchCallback));

  // event_router->addFileWatch create some tasks which are performed on
  // TaskScheduler. Wait until they are done.
  base::TaskScheduler::GetInstance()->FlushForTesting();
  // We also wait the UI thread here, since some tasks which are performed
  // above message loop back results to the UI thread.
  base::RunLoop().RunUntilIdle();

  // When /a is deleted (1 and 2 is notified).
  FileChange first_change;
  first_change.Update(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/a")),
      FileType::FILE_TYPE_DIRECTORY, ChangeType::CHANGE_TYPE_DELETE);
  event_router_->OnFileChanged(first_change);
  EXPECT_EQ(2, counter);

  // When /a/b/c is deleted (1 is notified).
  FileChange second_change;
  second_change.Update(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/a/b/c")),
      FileType::FILE_TYPE_DIRECTORY, ChangeType::CHANGE_TYPE_DELETE);
  event_router_->OnFileChanged(second_change);
  EXPECT_EQ(3, counter);

  // When /z/y is deleted (Not notified).
  FileChange third_change;
  third_change.Update(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/z/y")),
      FileType::FILE_TYPE_DIRECTORY, ChangeType::CHANGE_TYPE_DELETE);
  event_router_->OnFileChanged(third_change);
  EXPECT_EQ(3, counter);

  // Remove file watchers.
  event_router_->RemoveFileWatch(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/a/b/c")),
      "extension_1");
  event_router_->RemoveFileWatch(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/a/d/e")),
      "extension_2");
  event_router_->RemoveFileWatch(
      base::FilePath(FILE_PATH_LITERAL("/no-existing-fs/root/aaa")),
      "extension_3");

  // event_router->addFileWatch create some tasks which are performed on
  // TaskScheduler. Wait until they are done.
  base::TaskScheduler::GetInstance()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, ContentChecksum) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  AddLocalFileSystem(browser()->profile(), temp_dir.GetPath());

  ASSERT_TRUE(RunComponentExtensionTest("file_browser/content_checksum_test"));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Recent) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath downloads_dir = temp_dir.GetPath();

  ASSERT_TRUE(file_manager::VolumeManager::Get(browser()->profile())
                  ->RegisterDownloadsDirectoryForTesting(downloads_dir));

  // Create an empty file.
  {
    base::File file(downloads_dir.Append("all-justice.jpg"),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
  }

  ASSERT_TRUE(RunComponentExtensionTest("file_browser/recent_test"));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, Crostini) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableCrostiniForProfile(&scoped_feature_list);

  // Setup CrostiniManager for testing.
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(browser()->profile());
  crostini_manager->set_skip_restart_for_testing();
  vm_tools::concierge::VmInfo vm_info;
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName,
                                           std::move(vm_info));

  ExpectCrostiniMount();

  // Add 'testing' volume with 'test_dir', create 'share_dir' in Downloads.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  AddLocalFileSystem(browser()->profile(), temp_dir.GetPath());
  base::FilePath downloads;
  ASSERT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(browser()->profile()),
          &downloads));
  ASSERT_TRUE(base::CreateDirectory(downloads.AppendASCII("share_dir")));

  // Setup prefs crostini.shared_paths.
  base::FilePath shared1 = downloads.AppendASCII("shared1");
  base::FilePath shared2 = downloads.AppendASCII("shared2");
  ASSERT_TRUE(base::CreateDirectory(shared1));
  ASSERT_TRUE(base::CreateDirectory(shared2));
  base::ListValue shared_paths;
  shared_paths.AppendString(shared1.value());
  shared_paths.AppendString(shared2.value());
  browser()->profile()->GetPrefs()->Set(crostini::prefs::kCrostiniSharedPaths,
                                        shared_paths);

  ASSERT_TRUE(RunComponentExtensionTest("file_browser/crostini_test"));
}

IN_PROC_BROWSER_TEST_F(FileManagerPrivateApiTest, CrostiniIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableCrostiniForProfile(&scoped_feature_list);
  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->set_skip_restart_for_testing();
  ExpectCrostiniMount();

  scoped_refptr<extensions::FileManagerPrivateMountCrostiniFunction> function(
      new extensions::FileManagerPrivateMountCrostiniFunction());
  // Use incognito profile.
  function->set_browser_context(browser()->profile()->GetOffTheRecordProfile());

  extensions::api_test_utils::SendResponseHelper response_helper(
      function.get());
  function->RunWithValidation()->Execute();
  response_helper.WaitForResponse();
  EXPECT_TRUE(response_helper.GetResponse());
}
