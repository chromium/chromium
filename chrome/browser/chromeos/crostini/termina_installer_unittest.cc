// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/termina_installer.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace crostini {

class TerminaInstallTest : public testing::Test {
 public:
  TerminaInstallTest() : browser_part_(g_browser_process->platform_part()) {}

  void CommonSetUp() {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    browser_part_.InitializeCrosComponentManager(component_manager_);
    chromeos::DlcserviceClient::InitializeFake();
    fake_dlc_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
    fake_dlc_client_->set_install_root_path(dlc_root_path_);
  }

  void SetUp() override {
    this->CommonSetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrostiniEnableDlc},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    chromeos::DlcserviceClient::Shutdown();
    browser_part_.ShutdownCrosComponentManager();
    component_manager_.reset();
  }

  void ExpectTrue(bool result) {
    EXPECT_TRUE(result);
    run_loop_.Quit();
  }

  void ExpectFalse(bool result) {
    EXPECT_FALSE(result);
    run_loop_.Quit();
  }

  void ExpectSuccess(TerminaInstaller::InstallResult result) {
    EXPECT_EQ(result, TerminaInstaller::InstallResult::Success);
    run_loop_.Quit();
  }

  void ExpectSuccess2(TerminaInstaller::InstallResult result) {
    EXPECT_EQ(result, TerminaInstaller::InstallResult::Success);
    run_loop_2_.Quit();
  }

  void ExpectFailure(TerminaInstaller::InstallResult result) {
    EXPECT_EQ(result, TerminaInstaller::InstallResult::Failure);
    run_loop_.Quit();
  }

  void ExpectNeedUpdate(TerminaInstaller::InstallResult result) {
    EXPECT_EQ(result, TerminaInstaller::InstallResult::NeedUpdate);
    run_loop_.Quit();
  }

  void ExpectNotCalled(TerminaInstaller::InstallResult result) {
    ASSERT_TRUE(false) << "Callback was run unexpectedly";
  }

  void ExpectOffline(TerminaInstaller::InstallResult result) {
    EXPECT_EQ(result, TerminaInstaller::InstallResult::Offline);
    run_loop_.Quit();
  }

  void InjectDlc() {
    dlcservice::DlcsWithContent dlcs;
    auto* dlc_info = dlcs.add_dlc_infos();
    dlc_info->set_id(kCrostiniDlcName);
    fake_dlc_client_->set_dlcs_with_content(dlcs);
  }

  const base::FilePath component_install_path_ =
      base::FilePath("/install/path");
  const base::FilePath component_mount_path_ = base::FilePath("/mount/path");
  using ComponentError = component_updater::CrOSComponentManager::Error;
  using ComponentInfo =
      component_updater::FakeCrOSComponentManager::ComponentInfo;

 protected:
  base::test::ScopedFeatureList feature_list_;

  void PrepareComponentForLoad() {
    component_manager_->set_supported_components(
        {imageloader::kTerminaComponentName});
    component_manager_->ResetComponentState(
        imageloader::kTerminaComponentName,
        ComponentInfo(ComponentError::NONE, component_install_path_,
                      component_mount_path_));
  }

  const std::string dlc_root_path_ = "/dlc/root/path";

  void CheckDlcInstallCalledTimes(int times) {
    base::RunLoop run_loop;

    fake_dlc_client_->GetExistingDlcs(base::BindOnce(
        [](base::OnceClosure quit, int times, const std::string& err,
           const dlcservice::DlcsWithContent& dlcs_with_content) {
          std::move(quit).Run();
          ASSERT_EQ(dlcs_with_content.dlc_infos_size(), times);
          for (auto dlc : dlcs_with_content.dlc_infos()) {
            EXPECT_EQ(dlc.id(), kCrostiniDlcName);
          }
        },
        run_loop.QuitClosure(), times));

    run_loop.Run();
  }

  void ExpectDlcInstalled() {
    EXPECT_EQ(termina_installer_.GetInstallLocation(),
              base::FilePath(dlc_root_path_));
    EXPECT_EQ(termina_installer_.GetDlcId(), "termina-dlc");
  }

  void ExpectComponentInstalled() {
    EXPECT_TRUE(component_manager_->IsRegisteredMayBlock(
        imageloader::kTerminaComponentName));
    EXPECT_EQ(termina_installer_.GetInstallLocation(),
              base::FilePath(component_mount_path_));
    EXPECT_EQ(termina_installer_.GetDlcId(), base::nullopt);
  }

 protected:
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
  chromeos::FakeDlcserviceClient* fake_dlc_client_;
  TerminaInstaller termina_installer_;
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_;
  base::RunLoop run_loop_2_;
};

// Specialization of TerminaInstallTest that force-enables installing via DLC
class TerminaDlcInstallTest : public TerminaInstallTest {
 public:
  TerminaDlcInstallTest() = default;

  void SetUp() override {
    this->CommonSetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrostiniUseDlc,
                              chromeos::features::kCrostiniEnableDlc},
        /*disabled_features=*/{});
  }
};

// Specialization of TerminaInstallTest that force-disables installing via DLC
class TerminaComponentInstallTest : public TerminaInstallTest {
 public:
  TerminaComponentInstallTest() = default;

  void SetUp() override {
    this->CommonSetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrostiniEnableDlc},
        /*disabled_features=*/{chromeos::features::kCrostiniUseDlc});
  }
};

// Specialization of TerminaInstallTest that enables installing via DLC but DLC
// isn't enabled
class TerminaDlcDisabledInstallTest : public TerminaInstallTest {
 public:
  TerminaDlcDisabledInstallTest() = default;

  void SetUp() override {
    this->CommonSetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kCrostiniUseDlc},
        /*disabled_features=*/{chromeos::features::kCrostiniEnableDlc});
  }
};

TEST_F(TerminaInstallTest, UninstallWithNothingInstalled) {
  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectTrue, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TerminaInstallTest, UninstallWithNothingInstalledListError) {
  fake_dlc_client_->set_get_existing_dlcs_error("An error");

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectFalse, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TerminaInstallTest, UninstallWithNothingInstalledUninstallError) {
  // These should be ignored because nothing needs to be uninstalled
  component_manager_->set_unload_component_result(false);
  fake_dlc_client_->set_uninstall_error("An error");

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectTrue, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TerminaInstallTest, UninstallWithComponentInstalled) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectTrue, base::Unretained(this)));
  run_loop_.Run();

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaInstallTest, UninstallWithComponentInstalledError) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});
  component_manager_->set_unload_component_result(false);

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectFalse, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TerminaInstallTest, UninstallWithDlcInstalled) {
  InjectDlc();

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectTrue, base::Unretained(this)));
  run_loop_.Run();

  CheckDlcInstallCalledTimes(0);
}

TEST_F(TerminaInstallTest, UninstallWithDlcInstalledUninstallError) {
  InjectDlc();
  fake_dlc_client_->set_uninstall_error("An error");

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectFalse, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TerminaDlcDisabledInstallTest,
       UninstallWithDlcDisabledUninstallErrorDoesntFail) {
  InjectDlc();
  fake_dlc_client_->set_uninstall_error("An error");

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectTrue, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TerminaInstallTest, UninstallWithBothInstalled) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});
  InjectDlc();

  termina_installer_.Uninstall(
      base::BindOnce(&TerminaInstallTest::ExpectTrue, base::Unretained(this)));
  run_loop_.Run();

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
  CheckDlcInstallCalledTimes(0);
}

TEST_F(TerminaDlcInstallTest, InstallDlc) {
  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();
}

TEST_F(TerminaDlcInstallTest, InstallDlcError) {
  fake_dlc_client_->set_install_error("An error");

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectFailure,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();
}

TEST_F(TerminaDlcInstallTest, InstallDlcNeedsReboot) {
  fake_dlc_client_->set_install_error(dlcservice::kErrorNeedReboot);

  termina_installer_.Install(
      base::BindOnce(&TerminaInstallTest::ExpectNeedUpdate,
                     base::Unretained(this)),
      /*is_initial_install=*/true);
  run_loop_.Run();
}

TEST_F(TerminaDlcInstallTest, InstallDlcNoImageFound) {
  fake_dlc_client_->set_install_error(dlcservice::kErrorNoImageFound);

  termina_installer_.Install(
      base::BindOnce(&TerminaInstallTest::ExpectNeedUpdate,
                     base::Unretained(this)),
      /*is_initial_install=*/true);
  run_loop_.Run();
}

TEST_F(TerminaDlcInstallTest, InstallDlcBusyTriggersRetry) {
  fake_dlc_client_->set_install_error(dlcservice::kErrorBusy);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(0));

  fake_dlc_client_->set_install_error(dlcservice::kErrorNone);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(2);
  ExpectDlcInstalled();
}

TEST_F(TerminaDlcInstallTest, InstallDlcBusyRetryIsCancelable) {
  fake_dlc_client_->set_install_error(dlcservice::kErrorBusy);

  termina_installer_.Install(
      base::BindOnce(&TerminaInstallTest::ExpectNotCalled,
                     base::Unretained(this)),
      /*is_initial_install=*/true);
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(0));

  CheckDlcInstallCalledTimes(1);

  termina_installer_.Cancel();

  task_env_.FastForwardBy(base::TimeDelta::FromDays(1));

  CheckDlcInstallCalledTimes(1);
}

TEST_F(TerminaDlcInstallTest, InstallDlcBusyDoesntTriggerRetry) {
  fake_dlc_client_->set_install_error(dlcservice::kErrorBusy);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectFailure,
                                            base::Unretained(this)),
                             /*is_initial_install=*/false);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(1);
}

TEST_F(TerminaDlcInstallTest, InstallDlcOffline) {
  fake_dlc_client_->set_install_error("An error");

  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectOffline,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();
}

TEST_F(TerminaDlcInstallTest, InstallDlcWithComponentInstalled) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();

  task_env_.RunUntilIdle();
  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaDlcInstallTest, InstallDlcWithComponentInstalledUninstallError) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});
  component_manager_->set_unload_component_result(false);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();
}

TEST_F(TerminaDlcInstallTest, InstallDlcFallback) {
  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/false);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();
}

TEST_F(TerminaDlcInstallTest, InstallDlcFallbackError) {
  fake_dlc_client_->set_install_error("An error");
  PrepareComponentForLoad();

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/false);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(1);
  ExpectComponentInstalled();
}

TEST_F(TerminaDlcInstallTest, InstallDlcFallbackIsCancelable) {
  fake_dlc_client_->set_install_error("An error");
  PrepareComponentForLoad();

  termina_installer_.Install(
      base::BindOnce(&TerminaInstallTest::ExpectNotCalled,
                     base::Unretained(this)),
      /*is_initial_install=*/false);
  termina_installer_.Cancel();

  task_env_.FastForwardBy(base::TimeDelta::FromDays(1));

  CheckDlcInstallCalledTimes(1);
  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaDlcInstallTest, InstallDlcFallbackOffline) {
  fake_dlc_client_->set_install_error("An error");
  PrepareComponentForLoad();

  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectOffline,
                                            base::Unretained(this)),
                             /*is_initial_install=*/false);
  run_loop_.Run();

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaDlcInstallTest,
       InstallDlcFallbackOfflineComponentAlreadyInstalled) {
  fake_dlc_client_->set_install_error("An error");
  PrepareComponentForLoad();
  component_manager_->RegisterCompatiblePath(imageloader::kTerminaComponentName,
                                             component_install_path_);

  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/false);
  run_loop_.Run();

  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, InstallComponent) {
  PrepareComponentForLoad();

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();

  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, InstallComponentOffline) {
  PrepareComponentForLoad();
  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectOffline,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();
}

TEST_F(TerminaComponentInstallTest, InstallComponentWithDlcInstalled) {
  PrepareComponentForLoad();
  InjectDlc();

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();

  CheckDlcInstallCalledTimes(0);
  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, InstallComponentWithDlcInstalledError) {
  PrepareComponentForLoad();
  InjectDlc();
  fake_dlc_client_->set_uninstall_error("An error");

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  run_loop_.Run();

  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, LoadComponentAlreadyInstalled) {
  component_manager_->set_supported_components(
      {imageloader::kTerminaComponentName});
  component_manager_->set_queue_load_requests(true);
  component_manager_->RegisterCompatiblePath(imageloader::kTerminaComponentName,
                                             component_install_path_);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));
  run_loop_.Run();
  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, LoadComponentInitiallyOffline) {
  component_manager_->set_supported_components(
      {imageloader::kTerminaComponentName});
  component_manager_->set_queue_load_requests(true);
  component_manager_->RegisterCompatiblePath(imageloader::kTerminaComponentName,
                                             component_install_path_);
  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_FALSE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));

  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess2,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));
  run_loop_.Run();
  ExpectComponentInstalled();

  run_loop_2_.Run();
  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, ComponentUpdatesOnlyOnce) {
  component_manager_->set_supported_components(
      {imageloader::kTerminaComponentName});
  component_manager_->set_queue_load_requests(true);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));
  run_loop_.Run();

  termina_installer_.Install(base::DoNothing(),
                             /*is_initial_install=*/true);
  EXPECT_FALSE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));

  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, UpdateComponentErrorRetry) {
  component_manager_->set_supported_components(
      {imageloader::kTerminaComponentName});
  component_manager_->set_queue_load_requests(true);
  component_manager_->RegisterCompatiblePath(imageloader::kTerminaComponentName,
                                             component_install_path_);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::INSTALL_FAILURE, base::FilePath(),
                    base::FilePath()));

  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_FALSE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess2,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));

  run_loop_.Run();
  ExpectComponentInstalled();

  run_loop_2_.Run();
  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, InstallComponentErrorNoRetry) {
  component_manager_->set_supported_components(
      {imageloader::kTerminaComponentName});
  component_manager_->set_queue_load_requests(true);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectFailure,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::INSTALL_FAILURE, base::FilePath(),
                    base::FilePath()));

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess2,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));

  run_loop_.Run();
  run_loop_2_.Run();
  ExpectComponentInstalled();
}

TEST_F(TerminaComponentInstallTest, UpdateInProgressTriggersRetry) {
  component_manager_->set_supported_components(
      {imageloader::kTerminaComponentName});
  component_manager_->set_queue_load_requests(true);

  termina_installer_.Install(base::BindOnce(&TerminaInstallTest::ExpectSuccess,
                                            base::Unretained(this)),
                             /*is_initial_install=*/true);
  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::UPDATE_IN_PROGRESS, base::FilePath(),
                    base::FilePath()));

  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(6));

  EXPECT_TRUE(component_manager_->HasPendingInstall(
      imageloader::kTerminaComponentName));
  EXPECT_TRUE(
      component_manager_->UpdateRequested(imageloader::kTerminaComponentName));
  component_manager_->FinishLoadRequest(
      imageloader::kTerminaComponentName,
      ComponentInfo(ComponentError::NONE, component_install_path_,
                    component_mount_path_));

  run_loop_.Run();
  ExpectComponentInstalled();
}

}  // namespace crostini
