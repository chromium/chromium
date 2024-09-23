// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_settings_ash.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class DeviceSettingsAshTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::ash::DeviceSettingsService::Initialize();
    scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util(
        new ownership::MockOwnerKeyUtil());
    owner_key_util->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
    ::ash::DeviceSettingsService::Get()->SetSessionManager(
        &session_manager_client_, owner_key_util);

    install_attributes_ = std::make_unique<ash::StubInstallAttributes>();
    ash::InstallAttributes::SetForTesting(install_attributes_.get());
  }

  void TearDown() override {
    ash::InstallAttributes::ShutdownForTesting();
    if (::ash::DeviceSettingsService::IsInitialized()) {
      ::ash::DeviceSettingsService::Get()->UnsetSessionManager();
      ::ash::DeviceSettingsService::Shutdown();
    }
  }

  void SetDeviceAssociationState(
      ::enterprise_management::PolicyData::AssociationState association_state) {
    device_policy_.policy_data().set_state(association_state);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());

    auto* const device_settings_service = ::ash::DeviceSettingsService::Get();
    base::test::TestFuture<void> waiter;
    device_settings_service->Store(device_policy_.GetCopy(),
                                   waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
    ASSERT_EQ(device_settings_service->status(),
              ::ash::DeviceSettingsService::STORE_SUCCESS);
  }

  base::test::TaskEnvironment task_environment_;
  ::ash::FakeSessionManagerClient session_manager_client_;
  ::policy::DevicePolicyBuilder device_policy_;
  std::unique_ptr<ash::StubInstallAttributes> install_attributes_;
};

TEST_F(DeviceSettingsAshTest, ShouldReturnFalseWhenServiceUninitialized) {
  // Uninitialize device settings service since it is initialized during setup.
  ::ash::DeviceSettingsService::Shutdown();

  DeviceSettingsAsh device_settings_service;
  base::test::TestFuture<bool> test_future;
  device_settings_service.IsDeviceDeprovisioned(test_future.GetCallback());
  EXPECT_FALSE(test_future.Get());
}

TEST_F(DeviceSettingsAshTest, ShouldReturnTrueWhenDeviceDeprovisioned) {
  DeviceSettingsAsh device_settings_service;
  base::test::TestFuture<bool> test_future;
  SetDeviceAssociationState(::enterprise_management::PolicyData::DEPROVISIONED);
  device_settings_service.IsDeviceDeprovisioned(test_future.GetCallback());
  EXPECT_TRUE(test_future.Get());
}

TEST_F(DeviceSettingsAshTest, ShouldReturnFalseIfDeviceNotDeprovisioned) {
  DeviceSettingsAsh device_settings_service;
  base::test::TestFuture<bool> test_future;
  SetDeviceAssociationState(::enterprise_management::PolicyData::ACTIVE);
  device_settings_service.IsDeviceDeprovisioned(test_future.GetCallback());
  EXPECT_FALSE(test_future.Get());
}

}  // namespace
}  // namespace crosapi
