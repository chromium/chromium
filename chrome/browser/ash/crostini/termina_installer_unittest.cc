// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/termina_installer.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using base::test::TestFuture;

namespace crostini {

class TerminaInstallTest : public testing::Test {
 public:
  TerminaInstallTest() : browser_part_(g_browser_process->platform_part()) {}

  void CommonSetUp() {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    browser_part_.InitializeComponentManager(component_manager_);
    fake_dlc_client_.set_install_root_path(dlc_root_path_);
  }

  void SetUp() override {
    this->CommonSetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    browser_part_.ShutdownComponentManager();
    component_manager_.reset();
  }

  void InjectDlc() {
    dlcservice::DlcsWithContent dlcs;
    auto* dlc_info = dlcs.add_dlc_infos();
    dlc_info->set_id(kCrostiniDlcName);
    fake_dlc_client_.set_dlcs_with_content(dlcs);
  }

  const base::FilePath component_install_path_ =
      base::FilePath("/install/path");
  const base::FilePath component_mount_path_ = base::FilePath("/mount/path");
  using ComponentError = component_updater::ComponentManagerAsh::Error;
  using ComponentInfo =
      component_updater::FakeComponentManagerAsh::ComponentInfo;

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
    TestFuture<std::string_view, const dlcservice::DlcsWithContent&>
        result_future;
    fake_dlc_client_.GetExistingDlcs(result_future.GetCallback());

    const dlcservice::DlcsWithContent& dlcs_with_content =
        result_future.Get<1>();
    ASSERT_EQ(dlcs_with_content.dlc_infos_size(), times);
    for (auto dlc : dlcs_with_content.dlc_infos()) {
      EXPECT_EQ(dlc.id(), kCrostiniDlcName);
    }
  }

  void ExpectDlcInstalled() {
    EXPECT_EQ(termina_installer_.GetInstallLocation(),
              base::FilePath(dlc_root_path_));
    EXPECT_EQ(termina_installer_.GetDlcId(), "termina-dlc");
  }

 protected:
  scoped_refptr<component_updater::FakeComponentManagerAsh> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
  ash::FakeDlcserviceClient fake_dlc_client_;
  TerminaInstaller termina_installer_;
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TerminaInstallTest, UninstallWithNothingInstalled) {
  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());
}

TEST_F(TerminaInstallTest, UninstallWithNothingInstalledListError) {
  fake_dlc_client_.set_get_existing_dlcs_error("An error");

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(TerminaInstallTest, UninstallWithNothingInstalledUninstallError) {
  // These should be ignored because nothing needs to be uninstalled
  component_manager_->set_unload_component_result(false);
  fake_dlc_client_.set_uninstall_error("An error");

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());
}

TEST_F(TerminaInstallTest, UninstallWithComponentInstalled) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaInstallTest, UninstallWithComponentInstalledError) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});
  component_manager_->set_unload_component_result(false);

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(TerminaInstallTest, UninstallWithDlcInstalled) {
  InjectDlc();

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());

  CheckDlcInstallCalledTimes(0);
}

TEST_F(TerminaInstallTest, UninstallWithDlcInstalledUninstallError) {
  InjectDlc();
  fake_dlc_client_.set_uninstall_error("An error");

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(TerminaInstallTest, UninstallWithBothInstalled) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});
  InjectDlc();

  TestFuture<bool> result_future;
  termina_installer_.Uninstall(result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
  CheckDlcInstallCalledTimes(0);
}

TEST_F(TerminaInstallTest, InstallDlc) {
  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Success, result_future.Get());

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();
}

TEST_F(TerminaInstallTest, InstallDlcCancell) {
  fake_dlc_client_.set_install_error(dlcservice::kErrorBusy);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());

  // The installer should *not* complete until dlcservice stops being busy.
  task_env_.RunUntilIdle();
  termina_installer_.CancelInstall();
  task_env_.RunUntilIdle();
  EXPECT_FALSE(result_future.IsReady());

  task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(result_future.IsReady());
  EXPECT_EQ(TerminaInstaller::InstallResult::Cancelled, result_future.Get());
}

TEST_F(TerminaInstallTest, InstallDlcError) {
  fake_dlc_client_.set_install_error("An error");

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Failure, result_future.Get());
}

TEST_F(TerminaInstallTest, InstallDlcNeedsReboot) {
  fake_dlc_client_.set_install_error(dlcservice::kErrorNeedReboot);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::NeedUpdate, result_future.Get());
}

TEST_F(TerminaInstallTest, InstallDlcNoImageFound) {
  fake_dlc_client_.set_install_error(dlcservice::kErrorNoImageFound);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::NeedUpdate, result_future.Get());
}

TEST_F(TerminaInstallTest, InstallDlcBusyTriggersRetry) {
  fake_dlc_client_.set_install_error(dlcservice::kErrorBusy);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  task_env_.FastForwardBy(base::Seconds(0));

  fake_dlc_client_.set_install_error(dlcservice::kErrorNone);
  EXPECT_EQ(TerminaInstaller::InstallResult::Success, result_future.Get());

  CheckDlcInstallCalledTimes(2);
  ExpectDlcInstalled();
}

TEST_F(TerminaInstallTest, InstallDlcBusyRetryIsCancelable) {
  fake_dlc_client_.set_install_error(dlcservice::kErrorBusy);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  task_env_.FastForwardBy(base::Seconds(0));

  CheckDlcInstallCalledTimes(1);

  termina_installer_.CancelInstall();
  EXPECT_EQ(TerminaInstaller::InstallResult::Cancelled, result_future.Get());

  task_env_.FastForwardBy(base::Days(1));

  CheckDlcInstallCalledTimes(1);
}

TEST_F(TerminaInstallTest, InstallDlcOffline) {
  fake_dlc_client_.set_install_error("An error");

  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Offline, result_future.Get());
}

TEST_F(TerminaInstallTest, InstallDlcWithComponentInstalled) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Success, result_future.Get());

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();

  task_env_.RunUntilIdle();
  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaInstallTest, InstallDlcWithComponentInstalledUninstallError) {
  component_manager_->SetRegisteredComponents(
      {imageloader::kTerminaComponentName});
  component_manager_->set_unload_component_result(false);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Success, result_future.Get());

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();
}

TEST_F(TerminaInstallTest, InstallDlcFallback) {
  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Success, result_future.Get());

  CheckDlcInstallCalledTimes(1);
  ExpectDlcInstalled();
}

TEST_F(TerminaInstallTest, InstallDlcFallbackError) {
  fake_dlc_client_.set_install_error("An error");
  PrepareComponentForLoad();

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Failure, result_future.Get());

  CheckDlcInstallCalledTimes(1);
  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaInstallTest, InstallDlcFallbackOffline) {
  fake_dlc_client_.set_install_error("An error");
  PrepareComponentForLoad();

  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Offline, result_future.Get());

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

TEST_F(TerminaInstallTest, InstallDlcFallbackOfflineComponentAlreadyInstalled) {
  fake_dlc_client_.set_install_error("An error");
  PrepareComponentForLoad();
  component_manager_->RegisterCompatiblePath(
      imageloader::kTerminaComponentName,
      component_updater::CompatibleComponentInfo(component_install_path_,
                                                 /* version= */ std::nullopt));

  auto* network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  network_connection_tracker->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  TestFuture<TerminaInstaller::InstallResult> result_future;
  termina_installer_.Install(result_future.GetCallback());
  EXPECT_EQ(TerminaInstaller::InstallResult::Offline, result_future.Get());

  EXPECT_FALSE(component_manager_->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName));
}

}  // namespace crostini
