// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_dlc_installer.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
enum class Action { EnableDlc, DisableDlc };

struct EnableDisableTestData {
  const char* dlc_error;
  ArcDlcInstaller::InstallerState state_to_be_set;
  size_t callback_count;
  Action action;
  size_t dlc_count;
  ArcDlcInstaller::InstallerState expected_state;
  bool is_dlc_enabled;
};

constexpr EnableDisableTestData enable_disable_test_data[] = {
    // Tests that DLC is installed when install error is kErrorNone.
    {dlcservice::kErrorNone, ArcDlcInstaller::InstallerState::kUninstalled, 1,
     Action::EnableDlc, 1, ArcDlcInstaller::InstallerState::kInstalled, true},
    // Tests that DLC is not installed when install error is kErrorInvalidDlc.
    {dlcservice::kErrorInvalidDlc,
     ArcDlcInstaller::InstallerState::kUninstalled, 1, Action::EnableDlc, 1,
     ArcDlcInstaller::InstallerState::kUninstalled, true},
    // Tests that DLC is not installed when install error is kErrorNeedReboot.
    {dlcservice::kErrorNeedReboot,
     ArcDlcInstaller::InstallerState::kUninstalled, 1, Action::EnableDlc, 1,
     ArcDlcInstaller::InstallerState::kUninstalled, true},
    // Tests that DLC is not installed when install error is kErrorAllocation.
    {dlcservice::kErrorAllocation,
     ArcDlcInstaller::InstallerState::kUninstalled, 1, Action::EnableDlc, 1,
     ArcDlcInstaller::InstallerState::kUninstalled, true},
    // Tests that DLC is not installed when install error is kErrorNoImageFound.
    {dlcservice::kErrorNoImageFound,
     ArcDlcInstaller::InstallerState::kUninstalled, 1, Action::EnableDlc, 1,
     ArcDlcInstaller::InstallerState::kUninstalled, true},
    // Tests that DLC is not installed when install error is kErrorInternal.
    {dlcservice::kErrorInternal, ArcDlcInstaller::InstallerState::kUninstalled,
     1, Action::EnableDlc, 1, ArcDlcInstaller::InstallerState::kUninstalled,
     true},
    // Tests that DLC is not installed when install error is kErrorBusy.
    {dlcservice::kErrorBusy, ArcDlcInstaller::InstallerState::kUninstalled, 1,
     Action::EnableDlc, 1, ArcDlcInstaller::InstallerState::kUninstalled, true},
    // Tests that DLC is not installed when current state is kUninstalling.
    {dlcservice::kErrorNone, ArcDlcInstaller::InstallerState::kUninstalling, 0,
     Action::EnableDlc, 0, ArcDlcInstaller::InstallerState::kUninstalling,
     true},
    // Tests that DLC is not installed when current state is kInstalling.
    {dlcservice::kErrorNone, ArcDlcInstaller::InstallerState::kInstalling, 0,
     Action::EnableDlc, 0, ArcDlcInstaller::InstallerState::kInstalling, true},

    // Tests that DLC is uninstalled when install error is kErrorNone.
    {dlcservice::kErrorNone, ArcDlcInstaller::InstallerState::kInstalled, 1,
     Action::DisableDlc, 0, ArcDlcInstaller::InstallerState::kUninstalled,
     false},
    // Tests that DLC is not uninstalled when install error is kErrorInternal.
    {dlcservice::kErrorInternal, ArcDlcInstaller::InstallerState::kInstalled, 1,
     Action::DisableDlc, 0, ArcDlcInstaller::InstallerState::kInstalled, false},
    // Tests that DLC is not uninstalled when current state is kUninstalling.
    {dlcservice::kErrorNone, ArcDlcInstaller::InstallerState::kUninstalling, 0,
     Action::DisableDlc, 0, ArcDlcInstaller::InstallerState::kUninstalling,
     false},
    // Tests that DLC is not uninstalled when current state is kInstalling.
    {dlcservice::kErrorNone, ArcDlcInstaller::InstallerState::kInstalling, 0,
     Action::DisableDlc, 0, ArcDlcInstaller::InstallerState::kInstalling,
     false},
};

struct CallbackTestData {
  ArcDlcInstaller::InstallerState state_to_be_set;
  int expect_num_callbacks;
};

constexpr CallbackTestData callback_test_data[] = {
    // Tests that callback is invoked immediately if current state is
    // kUninstalled.
    {ArcDlcInstaller::InstallerState::kUninstalled, 1},
    // Tests that callback is invoked immediately if current state is
    // kInstalled.
    {ArcDlcInstaller::InstallerState::kInstalled, 1},
    // Tests that callback is stored and not invoked if current state is
    // kUninstalling.
    {ArcDlcInstaller::InstallerState::kUninstalling, 0},
    // Tests that callback is stored and not invoked if current state is
    // kInstalling.
    {ArcDlcInstaller::InstallerState::kInstalling, 0},
};
}  // namespace

class ArcDlcInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_dlc_client_.set_install_root_path("default_dlc_root_path_");
    arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>();
  }

  void TearDown() override { arc_dlc_installer_.reset(); }

  // Get installed DLC names from |fake_dlc_client_|.
  std::vector<std::string> GetInstalledDlcNames() {
    base::RunLoop run_loop;

    std::vector<std::string> dlc_list;
    fake_dlc_client_.GetExistingDlcs(base::BindOnce(
        [](base::OnceClosure quit, std::vector<std::string>* dlc_list,
           std::string_view err,
           const dlcservice::DlcsWithContent& dlcs_with_content) {
          for (const auto& dlc : dlcs_with_content.dlc_infos()) {
            dlc_list->push_back(dlc.id());
          }
          std::move(quit).Run();
        },
        run_loop.QuitClosure(), &dlc_list));

    run_loop.Run();
    return dlc_list;
  }

  ash::FakeDlcserviceClient fake_dlc_client_;
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;
};

// ArcDlcInstallerEnableDisableTest tests that ArcDlcInstaller's
// RequestEnable() and RequestDisable() transit to the correct
// states under different scenarios.
class ArcDlcInstallerEnableDisableTest
    : public ArcDlcInstallerTest,
      public ::testing::WithParamInterface<EnableDisableTestData> {};

TEST_P(ArcDlcInstallerEnableDisableTest, EnableDisableTest) {
  if (GetParam().action == Action::EnableDlc) {
    fake_dlc_client_.set_install_error(GetParam().dlc_error);
  } else {
    fake_dlc_client_.set_uninstall_error(GetParam().dlc_error);
  }
  arc_dlc_installer_->SetStateForTesting(GetParam().state_to_be_set);
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run()).Times(GetParam().callback_count);
  arc_dlc_installer_->WaitForStableState(callback.Get());

  if (GetParam().action == Action::EnableDlc) {
    arc_dlc_installer_->RequestEnable();
  } else {
    arc_dlc_installer_->RequestDisable();
  }
  base::RunLoop().RunUntilIdle();

  std::vector<std::string> dlc_list = GetInstalledDlcNames();
  EXPECT_EQ(dlc_list.size(), GetParam().dlc_count);
  for (const auto& dlc : dlc_list) {
    EXPECT_EQ(dlc, kHoudiniRvcDlc);
  }

  EXPECT_EQ(arc_dlc_installer_->GetStateForTesting(),
            GetParam().expected_state);
  EXPECT_EQ(arc_dlc_installer_->GetIsDlcEnabledForTesting(),
            GetParam().is_dlc_enabled);
}

INSTANTIATE_TEST_SUITE_P(ArcDlcInstallerEnableDisableTest,
                         ArcDlcInstallerEnableDisableTest,
                         ::testing::ValuesIn(enable_disable_test_data));

// ArcDlcInstallerCallbackTest tests that the callback passed
// into WaitForStableState() is invoked at the appropriate
// timing.
class ArcDlcInstallerCallbackTest
    : public ArcDlcInstallerTest,
      public ::testing::WithParamInterface<CallbackTestData> {};

TEST_P(ArcDlcInstallerCallbackTest, CallbackTest) {
  arc_dlc_installer_->SetStateForTesting(GetParam().state_to_be_set);
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run()).Times(GetParam().expect_num_callbacks);
  arc_dlc_installer_->WaitForStableState(callback.Get());
}

INSTANTIATE_TEST_SUITE_P(ArcDlcInstallerCallbackTest,
                         ArcDlcInstallerCallbackTest,
                         ::testing::ValuesIn(callback_test_data));

// Tests that installation, followed by uninstallation, are both successful.
TEST_F(ArcDlcInstallerTest, TestInstallAndUninstallSuccess) {
  fake_dlc_client_.set_install_error(dlcservice::kErrorNone);
  fake_dlc_client_.set_uninstall_error(dlcservice::kErrorNone);

  arc_dlc_installer_->RequestEnable();
  base::RunLoop().RunUntilIdle();

  std::vector<std::string> dlc_list_before = GetInstalledDlcNames();
  EXPECT_EQ(dlc_list_before.size(), 1ul);
  EXPECT_EQ(dlc_list_before[0], kHoudiniRvcDlc);

  EXPECT_EQ(arc_dlc_installer_->GetStateForTesting(),
            ArcDlcInstaller::InstallerState::kInstalled);
  EXPECT_EQ(arc_dlc_installer_->GetIsDlcEnabledForTesting(), true);

  arc_dlc_installer_->RequestDisable();
  base::RunLoop().RunUntilIdle();

  std::vector<std::string> dlc_list_after = GetInstalledDlcNames();
  EXPECT_EQ(dlc_list_after.size(), 0ul);

  EXPECT_EQ(arc_dlc_installer_->GetStateForTesting(),
            ArcDlcInstaller::InstallerState::kUninstalled);
  EXPECT_EQ(arc_dlc_installer_->GetIsDlcEnabledForTesting(), false);
}

}  // namespace arc
