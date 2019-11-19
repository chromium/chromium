// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/device_disabling_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace chromeos {
namespace system {

namespace {

const char kTestUser[] = "user@example.com";
const char kEnrollmentDomain[] = "example.com";
const char kDisabledMessage1[] = "Device disabled 1.";
const char kDisabledMessage2[] = "Device disabled 2.";

}

class DeviceDisablingManagerTestBase : public testing::Test,
                                       public DeviceDisablingManager::Delegate {
 public:
  DeviceDisablingManagerTestBase();

  // testing::Test:
  void TearDown() override;

  virtual void CreateDeviceDisablingManager();
  virtual void DestroyDeviceDisablingManager();
  void LogIn();

  // DeviceDisablingManager::Delegate:
  MOCK_METHOD0(RestartToLoginScreen, void());
  MOCK_METHOD0(ShowDeviceDisabledScreen, void());

  DeviceDisablingManager* GetDeviceDisablingManager() {
    return device_disabling_manager_.get();
  }

  // Configure install attributes.
  void SetUnowned();
  void SetEnterpriseOwned();
  void SetConsumerOwned();

 private:
  content::BrowserTaskEnvironment task_environment_;
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::FakeChromeUserManager fake_user_manager_;
  std::unique_ptr<DeviceDisablingManager> device_disabling_manager_;
  FakeStatisticsProvider statistics_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManagerTestBase);
};

DeviceDisablingManagerTestBase::DeviceDisablingManagerTestBase() {
  system::StatisticsProvider::SetTestProvider(&statistics_provider_);
}

void DeviceDisablingManagerTestBase::TearDown() {
  DestroyDeviceDisablingManager();
}

void DeviceDisablingManagerTestBase::CreateDeviceDisablingManager() {
  device_disabling_manager_.reset(new DeviceDisablingManager(
      this,
      CrosSettings::Get(),
      &fake_user_manager_));
  device_disabling_manager_->Init();
}

void DeviceDisablingManagerTestBase::DestroyDeviceDisablingManager() {
  device_disabling_manager_.reset();
}

void DeviceDisablingManagerTestBase::LogIn() {
  fake_user_manager_.AddUser(AccountId::FromUserEmail(kTestUser));
}

void DeviceDisablingManagerTestBase::SetUnowned() {
  cros_settings_test_helper_.InstallAttributes()->Clear();
}

void DeviceDisablingManagerTestBase::SetEnterpriseOwned() {
  cros_settings_test_helper_.InstallAttributes()->SetCloudManaged(
      kEnrollmentDomain, "fake-id");
}

void DeviceDisablingManagerTestBase::SetConsumerOwned() {
  cros_settings_test_helper_.InstallAttributes()->SetConsumerOwned();
}

// Base class for tests that verify device disabling behavior during OOBE, when
// the device is not owned yet.
class DeviceDisablingManagerOOBETest : public DeviceDisablingManagerTestBase {
 public:
  DeviceDisablingManagerOOBETest();

  // DeviceDisablingManagerTestBase:
  void SetUp() override;
  void TearDown() override;

  bool device_disabled() const { return device_disabled_; }

  void CheckWhetherDeviceDisabledDuringOOBE();

  void SetDeviceDisabled(bool disabled);

 private:
  void OnDeviceDisabledChecked(bool device_disabled);

  TestingPrefServiceSimple local_state_;
  FakeStatisticsProvider statistics_provider_;

  base::RunLoop run_loop_;
  bool device_disabled_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManagerOOBETest);
};

DeviceDisablingManagerOOBETest::DeviceDisablingManagerOOBETest()
    : device_disabled_(false) {
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
}

void DeviceDisablingManagerOOBETest::SetUp() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(
      local_state_.registry());
  CreateDeviceDisablingManager();
  system::StatisticsProvider::SetTestProvider(&statistics_provider_);
}

void DeviceDisablingManagerOOBETest::TearDown() {
  DeviceDisablingManagerTestBase::TearDown();
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}

void DeviceDisablingManagerOOBETest::CheckWhetherDeviceDisabledDuringOOBE() {
  GetDeviceDisablingManager()->CheckWhetherDeviceDisabledDuringOOBE(
      base::Bind(&DeviceDisablingManagerOOBETest::OnDeviceDisabledChecked,
                 base::Unretained(this)));
  run_loop_.Run();
}

void DeviceDisablingManagerOOBETest::SetDeviceDisabled(bool disabled) {
  DictionaryPrefUpdate dict(&local_state_, prefs::kServerBackedDeviceState);
  if (disabled) {
    dict->SetString(policy::kDeviceStateMode,
                    policy::kDeviceStateRestoreModeDisabled);
  } else {
    dict->Remove(policy::kDeviceStateMode, nullptr);
  }
  dict->SetString(policy::kDeviceStateManagementDomain, kEnrollmentDomain);
  dict->SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage1);
}

void DeviceDisablingManagerOOBETest::OnDeviceDisabledChecked(
    bool device_disabled) {
  device_disabled_ = device_disabled;
  run_loop_.Quit();
}

// Verifies that the device is not considered disabled during OOBE by default.
TEST_F(DeviceDisablingManagerOOBETest, NotDisabledByDefault) {
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when it is
// explicitly marked as not disabled.
TEST_F(DeviceDisablingManagerOOBETest, NotDisabledWhenExplicitlyNotDisabled) {
  SetDeviceDisabled(false);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when device
// disabling is turned off by flag, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerOOBETest, NotDisabledWhenTurnedOffByFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDeviceDisabling);
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when it is
// already enrolled, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerOOBETest, NotDisabledWhenEnterpriseOwned) {
  SetEnterpriseOwned();
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when it is
// already owned by a consumer, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerOOBETest, NotDisabledWhenConsumerOwned) {
  SetConsumerOwned();
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is considered disabled during OOBE when it is marked
// as disabled, device disabling is not turned off by flag and the device is not
// owned yet.
TEST_F(DeviceDisablingManagerOOBETest, ShowWhenDisabledAndNotOwned) {
  SetUnowned();
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_TRUE(device_disabled());
  EXPECT_EQ(kEnrollmentDomain,
            GetDeviceDisablingManager()->enrollment_domain());
  EXPECT_EQ(kDisabledMessage1, GetDeviceDisablingManager()->disabled_message());
}

// Base class for tests that verify device disabling behavior once the device is
// owned.
class DeviceDisablingManagerTest : public DeviceDisablingManagerTestBase,
                                   public DeviceDisablingManager::Observer {
 public:
  DeviceDisablingManagerTest();

  // DeviceDisablingManagerTestBase:
  void TearDown() override;
  void CreateDeviceDisablingManager() override;
  void DestroyDeviceDisablingManager() override;

  // DeviceDisablingManager::Observer:
  MOCK_METHOD1(OnDisabledMessageChanged, void(const std::string&));

  void MakeCrosSettingsTrusted();

  void SetDeviceDisabled(bool disabled);
  void SetDisabledMessage(const std::string& disabled_message);

 private:
  void SimulatePolicyFetch();

  FakeSessionManagerClient session_manager_client_;
  policy::DevicePolicyBuilder device_policy_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManagerTest);
};

DeviceDisablingManagerTest::DeviceDisablingManagerTest() {
}

void DeviceDisablingManagerTest::TearDown() {
  chromeos::DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceDisablingManagerTestBase::TearDown();
}

void DeviceDisablingManagerTest::CreateDeviceDisablingManager() {
  DeviceDisablingManagerTestBase::CreateDeviceDisablingManager();
  GetDeviceDisablingManager()->AddObserver(this);
}

void DeviceDisablingManagerTest::DestroyDeviceDisablingManager() {
  if (GetDeviceDisablingManager())
    GetDeviceDisablingManager()->RemoveObserver(this);
  DeviceDisablingManagerTestBase::DestroyDeviceDisablingManager();
}

void DeviceDisablingManagerTest::MakeCrosSettingsTrusted() {
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util(
      new ownership::MockOwnerKeyUtil);
  owner_key_util->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  chromeos::DeviceSettingsService::Get()->SetSessionManager(
      &session_manager_client_, owner_key_util);
  SimulatePolicyFetch();
}

void DeviceDisablingManagerTest::SetDeviceDisabled(bool disabled) {
  if (disabled) {
    device_policy_.policy_data().mutable_device_state()->set_device_mode(
        enterprise_management::DeviceState::DEVICE_MODE_DISABLED);
  } else {
    device_policy_.policy_data().mutable_device_state()->clear_device_mode();
  }
  SimulatePolicyFetch();
}

void DeviceDisablingManagerTest::SetDisabledMessage(
    const std::string& disabled_message) {
  device_policy_.policy_data().mutable_device_state()->
      mutable_disabled_state()->set_message(disabled_message);
  SimulatePolicyFetch();
}

void DeviceDisablingManagerTest::SimulatePolicyFetch() {
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  chromeos::DeviceSettingsService::Get()->OwnerKeySet(true);
  content::RunAllTasksUntilIdle();
}

// Verifies that the device is not considered disabled by default when it is
// enrolled for enterprise management.
TEST_F(DeviceDisablingManagerTest, NotDisabledByDefault) {
  SetEnterpriseOwned();
  MakeCrosSettingsTrusted();

  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
}

// Verifies that the device is not considered disabled when it is explicitly
// marked as not disabled.
TEST_F(DeviceDisablingManagerTest, NotDisabledWhenExplicitlyNotDisabled) {
  SetEnterpriseOwned();
  MakeCrosSettingsTrusted();
  SetDeviceDisabled(false);

  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
}

// Verifies that the device is not considered disabled when device disabling is
// turned off by flag, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerTest, NotDisabledWhenTurnedOffByFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDeviceDisabling);
  SetEnterpriseOwned();
  MakeCrosSettingsTrusted();
  SetDeviceDisabled(true);

  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
}

// Verifies that the device is not considered disabled when it is owned by a
// consumer, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerTest, NotDisabledWhenConsumerOwned) {
  SetConsumerOwned();
  MakeCrosSettingsTrusted();
  SetDeviceDisabled(true);

  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
}

// Verifies that the device disabled screen is shown immediately when the device
// is already marked as disabled on start-up.
TEST_F(DeviceDisablingManagerTest, DisabledOnLoginScreen) {
  SetEnterpriseOwned();
  MakeCrosSettingsTrusted();
  SetDisabledMessage(kDisabledMessage1);
  SetDeviceDisabled(true);

  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(1);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
  EXPECT_EQ(kEnrollmentDomain,
            GetDeviceDisablingManager()->enrollment_domain());
  EXPECT_EQ(kDisabledMessage1, GetDeviceDisablingManager()->disabled_message());
}

// Verifies that the device disabled screen is shown immediately when the device
// becomes disabled while the login screen is showing. Also verifies that Chrome
// restarts when the device becomes enabled again.
TEST_F(DeviceDisablingManagerTest, DisableAndReEnableOnLoginScreen) {
  SetEnterpriseOwned();
  MakeCrosSettingsTrusted();
  SetDisabledMessage(kDisabledMessage1);

  // Verify that initially, the disabled screen is not shown and Chrome does not
  // restart.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
  Mock::VerifyAndClearExpectations(this);

  // Mark the device as disabled. Verify that the device disabled screen is
  // shown.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(1);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(kDisabledMessage1)).Times(1);
  SetDeviceDisabled(true);
  Mock::VerifyAndClearExpectations(this);
  EXPECT_EQ(kEnrollmentDomain,
            GetDeviceDisablingManager()->enrollment_domain());
  EXPECT_EQ(kDisabledMessage1, GetDeviceDisablingManager()->disabled_message());

  // Update the disabled message. Verify that the device disabled screen is
  // updated.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(kDisabledMessage2)).Times(1);
  SetDisabledMessage(kDisabledMessage2);
  Mock::VerifyAndClearExpectations(this);
  EXPECT_EQ(kDisabledMessage2, GetDeviceDisablingManager()->disabled_message());

  // Mark the device as enabled again. Verify that Chrome restarts.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(1);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  SetDeviceDisabled(false);
}

// Verifies that Chrome restarts when the device becomes disabled while a
// session is in progress.
TEST_F(DeviceDisablingManagerTest, DisableDuringSession) {
  SetEnterpriseOwned();
  MakeCrosSettingsTrusted();
  SetDisabledMessage(kDisabledMessage1);
  LogIn();

  // Verify that initially, the disabled screen is not shown and Chrome does not
  // restart.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(0);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  CreateDeviceDisablingManager();
  Mock::VerifyAndClearExpectations(this);

  // Mark the device as disabled. Verify that Chrome restarts.
  EXPECT_CALL(*this, RestartToLoginScreen()).Times(1);
  EXPECT_CALL(*this, ShowDeviceDisabledScreen()).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(_)).Times(0);
  EXPECT_CALL(*this, OnDisabledMessageChanged(kDisabledMessage1)).Times(1);
  SetDeviceDisabled(true);
}

// Verifies that the HonorDeviceDisablingDuringNormalOperation() method returns
// true iff the device is enterprise enrolled and device disabling is not turned
// off by flag.
TEST_F(DeviceDisablingManagerTest, HonorDeviceDisablingDuringNormalOperation) {
  // Not enterprise owned, not disabled by flag.
  EXPECT_FALSE(
      DeviceDisablingManager::HonorDeviceDisablingDuringNormalOperation());

  // Enterprise owned, not disabled by flag.
  SetEnterpriseOwned();
  EXPECT_TRUE(
      DeviceDisablingManager::HonorDeviceDisablingDuringNormalOperation());

  // Enterprise owned, disabled by flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDeviceDisabling);
  EXPECT_FALSE(
      DeviceDisablingManager::HonorDeviceDisablingDuringNormalOperation());

  // Not enterprise owned, disabled by flag.
  SetUnowned();
  EXPECT_FALSE(
      DeviceDisablingManager::HonorDeviceDisablingDuringNormalOperation());
}

}  // namespace system
}  // namespace chromeos
