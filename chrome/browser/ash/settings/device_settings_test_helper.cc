// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_test_helper.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

namespace ash {

ScopedDeviceSettingsTestHelper::ScopedDeviceSettingsTestHelper() {
  DeviceSettingsService::Initialize();
  DeviceSettingsService::Get()->SetSessionManager(
      &session_manager_client_, new ownership::MockOwnerKeyUtil());
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();
}

ScopedDeviceSettingsTestHelper::~ScopedDeviceSettingsTestHelper() {
  content::RunAllTasksUntilIdle();
  DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceSettingsService::Shutdown();
}

DeviceSettingsTestBase::DeviceSettingsTestBase(bool profile_creation_enabled)
    : profile_creation_enabled_(profile_creation_enabled),
      task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

DeviceSettingsTestBase::DeviceSettingsTestBase(
    base::test::TaskEnvironment::TimeSource time)
    : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP, time) {}

DeviceSettingsTestBase::~DeviceSettingsTestBase() {
  CHECK(teardown_called_);
}

void DeviceSettingsTestBase::SetUp() {
  // Initialize ProfileHelper including BrowserContextHelper.
  ProfileHelper::Get();

  device_policy_ = std::make_unique<policy::DevicePolicyBuilder>();
  owner_key_util_ = new ownership::MockOwnerKeyUtil();
  device_settings_service_ = std::make_unique<DeviceSettingsService>();
  ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
  UserDataAuthClient::InitializeFake();
  CryptohomeMiscClient::InitializeFake();
  chromeos::PowerManagerClient::InitializeFake();
  chromeos::TpmManagerClient::InitializeFake();
  OwnerSettingsServiceAshFactory::SetDeviceSettingsServiceForTesting(
      device_settings_service_.get());
  OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
      owner_key_util_);
  base::RunLoop().RunUntilIdle();

  device_policy_->payload().mutable_metrics_enabled()->set_metrics_enabled(
      false);
  ReloadDevicePolicy();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  device_settings_service_->SetSessionManager(&session_manager_client_,
                                              owner_key_util_);

  if (profile_creation_enabled_) {
    profile_ = std::make_unique<TestingProfile>();
    FakeNssService::InitializeForBrowserContext(profile_.get(),
                                                /*enable_system_slot=*/false);
  }
}

void DeviceSettingsTestBase::TearDown() {
  teardown_called_ = true;
  OwnerSettingsServiceAshFactory::SetDeviceSettingsServiceForTesting(nullptr);
  FlushDeviceSettings();
  device_settings_service_->UnsetSessionManager();
  device_settings_service_.reset();
  chromeos::TpmManagerClient::Shutdown();
  chromeos::PowerManagerClient::Shutdown();
  CryptohomeMiscClient::Shutdown();
  UserDataAuthClient::Shutdown();
  device_policy_.reset();
  base::RunLoop().RunUntilIdle();
  profile_.reset();
  ConciergeClient::Shutdown();
}

void DeviceSettingsTestBase::ReloadDevicePolicy() {
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
}

void DeviceSettingsTestBase::FlushDeviceSettings() {
  content::RunAllTasksUntilIdle();
}

void DeviceSettingsTestBase::ReloadDeviceSettings() {
  device_settings_service_->OwnerKeySet(true);
  FlushDeviceSettings();
}

void DeviceSettingsTestBase::InitOwner(const AccountId& account_id,
                                       bool tpm_is_ready) {
  ash::AnnotatedAccountId::Set(profile_.get(), account_id);
  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
  CHECK(service);
  if (tpm_is_ready)
    service->OnTPMTokenReady();
}

void DeviceSettingsTestBase::SetSessionStopping() {
  session_manager_client_.NotifySessionStopping();
}

}  // namespace ash
