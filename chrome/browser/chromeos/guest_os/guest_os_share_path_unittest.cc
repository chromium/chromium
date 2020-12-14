// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
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
      chromeos::disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

}  // namespace

namespace guest_os {

class GuestOsSharePathTest : public testing::Test {
 public:
  const bool PERSIST_YES = true;
  const bool PERSIST_NO = false;
  enum class Persist { NO, YES };
  enum class SeneschalClientCalled { NO, YES };
  enum class Success { NO, YES };

  void SharePathCallback(
      const std::string& expected_vm_name,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const vm_tools::seneschal::SharePathRequest::StorageLocation*
          expected_seneschal_storage_location,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const base::FilePath& container_path,
      bool success,
      const std::string& failure_reason) {
    const base::DictionaryValue* prefs =
        profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
    EXPECT_TRUE(prefs->HasKey(shared_path_.value()));
    EXPECT_EQ(prefs->FindKey(shared_path_.value())->GetList().size(), 1U);
    EXPECT_EQ(prefs->FindKey(shared_path_.value())->GetList()[0].GetString(),
              crostini::kCrostiniDefaultVmName);
    if (expected_persist == Persist::YES) {
      EXPECT_EQ(prefs->size(), 2U);
      EXPECT_TRUE(prefs->HasKey(share_path_.value()));
      EXPECT_EQ(prefs->FindKey(share_path_.value())->GetList().size(), 1U);
      EXPECT_EQ(prefs->FindKey(share_path_.value())->GetList()[0].GetString(),
                expected_vm_name);
    } else {
      EXPECT_EQ(prefs->size(), 1U);
    }
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
      Persist expected_persist,
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
    SharePathCallback(
        expected_vm_name, expected_persist, expected_seneschal_client_called,
        expected_seneschal_storage_location, expected_seneschal_path,
        expected_success, expected_failure_reason, container_path, success,
        failure_reason);
  }

  void SharePersistedPathsCallback(bool success,
                                   const std::string& failure_reason) {
    EXPECT_TRUE(success);
    EXPECT_EQ(profile()
                  ->GetPrefs()
                  ->GetDictionary(prefs::kGuestOSPathsSharedToVms)
                  ->size(),
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
    const base::DictionaryValue* prefs =
        profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
    if (expected_persist == Persist::YES) {
      EXPECT_TRUE(prefs->HasKey(path.value()));
    } else {
      EXPECT_FALSE(prefs->HasKey(path.value()));
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
    chromeos::DBusThreadManager::Initialize();
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
    fake_seneschal_client_ = static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
  }

  ~GuestOsSharePathTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUpVolume() {
    // Setup Downloads and path to share, which depend on MyFilesVolume flag,
    // thus can't be on SetUp.
    chromeos::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager);
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildVolumeManager));
    root_ = file_manager::util::GetMyFilesFolderForProfile(profile());
    file_manager::VolumeManager::Get(profile())
        ->RegisterDownloadsDirectoryForTesting(root_);
    share_path_ = root_.Append("path-to-share");
    shared_path_ = root_.Append("already-shared");
    ASSERT_TRUE(base::CreateDirectory(shared_path_));
    DictionaryPrefUpdate update(profile()->GetPrefs(),
                                prefs::kGuestOSPathsSharedToVms);
    base::DictionaryValue* shared_paths = update.Get();
    base::Value termina(base::Value::Type::LIST);
    termina.Append(base::Value(crostini::kCrostiniDefaultVmName));
    shared_paths->SetKey(shared_path_.value(), std::move(termina));
    volume_downloads_ = file_manager::Volume::CreateForDownloads(root_);
    guest_os_share_path_->RegisterSharedPath(crostini::kCrostiniDefaultVmName,
                                             shared_path_);
    // Run threads now to allow watcher for shared_path_ to start.
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeCrosComponentManager(component_manager_);
    chromeos::DlcserviceClient::InitializeFake();

    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    guest_os_share_path_ = GuestOsSharePath::GetForProfile(profile());

    // Setup for DriveFS.
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    account_id_ = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    GetFakeUserManager()->AddUser(account_id_);
    profile()->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
    drivefs_ =
        base::FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980");

    // Create 'vm-running' VM instance which is running.
    crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        "vm-running");

    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();

    // Create ArcSessionManager for ARCVM testing.
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
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
    chromeos::DlcserviceClient::Shutdown();
    browser_part_.ShutdownCrosComponentManager();
    component_manager_.reset();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
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

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeSeneschalClient* fake_seneschal_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<TestingProfile> profile_;
  GuestOsSharePath* guest_os_share_path_;
  base::test::ScopedFeatureList features_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  AccountId account_id_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;

  DISALLOW_COPY_AND_ASSIGN(GuestOsSharePathTest);
};

TEST_F(GuestOsSharePathTest, SuccessMyFilesRoot) {
  SetUpVolume();
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile());
  guest_os_share_path_->SharePath(
      "vm-running", my_files, PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessNoPersist) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", share_path_, PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessPersist) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", share_path_, PERSIST_YES,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessPluginVm) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "PvmDefault", share_path_, PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "PvmDefault", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

// Tests that ARCVM can share path.
TEST_F(GuestOsSharePathTest, SuccessArcvm) {
  SetUpVolume();

  // Set up VmInfo in |arc_session_manager_| to simulate a running ARCVM.
  vm_tools::concierge::VmStartedSignal start_signal;
  start_signal.set_name(arc::kArcVmName);
  start_signal.mutable_vm_info()->set_seneschal_server_handle(1000UL);
  arc_session_manager_->OnVmStarted(start_signal);

  guest_os_share_path_->SharePath(
      arc::kArcVmName, drivefs_.Append("root").Append("ArcvmTest"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), arc::kArcVmName, Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "ArcvmTest", Success::YES, ""));
  // Also validate the seneschal server handle.
  EXPECT_EQ(1000UL, fake_seneschal_client_->last_share_path_request().handle());
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsMyDrive) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("root").Append("my"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "my", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsMyDriveRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("root"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailDriveFsRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_, PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsTeamDrives) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("team_drives").Append("team"), PERSIST_NO,
      base::BindOnce(
          &GuestOsSharePathTest::SharePathCallback, base::Unretained(this),
          "vm-running", Persist::NO, SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::DRIVEFS_TEAM_DRIVES, "team",
          Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Enable when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, DISABLED_SuccessDriveFsComputersGrandRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, Bug917920DriveFsComputersGrandRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Enable when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, DISABLED_SuccessDriveFsComputerRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers").Append("pc"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
TEST_F(GuestOsSharePathTest, Bug917920DriveFsComputerRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers").Append("pc"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessDriveFsComputersLevel3) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running",
      drivefs_.Append("Computers").Append("pc").Append("SyncFolder"),
      PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc/SyncFolder", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailDriveFsTrash) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", drivefs_.Append(".Trash").Append("in-the-trash"),
      PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessRemovable) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", base::FilePath("/media/removable/MyUSB"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::REMOVABLE, "MyUSB",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, FailRemovableRoot) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", base::FilePath("/media/removable"), PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SuccessSystemFonts) {
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-running", base::FilePath("/usr/share/fonts"), PERSIST_NO,
      base::BindOnce(
          &GuestOsSharePathTest::SharePathCallback, base::Unretained(this),
          "vm-running", Persist::NO, SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::FONTS, "", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorSeneschal) {
  features_.InitWithFeatures({features::kCrostini}, {});
  GetFakeUserManager()->LoginUser(account_id_);
  SetUpVolume();
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  vm_tools::seneschal::SharePathResponse share_path_response;
  share_path_response.set_success(false);
  share_path_response.set_failure_reason("test failure");
  fake_seneschal_client_->set_share_path_response(share_path_response);

  guest_os_share_path_->SharePath(
      "error-seneschal", share_path_, PERSIST_YES,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "error-seneschal", Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::NO, "test failure"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorPathNotAbsolute) {
  SetUpVolume();
  const base::FilePath path("not/absolute/dir");
  guest_os_share_path_->SharePath(
      "vm-running", path, PERSIST_YES,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorReferencesParent) {
  SetUpVolume();
  const base::FilePath path("/path/../references/parent");
  guest_os_share_path_->SharePath(
      "vm-running", path, PERSIST_NO,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorNotUnderDownloads) {
  SetUpVolume();
  const base::FilePath path("/not/under/downloads");
  guest_os_share_path_->SharePath(
      "vm-running", path, PERSIST_YES,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathVmToBeRestarted) {
  features_.InitWithFeatures({features::kCrostini}, {});
  GetFakeUserManager()->LoginUser(account_id_);
  SetUpVolume();
  guest_os_share_path_->SharePath(
      "vm-to-be-started", share_path_, PERSIST_YES,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-to-be-started", Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePathErrorVmCouldNotBeStarted) {
  SetUpVolume();
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  guest_os_share_path_->SharePath(
      "error-vm-could-not-be-started", share_path_, PERSIST_YES,
      base::BindOnce(&GuestOsSharePathTest::SharePathCallback,
                     base::Unretained(this), "error-vm-could-not-be-started",
                     Persist::YES, SeneschalClientCalled::NO, nullptr, "",
                     Success::NO, "VM could not be started"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, SharePersistedPaths) {
  SetUpVolume();
  base::FilePath share_path2_ = root_.AppendASCII("path-to-share-2");
  ASSERT_TRUE(base::CreateDirectory(share_path2_));
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
  base::Value shared_paths(base::Value::Type::DICTIONARY);
  base::Value vms(base::Value::Type::LIST);
  vms.Append(base::Value(crostini::kCrostiniDefaultVmName));
  shared_paths.SetKey(share_path_.value(), std::move(vms));
  base::Value vms2(base::Value::Type::LIST);
  vms2.Append(base::Value(crostini::kCrostiniDefaultVmName));
  shared_paths.SetKey(share_path2_.value(), std::move(vms2));
  profile()->GetPrefs()->Set(prefs::kGuestOSPathsSharedToVms, shared_paths);
  guest_os_share_path_->SharePersistedPaths(
      crostini::kCrostiniDefaultVmName,
      base::BindOnce(&GuestOsSharePathTest::SharePersistedPathsCallback,
                     base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, RegisterPersistedPaths) {
  base::Value shared_paths(base::Value::Type::DICTIONARY);
  SetUpVolume();
  profile()->GetPrefs()->Set(prefs::kGuestOSPathsSharedToVms, shared_paths);

  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/a"));
  const base::DictionaryValue* prefs =
      profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v1");

  // Adding the same path again for same VM should not cause any changes.
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/a"));
  prefs = profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);

  // Adding the same path for a new VM adds to the vm list.
  guest_os_share_path_->RegisterPersistedPath("v2", base::FilePath("/a/a/a"));
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 2U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v1");
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[1].GetString(), "v2");

  // Add more paths.
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/b"));
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/c"));
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/b/a"));
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/b/a/a"));
  EXPECT_EQ(prefs->size(), 5U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 2U);
  EXPECT_EQ(prefs->FindKey("/a/a/b")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/c")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/b/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/b/a/a")->GetList().size(), 1U);

  // Adding /a/a should remove /a/a/a, /a/a/b, /a/a/c.
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a"));
  EXPECT_EQ(prefs->size(), 4U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v2");
  EXPECT_EQ(prefs->FindKey("/a/b/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/b/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a")->GetList()[0].GetString(), "v1");

  // Adding /a should remove /a/a, /a/b/a.
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/a"));
  EXPECT_EQ(prefs->size(), 3U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v2");
  EXPECT_EQ(prefs->FindKey("/b/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a")->GetList()[0].GetString(), "v1");

  // Adding / should remove all others.
  guest_os_share_path_->RegisterPersistedPath("v1", base::FilePath("/"));
  EXPECT_EQ(prefs->size(), 2U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v2");
  EXPECT_EQ(prefs->FindKey("/")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/")->GetList()[0].GetString(), "v1");

  // Add / for v2.
  guest_os_share_path_->RegisterPersistedPath("v2", base::FilePath("/"));
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/")->GetList().size(), 2U);
  EXPECT_EQ(prefs->FindKey("/")->GetList()[0].GetString(), "v1");
  EXPECT_EQ(prefs->FindKey("/")->GetList()[1].GetString(), "v2");
}

TEST_F(GuestOsSharePathTest, UnsharePathSuccess) {
  SetUpVolume();
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  base::Value vms(base::Value::Type::LIST);
  vms.Append(base::Value("vm-running"));
  shared_paths->SetKey(shared_path_.value(), std::move(vms));
  guest_os_share_path_->UnsharePath(
      "vm-running", shared_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles/already-shared",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathRoot) {
  SetUpVolume();
  guest_os_share_path_->UnsharePath(
      "vm-running", root_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), root_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathVmNotRunning) {
  SetUpVolume();
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  base::Value vms(base::Value::Type::LIST);
  vms.Append(base::Value("vm-not-running"));
  shared_paths->SetKey(shared_path_.value(), std::move(vms));
  guest_os_share_path_->UnsharePath(
      "vm-not-running", shared_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::YES,
                     "VM not running"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathPluginVmNotRunning) {
  SetUpVolume();
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  base::Value vms(base::Value::Type::LIST);
  vms.Append(base::Value("PvmDefault"));
  shared_paths->SetKey(shared_path_.value(), std::move(vms));
  guest_os_share_path_->UnsharePath(
      "PvmDefault", shared_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::YES,
                     "PluginVm not running"));
  run_loop()->Run();
}

// Tests that it cannot unshare path when ARCVM is not running.
TEST_F(GuestOsSharePathTest, UnsharePathArcvmNotRunning) {
  SetUpVolume();
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  base::Value vms(base::Value::Type::LIST);
  vms.Append(base::Value(arc::kArcVmName));
  shared_paths->SetKey(shared_path_.value(), std::move(vms));

  // Remove VmInfo from |arc_session_manager_| to simulate a stopped ARCVM.
  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(arc::kArcVmName);
  arc_session_manager_->OnVmStopped(stop_signal);

  guest_os_share_path_->UnsharePath(
      arc::kArcVmName, shared_path_, true,
      base::BindOnce(&GuestOsSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::YES,
                     "ARCVM not running, cannot unshare paths"));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnsharePathInvalidPath) {
  SetUpVolume();
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
  SetUpVolume();
  // path1:['vm1'], path2:['vm2'], path3:['vm3'], path12:['vm1','vm2']
  base::Value shared_paths(base::Value::Type::DICTIONARY);

  base::FilePath path1("/path1");
  base::Value path1vms(base::Value::Type::LIST);
  path1vms.Append(base::Value("vm1"));
  shared_paths.SetKey(path1.value(), std::move(path1vms));
  base::FilePath path2("/path2");
  base::Value path2vms(base::Value::Type::LIST);
  path2vms.Append(base::Value("vm2"));
  shared_paths.SetKey(path2.value(), std::move(path2vms));
  base::FilePath path3("/path3");
  base::Value path3vms(base::Value::Type::LIST);
  path3vms.Append(base::Value("vm3"));
  shared_paths.SetKey(path3.value(), std::move(path3vms));
  base::FilePath path12("/path12");
  base::Value path12vms(base::Value::Type::LIST);
  path12vms.Append(base::Value("vm1"));
  path12vms.Append(base::Value("vm2"));
  shared_paths.SetKey(path12.value(), std::move(path12vms));
  profile()->GetPrefs()->Set(prefs::kGuestOSPathsSharedToVms, shared_paths);

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
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalSharePathCallback, base::Unretained(this),
      "share-on-mount", shared_path_, crostini::kCrostiniDefaultVmName,
      Persist::NO, SeneschalClientCalled::YES,
      &vm_tools::seneschal::SharePathRequest::MY_FILES, "already-shared",
      Success::YES, ""));
  guest_os_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_downloads_);
  run_loop_->Run();
}

TEST_F(GuestOsSharePathTest, ShareOnMountSuccessSelfMount) {
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalSharePathCallback, base::Unretained(this),
      "share-on-mount", shared_path_, crostini::kCrostiniDefaultVmName,
      Persist::NO, SeneschalClientCalled::YES,
      &vm_tools::seneschal::SharePathRequest::MY_FILES, "already-shared",
      Success::YES, ""));
  guest_os_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_shared_path);
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, ShareOnMountVmNotRunning) {
  SetUpVolume();

  // Test mount.
  guest_os_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_downloads_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);

  // Test unmount.
  guest_os_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_downloads_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);
}

TEST_F(GuestOsSharePathTest, ShareOnMountVolumeUnrelated) {
  SetUpVolume();
  auto volume_unrelated_ = file_manager::Volume::CreateForDownloads(
      base::FilePath("/unrelated/path"));

  // Test mount.
  guest_os_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_unrelated_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);

  // Test unmount.
  guest_os_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_unrelated_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);
}

TEST_F(GuestOsSharePathTest, UnshareOnUnmountSuccessParentMount) {
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "unshare-on-unmount", shared_path_, Persist::YES,
      SeneschalClientCalled::YES, "MyFiles/already-shared", Success::YES, ""));
  guest_os_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_downloads_);
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnshareOnUnmountSuccessSelfMount) {
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "unshare-on-unmount", shared_path_, Persist::YES,
      SeneschalClientCalled::YES, "MyFiles/already-shared", Success::YES, ""));
  guest_os_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_shared_path);
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnshareOnDeleteMountExists) {
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
  ASSERT_TRUE(base::DeleteFile(shared_path_));
  guest_os_share_path_->set_seneschal_callback_for_testing(base::BindRepeating(
      &GuestOsSharePathTest::SeneschalUnsharePathCallback,
      base::Unretained(this), "unshare-on-delete", shared_path_, Persist::NO,
      SeneschalClientCalled::YES, "MyFiles/already-shared", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(GuestOsSharePathTest, UnshareOnDeleteMountRemoved) {
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
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
  SetUpVolume();
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);
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
  SetUpVolume();
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
}

}  // namespace guest_os
