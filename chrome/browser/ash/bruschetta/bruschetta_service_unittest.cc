// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service.h"

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

namespace {
const char kTestVmName[] = "vm_name";
const char kTestVmConfig[] = "vm_config";
}  // namespace

class BruschettaServiceTest : public testing::Test,
                              public guest_os::FakeVmServicesHelper {
 public:
  BruschettaServiceTest() = default;
  BruschettaServiceTest(const BruschettaServiceTest&) = delete;
  BruschettaServiceTest& operator=(const BruschettaServiceTest&) = delete;
  ~BruschettaServiceTest() override = default;

 protected:
  void SetUp() override {
    SetupPrefs();

    service_ = std::make_unique<BruschettaService>(&profile_);
  }

  void TearDown() override {}

  void SetupPrefs() {
    base::Value::Dict pref;
    base::Value::Dict config;
    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::RUN_ALLOWED));

    base::Value::Dict vtpm;
    vtpm.Set(prefs::kPolicyVTPMEnabledKey, false);
    vtpm.Set(prefs::kPolicyVTPMUpdateActionKey,
             static_cast<int>(
                 prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED));

    config.Set(prefs::kPolicyVTPMKey, std::move(vtpm));
    config.Set(prefs::kPolicyNameKey, "Display Name");

    pref.Set(kTestVmConfig, std::move(config));
    profile_.GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                                 std::move(pref));
  }

  void EnableByPolicy() {
    auto updater = ScopedDictPrefUpdate(profile_.GetPrefs(),
                                        prefs::kBruschettaVMConfiguration);

    updater.Get()
        .FindDict(kTestVmConfig)
        ->Set(prefs::kPolicyEnabledKey,
              static_cast<int>(prefs::PolicyEnabledState::RUN_ALLOWED));
  }

  void DisableByPolicy() {
    auto updater = ScopedDictPrefUpdate(profile_.GetPrefs(),
                                        prefs::kBruschettaVMConfiguration);

    updater.Get()
        .FindDict(kTestVmConfig)
        ->Set(prefs::kPolicyEnabledKey,
              static_cast<int>(prefs::PolicyEnabledState::BLOCKED));
  }

  void SetVtpmConfig(bool enabled, prefs::PolicyUpdateAction action) {
    auto updater = ScopedDictPrefUpdate(profile_.GetPrefs(),
                                        prefs::kBruschettaVMConfiguration);

    auto* vtpm =
        updater.Get().FindDict(kTestVmConfig)->FindDict(prefs::kPolicyVTPMKey);
    vtpm->Set(prefs::kPolicyVTPMEnabledKey, enabled);
    vtpm->Set(prefs::kPolicyVTPMUpdateActionKey, static_cast<int>(action));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BruschettaService> service_;
};

TEST_F(BruschettaServiceTest, GetLauncherPolicyEnabled) {
  EnableByPolicy();
  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);
  ASSERT_NE(service_->GetLauncher(kTestVmName), nullptr);
}

TEST_F(BruschettaServiceTest, GetLauncherPolicyDisabled) {
  DisableByPolicy();
  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);
  ASSERT_EQ(service_->GetLauncher(kTestVmName), nullptr);
}

TEST_F(BruschettaServiceTest, GetLauncherPolicyUpdate) {
  EnableByPolicy();
  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);
  DisableByPolicy();
  ASSERT_EQ(service_->GetLauncher(kTestVmName), nullptr);
  EnableByPolicy();
  ASSERT_NE(service_->GetLauncher(kTestVmName), nullptr);
}

TEST_F(BruschettaServiceTest, DynamicPolicyDisableForcesShutdown) {
  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(kTestVmName);

  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);

  EnableByPolicy();
  service_->RegisterVmLaunch(kTestVmName, {});
  ASSERT_TRUE(service_->GetRunningVmsForTesting().contains(kTestVmName));

  DisableByPolicy();
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 1);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service_->GetRunningVmsForTesting().contains(kTestVmName));
}

TEST_F(BruschettaServiceTest, DynamicPolicyUpdateVtpmFromEnabled) {
  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(kTestVmName);

  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);

  EnableByPolicy();
  service_->RegisterVmLaunch(kTestVmName, {.vtpm_enabled = true});
  ASSERT_TRUE(service_->GetRunningVmsForTesting().contains(kTestVmName));

  SetVtpmConfig(false, prefs::PolicyUpdateAction::NONE);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 0);
  SetVtpmConfig(false,
                prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 1);
  SetVtpmConfig(false, prefs::PolicyUpdateAction::FORCE_SHUTDOWN_ALWAYS);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 2);
  SetVtpmConfig(true, prefs::PolicyUpdateAction::NONE);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 2);
  SetVtpmConfig(true,
                prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 2);
  SetVtpmConfig(true, prefs::PolicyUpdateAction::FORCE_SHUTDOWN_ALWAYS);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 2);
}

TEST_F(BruschettaServiceTest, DynamicPolicyUpdateVtpmFromDisabled) {
  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(kTestVmName);

  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);

  EnableByPolicy();
  service_->RegisterVmLaunch(kTestVmName, {.vtpm_enabled = false});
  ASSERT_TRUE(service_->GetRunningVmsForTesting().contains(kTestVmName));

  SetVtpmConfig(false, prefs::PolicyUpdateAction::NONE);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 0);
  SetVtpmConfig(false,
                prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 0);
  SetVtpmConfig(false, prefs::PolicyUpdateAction::FORCE_SHUTDOWN_ALWAYS);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 0);
  SetVtpmConfig(true, prefs::PolicyUpdateAction::NONE);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 0);
  SetVtpmConfig(true,
                prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 0);
  SetVtpmConfig(true, prefs::PolicyUpdateAction::FORCE_SHUTDOWN_ALWAYS);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 1);
}

TEST_F(BruschettaServiceTest, VirtualMachinesAllowed) {
  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(kTestVmName);

  service_->RegisterInPrefs(MakeBruschettaId(kTestVmName), kTestVmConfig);

  EnableByPolicy();
  service_->RegisterVmLaunch(kTestVmName, {});
  ASSERT_TRUE(service_->GetRunningVmsForTesting().contains(kTestVmName));

  profile_.ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
  profile_.ScopedCrosSettingsTestHelper()->GetStubbedProvider()->SetBoolean(
      ash::kVirtualMachinesAllowed, false);

  ASSERT_FALSE(virtual_machines::AreVirtualMachinesAllowedByPolicy());
  ASSERT_EQ(service_->GetLauncher(kTestVmName), nullptr);
  ASSERT_EQ(FakeConciergeClient()->stop_vm_call_count(), 1);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service_->GetRunningVmsForTesting().contains(kTestVmName));
}

TEST_F(BruschettaServiceTest, RemoveVmSuccess) {
  base::RunLoop run_loop;
  bool result = false;
  service_->RemoveVm(MakeBruschettaId(kTestVmName),
                     base::BindLambdaForTesting([&](bool success) {
                       result = success;
                       run_loop.Quit();
                     }));
  run_loop.Run();
  ASSERT_TRUE(result);
}

TEST_F(BruschettaServiceTest, RemoveVmFailedToRemoveDisk) {
  vm_tools::concierge::DestroyDiskImageResponse response;
  response.set_status(
      ::vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  FakeConciergeClient()->set_destroy_disk_image_response(response);
  base::RunLoop run_loop;
  bool result = false;
  service_->RemoveVm(MakeBruschettaId(kTestVmName),
                     base::BindLambdaForTesting([&](bool success) {
                       result = success;
                       run_loop.Quit();
                     }));
  run_loop.Run();
  ASSERT_FALSE(result);
}

TEST_F(BruschettaServiceTest, RemoveVmFailedToRemoveDlc) {
  FakeDlcserviceClient()->set_uninstall_error("Error");
  base::RunLoop run_loop;
  bool result = false;
  service_->RemoveVm(MakeBruschettaId(kTestVmName),
                     base::BindLambdaForTesting([&](bool success) {
                       result = success;
                       run_loop.Quit();
                     }));
  run_loop.Run();
  ASSERT_FALSE(result);
}

}  // namespace bruschetta
