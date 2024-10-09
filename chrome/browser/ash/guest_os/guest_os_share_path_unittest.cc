// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_share_path.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_service.pb.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */,
      ash::disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

}  // namespace

namespace guest_os {

class GuestOsSharePathTest : public testing::Test {
 public:
  enum class Persist { NO, YES };
  enum class SeneschalClientCalled { NO, YES };
  enum class Success { NO, YES };

  void SharePathCallback(
      const std::string& expected_vm_name,
      SeneschalClientCalled expected_seneschal_client_called,
      const vm_tools::seneschal::SharePathRequest::StorageLocation*
          expected_seneschal_storage_location,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const base::FilePath& container_path,
      bool success,
      const std::string& failure_reason) {
    const base::Value::Dict& prefs =
        profile()->GetPrefs()->GetDict(prefs::kGuestOSPathsSharedToVms);

    const base::Value::List* shared_path_list =
        prefs.FindList(shared_path_.value());
    ASSERT_TRUE(shared_path_list);
    EXPECT_EQ(shared_path_list->size(), 1U);
    EXPECT_EQ(shared_path_list->front().GetString(),
              crostini::kCrostiniDefaultVmName);
    EXPECT_EQ(prefs.size(), 1U);
    EXPECT_EQ(fake_seneschal_client_->share_path_called(),
              expected_seneschal_client_called == SeneschalClientCalled::YES);
    if (expected_seneschal_client_called == SeneschalClientCalled::YES) {
      EXPECT_EQ(
          fake_seneschal_client_->last_share_path_request().storage_location(),
          *expected_seneschal_storage_location);
      EXPECT_EQ(fake_seneschal_client_->last_share_path_request()
                    .shared_path()
                    .path(),
                expected_seneschal_path);
    }
    EXPECT_EQ(success, expected_success == Success::YES);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    run_loop()->Quit();
  }

  void SeneschalSharePathCallback(
      const std::string& expected_operation,
      const base::FilePath& expected_path,
      const std::string& expected_vm_name,
      SeneschalClientCalled expected_seneschal_client_called,
      const vm_tools::seneschal::SharePathRequest::StorageLocation*
          expected_seneschal_storage_location,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const std::string& operation,
      const base::FilePath& cros_path,
      const base::FilePath& container_path,
      bool success,
      const std::string& failure_reason) {
    EXPECT_EQ(expected_operation, operation);
    EXPECT_EQ(expected_path, cros_path);
    SharePathCallback(expected_vm_name, expected_seneschal_client_called,
                      expected_seneschal_storage_location,
                      expected_seneschal_path, expected_success,
                      expected_failure_reason, container_path, success,
                      failure_reason);
  }

  void SharePersistedPathsCallback(bool success,
                                   const std::string& failure_reason) {
    EXPECT_TRUE(success);
    EXPECT_EQ(
        profile()->GetPrefs()->GetDict(prefs::kGuestOSPathsSharedToVms).size(),
        2U);
    run_loop()->Quit();
  }

  void SharePathErrorVmNotRunningCallback(base::OnceClosure closure,
                                          bool success,
                                          std::string failure_reason) {
    EXPECT_FALSE(fake_seneschal_client_->share_path_called());
    EXPECT_EQ(success, false);
    EXPECT_EQ(failure_reason, "Cannot share, VM not running");
    std::move(closure).Run();
  }

  void UnsharePathCallback(
      const base::FilePath& path,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      bool success,
      const std::string& failure_reason) {
    const base::Value::Dict& prefs =
        profile()->GetPrefs()->GetDict(prefs::kGuestOSPathsSharedToVms);
    if (expected_persist == Persist::YES) {
      EXPECT_NE(prefs.Find(path.value()), nullptr);
    } else {
      EXPECT_EQ(prefs.Find(path.value()), nullptr);
    }
    EXPECT_EQ(fake_seneschal_client_->unshare_path_called(),
              expected_seneschal_client_called == SeneschalClientCalled::YES);
    if (expected_seneschal_client_called == SeneschalClientCalled::YES) {
      EXPECT_EQ(fake_seneschal_client_->last_unshare_path_request().path(),
                expected_seneschal_path);
    }
    EXPECT_EQ(success, expected_success == Success::YES);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    run_loop()->Quit();
  }

  void SeneschalUnsharePathCallback(
      const std::string expected_operation,
      const base::FilePath& expected_path,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const std::string& operation,
      const base::FilePath& cros_path,
      const base::FilePath& container_path,
      bool success,
      const std::string& failure_reason) {
    EXPECT_EQ(expected_operation, operation);
    EXPECT_EQ(expected_path, cros_path);
    UnsharePathCallback(cros_path, expected_persist,
                        expected_seneschal_client_called,
                        expected_seneschal_path, expected_success,
                        expected_failure_reason, success, failure_reason);
  }

  GuestOsSharePathTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::DebugDaemonClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    ash::VmPluginDispatcherClient::InitializeFake();

    fake_concierge_client_ = ash::FakeConciergeClient::Get();
    fake_seneschal_client_ = ash::FakeSeneschalClient::Get();
  }

  GuestOsSharePathTest(const GuestOsSharePathTest&) = delete;
  GuestOsSharePathTest& operator=(const GuestOsSharePathTest&) = delete;

  ~GuestOsSharePathTest() override {
    ash::VmPluginDispatcherClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  void SharePathFor(std::string vm_name) {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kGuestOSPathsSharedToVms);
    base::Value::List pref;
    pref.Append(vm_name);
    update->Set(shared_path_.value(), std::move(pref));
    guest_os_share_path_->RegisterSharedPath(vm_name, shared_path_);
    // Run threads now to allow watcher for shared_path_ to start.
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeComponentManager(component_manager_);
    ash::DlcserviceClient::InitializeFake();

    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    guest_os_share_path_ = GuestOsSharePathFactory::GetForProfile(profile());

    // Setup for DriveFS.
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
    account_id_ = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    GetFakeUserManager()->AddUser(account_id_);
    profile()->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
    drivefs_ =
        base::FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980");

    guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile())
        ->AddGuestForTesting(guest_os::GuestId{guest_os::VmType::UNKNOWN,
                                               "vm-running", "unused"});
    guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile())
        ->AddGuestForTesting(guest_os::GuestId{guest_os::VmType::TERMINA,
                                               crostini::kCrostiniDefaultVmName,
                                               "unused"});

    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildVolumeManager));
    root_ = file_manager::util::GetMyFilesFolderForProfile(profile());
    file_manager::VolumeManager::Get(profile())
        ->RegisterDownloadsDirectoryForTesting(root_);
    share_path_ = root_.Append("path-to-share");
    shared_path_ = root_.Append("already-shared");
    volume_downloads_ = file_manager::Volume::CreateForDownloads(root_);
    ASSERT_TRUE(base::CreateDirectory(shared_path_));
    SharePathFor(crostini::kCrostiniDefaultVmName);
  }

  void TearDown() override {
    arc_session_manager_.reset();
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    // Shutdown GuestOsSharePath to schedule FilePathWatchers to be destroyed,
    // then run thread bundle to ensure they are.
    guest_os_share_path_->Shutdown();
    task_environment_.RunUntilIdle();
    run_loop_.reset();
    scoped_user_manager_.reset();
    profile_.reset();
    ash::disks::DiskMountManager::Shutdown();
    ash::DlcserviceClient::Shutdown();
    browser_part_.ShutdownComponentManager();
    component_manager_.reset();
  }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  base::FilePath root_;
  base::FilePath share_path_;
  base::FilePath shared_path_;
  base::FilePath drivefs_;
  std::unique_ptr<file_manager::Volume> volume_downloads_;

  raw_ptr<ash::FakeSeneschalClient, DanglingUntriaged> fake_seneschal_client_;
  raw_ptr<ash::FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<GuestOsSharePath, DanglingUntriaged> guest_os_share_path_;
  base::test::ScopedFeatureList features_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  AccountId account_id_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeComponentManagerAsh> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
};

TEST_F(GuestOsSharePathTest, SuccessMyFilesRoot) {
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile());
  guest_os_share_path_->SharePath(
      "vm-running", 0, my_files,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessNoPersist) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, share_path_,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessPersist) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, share_path_,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsMyDrive) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("root").Append("my"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "my", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsMyDriveRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("root"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailDriveFsRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsTeamDrives) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("team_drives").Append("team"),
      base::BindOnce(
          &GuestOsSharePathTest::SharePathCallback, base::Unretained(this),
          "vm-running", SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::DRIVEFS_TEAM_DRIVES, "team",
          Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/40607763): Enable when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, DISABLED_SuccessDriveFsComputersGrandRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("Computers"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/40607763): Remove when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, Bug917920DriveFsComputersGrandRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("Computers"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

// TODO(crbug.com/40607763): Enable when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, DISABLED_SuccessDriveFsComputerRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("Computers").Append("pc"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/40607763): Remove when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, Bug917920DriveFsComputerRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append("Computers").Append("pc"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsComputersLevel3) {
  guest_os_share_path_->SharePath(
      "vm-running", 0,
      drivefs_.Append("Computers").Append("pc").Append("SyncFolder"),

      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc/SyncFolder", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsFilesById) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append(".files-by-id/1234/shared"),
      base::BindOnce(
          &GuestOsSharePathTest::SharePathCallback, base::Unretained(this),
          "vm-running", SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::DRIVEFS_FILES_BY_ID,
          "1234/shared", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsShortcutTargetsById) {
  guest_os_share_path_->SharePath(
      "vm-running", 0,
      drivefs_.Append(".shortcut-targets-by-id/1-abc-xyz/shortcut"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::
                         DRIVEFS_SHORTCUT_TARGETS_BY_ID,
                     "1-abc-xyz/shortcut", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailDriveFsTrash) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, drivefs_.Append(".Trash-1000").Append("in-the-trash"),

      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessRemovable) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, base::FilePath("/media/removable/MyUSB"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::REMOVABLE, "MyUSB",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailRemovableRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, base::FilePath("/media/removable"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessSystemFonts) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, base::FilePath("/usr/share/fonts"),
      base::BindOnce(
          &GuestOsSharePathTest::SharePathCallback, base::Unretained(this),
          "vm-running", SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::FONTS, "", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessGuestOs) {
  file_manager::VolumeManager::Get(profile())->AddSftpGuestOsVolume(
      "name", base::FilePath("/media/fuse/whatever"), base::FilePath("/meh"),
      guest_os::VmType::UNKNOWN);
  guest_os_share_path_->SharePath(
      "vm-running", 0, base::FilePath("/media/fuse/whatever"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::GUEST_OS_FILES, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessFusebox) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, base::FilePath("/media/fuse/fusebox/subdir"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::FUSEBOX, "subdir",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailFuseboxRoot) {
  guest_os_share_path_->SharePath(
      "vm-running", 0, base::FilePath("/media/fuse/fusebox"),
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorSeneschal) {
  features_.InitWithFeatures({features::kCrostini}, {});
  GetFakeUserManager()->LoginUser(account_id_);
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  vm_tools::seneschal::SharePathResponse share_path_response;
  share_path_response.set_success(false);
  share_path_response.set_failure_reason("test failure");
  fake_seneschal_client_->set_share_path_response(share_path_response);

  guest_os_share_path_->SharePath(
      "error-seneschal", 0, share_path_,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "error-seneschal",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::NO, "test failure"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorPathNotAbsolute) {
  const base::FilePath path("not/absolute/dir");
  guest_os_share_path_->SharePath(
      "vm-running", 0, path,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorReferencesParent) {
  const base::FilePath path("/path/../references/parent");
  guest_os_share_path_->SharePath(
      "vm-running", 0, path,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorNotUnderDownloads) {
  const base::FilePath path("/not/under/downloads");
  guest_os_share_path_->SharePath(
      "vm-running", 0, path,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running",
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathVmToBeRestarted) {
  features_.InitWithFeatures({features::kCrostini}, {});
  GetFakeUserManager()->LoginUser(account_id_);
  guest_os_share_path_->SharePath(
      "vm-to-be-started", 0, share_path_,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-to-be-started",
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePersistedPaths) {
  base::FilePath share_path2_ = root_.AppendASCII("path-to-share-2");
  ASSERT_TRUE(base::CreateDirectory(share_path2_));
  base::Value::Dict shared_paths;
  base::Value::List vms;
  vms.Append(base::Value(crostini::kCrostiniDefaultVmName));
  shared_paths.Set(share_path_.value(), std::move(vms));
  base::Value::List vms2;
  vms2.Append(base::Value(crostini::kCrostiniDefaultVmName));
  shared_paths.Set(share_path2_.value(), std::move(vms2));
  profile()->GetPrefs()->SetDict(prefs::kGuestOSPathsSharedToVms,
                                 std::move(shared_paths));
  guest_os_share_path_->SharePersistedPaths(
      crostini::kCrostiniDefaultVmName, 0,
      base::BindOnce(&GuestOsSharePathTest::SharePersistedPathsCallback,
                     base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, RegisterPersistedPaths) {
  base::Value::Dict shared_paths;
  profile()->GetPrefs()->SetDict(prefs::kGuestOSPathsSharedToVms,
                                 std::move(shared_paths));

  guest_os_share_path_->RegisterPersistedPaths("v1",
                                               {base::FilePath("/a/a/a")});
  const base::Value::Dict& prefs =
      profile()->GetPrefs()->GetDict(prefs::kGuestOSPathsSharedToVms);
  EXPECT_EQ(prefs.size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->front().GetString(), "v1");

  // Adding the same path again for same VM should not cause any changes.
  guest_os_share_path_->RegisterPersistedPaths("v1",
                                               {base::FilePath("/a/a/a")});
  EXPECT_EQ(prefs.size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 1U);

  // Adding the same path for a new VM adds to the vm list.
  guest_os_share_path_->RegisterPersistedPaths("v2",
                                               {base::FilePath("/a/a/a")});
  EXPECT_EQ(prefs.size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 2U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->front().GetString(), "v1");
  EXPECT_EQ(prefs.FindList("/a/a/a")->back().GetString(), "v2");

  // Add more paths.
  guest_os_share_path_->RegisterPersistedPaths(
      "v1", {base::FilePath("/a/a/b"), base::FilePath("/a/a/c"),
             base::FilePath("/a/b/a"), base::FilePath("/b/a/a")});
  EXPECT_EQ(prefs.size(), 5U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 2U);
  EXPECT_EQ(prefs.FindList("/a/a/b")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/c")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/b/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/b/a/a")->size(), 1U);

  // Adding /a/a should remove /a/a/a, /a/a/b, /a/a/c.
  guest_os_share_path_->RegisterPersistedPaths("v1", {base::FilePath("/a/a")});
  EXPECT_EQ(prefs.size(), 4U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->front().GetString(), "v2");
  EXPECT_EQ(prefs.FindList("/a/b/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/b/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a")->front().GetString(), "v1");

  // Adding /a should remove /a/a, /a/b/a.
  guest_os_share_path_->RegisterPersistedPaths("v1", {base::FilePath("/a")});
  EXPECT_EQ(prefs.size(), 3U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->front().GetString(), "v2");
  EXPECT_EQ(prefs.FindList("/b/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a")->front().GetString(), "v1");

  // Adding / should remove all others.
  guest_os_share_path_->RegisterPersistedPaths("v1", {base::FilePath("/")});
  EXPECT_EQ(prefs.size(), 2U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/a/a/a")->front().GetString(), "v2");
  EXPECT_EQ(prefs.FindList("/")->size(), 1U);
  EXPECT_EQ(prefs.FindList("/")->front().GetString(), "v1");

  // Add / for v2.
  guest_os_share_path_->RegisterPersistedPaths("v2", {base::FilePath("/")});
  EXPECT_EQ(prefs.size(), 1U);
  EXPECT_EQ(prefs.FindList("/")->size(), 2U);
  EXPECT_EQ(prefs.FindList("/")->front().GetString(), "v1");
  EXPECT_EQ(prefs.FindList("/")->back().GetString(), "v2");
}

TEST_F(GuestOsSharePathTest, UnsharePathSuccess) {
  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::Value::List vms;
  vms.Append("vm-running");
  update->Set(shared_path_.value(), std::move(vms));
  guest_os_share_path_->UnsharePath(
      "vm-running", shared_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles/already-shared",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathRoot) {
  guest_os_share_path_->UnsharePath(
      "vm-running", root_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), root_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathVmNotRunning) {
  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::Value::List vms;
  vms.Append("vm-not-running");
  update->Set(shared_path_.value(), std::move(vms));
  guest_os_share_path_->UnsharePath(
      "vm-not-running", shared_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::YES,
                     "VM not running"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathInvalidPath) {
  base::FilePath invalid("invalid/path");
  guest_os_share_path_->UnsharePath(
      "vm-running", invalid, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), invalid, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::NO,
                     "Invalid path to unshare"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, GetPersistedSharedPaths) {
  // path1:['vm1'], path2:['vm2'], path3:['vm3'], path12:['vm1','vm2']
  base::Value::Dict shared_paths;

  base::FilePath path1("/path1");
  base::Value::List path1vms;
  path1vms.Append(base::Value("vm1"));
  shared_paths.Set(path1.value(), std::move(path1vms));
  base::FilePath path2("/path2");
  base::Value::List path2vms;
  path2vms.Append(base::Value("vm2"));
  shared_paths.Set(path2.value(), std::move(path2vms));
  base::FilePath path3("/path3");
  base::Value::List path3vms;
  path3vms.Append(base::Value("vm3"));
  shared_paths.Set(path3.value(), std::move(path3vms));
  base::FilePath path12("/path12");
  base::Value::List path12vms;
  path12vms.Append(base::Value("vm1"));
  path12vms.Append(base::Value("vm2"));
  shared_paths.Set(path12.value(), std::move(path12vms));
  profile()->GetPrefs()->SetDict(prefs::kGuestOSPathsSharedToVms,
                                 std::move(shared_paths));

  std::vector<base::FilePath> paths =
      guest_os_share_path_->GetPersistedSharedPaths("vm1");
  std::sort(paths.begin(), paths.end());
  EXPECT_EQ(paths.size(), 2U);
  EXPECT_EQ(paths[0], path1);
  EXPECT_EQ(paths[1], path12);

  paths = guest_os_share_path_->GetPersistedSharedPaths("vm2");
  std::sort(paths.begin(), paths.end());
  EXPECT_EQ(paths.size(), 2U);
  EXPECT_EQ(paths[0], path12);
  EXPECT_EQ(paths[1], path2);

  paths = guest_os_share_path_->GetPersistedSharedPaths("vm3");
  EXPECT_EQ(paths.size(), 1U);
  EXPECT_EQ(paths[0], path3);

  paths = guest_os_share_path_->GetPersistedSharedPaths("vm4");
  EXPECT_EQ(paths.size(), 0U);
}

TEST_F(GuestOsSharePathTest, ShareOnMountSuccessParentMount) {
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalSharePathCallback, base::Unretained(this),
      "share-on-mount", shared_path_, crostini::kCrostiniDefaultVmName,
      SeneschalClientCalled::YES,
      &vm_tools::seneschal::SharePathRequest::MY_FILES, "already-shared",
      Success::YES, ""));
  guest_os_share_path_->OnVolumeMounted(ash::MountError::kSuccess,
                                        *volume_downloads_);
  run_loop_->Run();
}

TEST_F(GuestOsSharePathTest, ShareOnMountSuccessSelfMount) {
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalSharePathCallback, base::Unretained(this),
      "share-on-mount", shared_path_, crostini::kCrostiniDefaultVmName,
      SeneschalClientCalled::YES,
      &vm_tools::seneschal::SharePathRequest::MY_FILES, "already-shared",
      Success::YES, ""));
  guest_os_share_path_->OnVolumeMounted(ash::MountError::kSuccess,
                                        *volume_shared_path);
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, ShareOnMountVmNotRunning) {
  // Our test setup mocks out a running VM called kCrostiniDefaultVmName, since
  // the other tests with SetUpVolume also use it. Shut it down first so we can
  // test the not running case.
  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(crostini::kCrostiniDefaultVmName);
  fake_concierge_client_->NotifyVmStopped(stop_signal);

  // Test mount.
  guest_os_share_path_->OnVolumeMounted(ash::MountError::kSuccess,
                                        *volume_downloads_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);

  // Test unmount.
  guest_os_share_path_->OnVolumeUnmounted(ash::MountError::kSuccess,
                                          *volume_downloads_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);
}

TEST_F(GuestOsSharePathTest, ShareOnMountVolumeUnrelated) {
  auto volume_unrelated_ = file_manager::Volume::CreateForDownloads(
      base::FilePath("/unrelated/path"));

  // Test mount.
  guest_os_share_path_->OnVolumeMounted(ash::MountError::kSuccess,
                                        *volume_unrelated_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);

  // Test unmount.
  guest_os_share_path_->OnVolumeUnmounted(ash::MountError::kSuccess,
                                          *volume_unrelated_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);
}

TEST_F(GuestOsSharePathTest, UnshareOnUnmountSuccessParentMount) {
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "unshare-on-unmount", shared_path_, Persist::YES,
      SeneschalClientCalled::YES, "MyFiles/already-shared", Success::YES, ""));
  guest_os_share_path_->OnVolumeUnmounted(ash::MountError::kSuccess,
                                          *volume_downloads_);
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnshareOnUnmountSuccessSelfMount) {
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "unshare-on-unmount", shared_path_, Persist::YES,
      SeneschalClientCalled::YES, "MyFiles/already-shared", Success::YES, ""));
  guest_os_share_path_->OnVolumeUnmounted(ash::MountError::kSuccess,
                                          *volume_shared_path);
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnshareOnDeleteMountExists) {
  ASSERT_TRUE(base::DeleteFile(shared_path_));
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "unshare-on-delete", shared_path_, Persist::NO,
      SeneschalClientCalled::YES, "MyFiles/already-shared", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnshareOnDeleteMountRemoved) {
  // Rename root_ rather than delete to mimic atomic removal of mount.
  base::FilePath renamed =
      root_.DirName().Append(root_.BaseName().value() + ".tmp");
  ASSERT_TRUE(base::Move(root_, renamed));
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "ignore-delete-before-unmount", shared_path_,
      Persist::YES, SeneschalClientCalled::NO, "MyFiles/already-shared",
      Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, RegisterPathThenUnshare) {
  guest_os_share_path_->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                           share_path_);
  guest_os_share_path_->UnsharePath(
      crostini::kCrostiniDefaultVmName, share_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), share_path_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles/path-to-share",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, IsPathShared) {
  // shared_path_ and children paths are shared for 'termina'.
  for (auto& path : {shared_path_, shared_path_.Append("a.txt"),
                     shared_path_.Append("a"), shared_path_.Append("a/b")}) {
    EXPECT_TRUE(guest_os_share_path_->IsPathShared(
        crostini::kCrostiniDefaultVmName, path));
  }
  // Any parent paths are not shared.
  for (auto& path : {shared_path_.DirName(), root_}) {
    EXPECT_FALSE(guest_os_share_path_->IsPathShared(
        crostini::kCrostiniDefaultVmName, path));
  }

  // No paths are shared for 'not-shared' VM.
  for (auto& path :
       {shared_path_, shared_path_.Append("a.txt"), shared_path_.Append("a"),
        shared_path_.Append("a/b"), shared_path_.DirName(), root_}) {
    EXPECT_FALSE(guest_os_share_path_->IsPathShared("not-shared", path));
  }

  // IsPathShared should be false after VM shutdown.
  vm_tools::concierge::VmStoppedSignal signal;
  signal.set_name(crostini::kCrostiniDefaultVmName);
  signal.set_owner_id("test");
  fake_concierge_client_->NotifyVmStopped(signal);
  EXPECT_FALSE(guest_os_share_path_->IsPathShared(
      crostini::kCrostiniDefaultVmName, shared_path_));
}

class MockSharePathObserver : public GuestOsSharePath::Observer {
 public:
  MOCK_METHOD(void,
              OnPersistedPathRegistered,
              (const std::string& vm_name, const base::FilePath& path),
              (override));
  MOCK_METHOD(void,
              OnUnshare,
              (const std::string& vm_name, const base::FilePath& path),
              (override));
  MOCK_METHOD(void,
              OnGuestRegistered,
              (const guest_os::GuestId& guest),
              (override));
  MOCK_METHOD(void,
              OnGuestUnregistered,
              (const guest_os::GuestId& guest),
              (override));
};

TEST_F(GuestOsSharePathTest, RegisterListAndUnregister) {
  using testing::_;
  using testing::InSequence;
  using testing::UnorderedElementsAreArray;
  GuestId termina_1{VmType::TERMINA, "termina", "first"};
  GuestId termina_2{VmType::TERMINA, "termina", "second"};
  GuestId other_vm{VmType::TERMINA, "not-termina", "whatever"};
  MockSharePathObserver obs;
  guest_os_share_path_->AddObserver(&obs);
  std::vector guests{termina_1, termina_2, other_vm};

  // We'll first register three guests...
  EXPECT_CALL(obs, OnGuestRegistered(_)).Times(guests.size());

  // ...then unregister them in a specific order, so we expect termina_2 and
  // other_vm to be the last guests for their respective VMs.
  {
    InSequence seq;
    EXPECT_CALL(obs, OnGuestUnregistered(termina_1));  // termina_1;
    EXPECT_CALL(obs, OnGuestUnregistered(termina_2));  // termina_2;
    EXPECT_CALL(obs, OnGuestUnregistered(other_vm));   // other_vm;
  }

  for (const auto& guest : guests) {
    guest_os_share_path_->RegisterGuest(guest);
  }

  EXPECT_THAT(guest_os_share_path_->ListGuests(),
              UnorderedElementsAreArray(guests));

  for (const auto& guest : guests) {
    guest_os_share_path_->UnregisterGuest(guest);
  }
}

TEST_F(GuestOsSharePathTest, GetAndSetFirstForSession) {
  ASSERT_TRUE(guest_os_share_path_->GetAndSetFirstForSession("first"));
  ASSERT_TRUE(guest_os_share_path_->GetAndSetFirstForSession("second"));
  ASSERT_FALSE(guest_os_share_path_->GetAndSetFirstForSession("second"));
}

}  // namespace guest_os
