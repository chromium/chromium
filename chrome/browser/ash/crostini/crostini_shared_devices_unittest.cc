// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_shared_devices.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

using vm_tools::cicerone::UpdateContainerDevicesResponse;

constexpr char kMicrophone[] = "microphone";
constexpr char kInvalidDevice[] = "invalid-device";

class CrostiniSharedDevicesTest : public testing::Test {
 public:
  CrostiniSharedDevicesTest() : container_id_(DefaultContainerId()) {}

  CrostiniSharedDevicesTest(const CrostiniSharedDevicesTest&) = delete;
  CrostiniSharedDevicesTest& operator=(const CrostiniSharedDevicesTest&) =
      delete;

  ~CrostiniSharedDevicesTest() override = default;

  void SetUp() override {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    crostini_shared_devices_ =
        std::make_unique<CrostiniSharedDevices>(profile());

    update_container_devices_response_.set_status(
        UpdateContainerDevicesResponse::OK);
    AddContainerToPrefs(profile(), container_id_, {});
  }

  void TearDown() override {
    crostini_shared_devices_.reset();
    test_helper_.reset();
    profile_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

 protected:
  CrostiniManager* crostini_manager() {
    return CrostiniManager::GetForProfile(profile());
  }

  Profile* profile() { return profile_.get(); }

  void set_vm_device_result(
      std::string vm_device,
      const UpdateContainerDevicesResponse::UpdateResult result) {
    auto& results_map = *update_container_devices_response_.mutable_results();
    results_map[vm_device] = result;
    ash::FakeCiceroneClient::Get()->set_update_container_devices_response(
        update_container_devices_response_);
  }

  void SetVmDeviceShared(const std::string& vm_device,
                         bool shared,
                         bool expect_applied) {
    base::test::TestFuture<bool> result_future;
    crostini_shared_devices_->SetVmDeviceShared(
        container_id_, vm_device, shared, result_future.GetCallback());
    EXPECT_EQ(expect_applied, result_future.Get());
  }

  void AddRunningContainer(const guest_os::GuestId& container_id) {
    crostini_manager()->AddRunningVmForTesting(container_id.vm_name);
    // Trigger the running container logic via
    crostini_manager()->AddRunningContainerForTesting(
        container_id_.vm_name,
        ContainerInfo(container_id.container_name, kCrostiniDefaultUsername,
                      "home/testuser1", "CONTAINER_IP_ADDRESS"),
        true);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  guest_os::GuestId container_id_;

  vm_tools::cicerone::UpdateContainerDevicesResponse
      update_container_devices_response_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniSharedDevices> crostini_shared_devices_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CrostiniSharedDevicesTest, ShareMicSuccess) {
  AddRunningContainer(container_id_);
  EXPECT_FALSE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
  set_vm_device_result(kMicrophone, UpdateContainerDevicesResponse::SUCCESS);
  SetVmDeviceShared(kMicrophone, true, true);

  EXPECT_TRUE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
}

TEST_F(CrostiniSharedDevicesTest, ShareMicNoSuchDevice) {
  AddRunningContainer(container_id_);
  EXPECT_FALSE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
  set_vm_device_result(kMicrophone,
                       UpdateContainerDevicesResponse::NO_SUCH_VM_DEVICE);
  SetVmDeviceShared(kMicrophone, true, true);
  EXPECT_FALSE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
}

TEST_F(CrostiniSharedDevicesTest, ShareMicActionFailed) {
  AddRunningContainer(container_id_);
  EXPECT_FALSE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
  set_vm_device_result(kMicrophone,
                       UpdateContainerDevicesResponse::ACTION_FAILED);
  SetVmDeviceShared(kMicrophone, true, true);
  EXPECT_FALSE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
}

TEST_F(CrostiniSharedDevicesTest, SharedDevicesBeforeContainerRunning) {
  set_vm_device_result(kMicrophone, UpdateContainerDevicesResponse::SUCCESS);
  set_vm_device_result(kInvalidDevice,
                       UpdateContainerDevicesResponse::NO_SUCH_VM_DEVICE);

  EXPECT_FALSE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
  EXPECT_FALSE(crostini_shared_devices_->IsVmDeviceShared(container_id_,
                                                          kInvalidDevice));

  SetVmDeviceShared(kMicrophone, true, false);
  SetVmDeviceShared(kInvalidDevice, true, false);
  EXPECT_TRUE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
  EXPECT_TRUE(crostini_shared_devices_->IsVmDeviceShared(container_id_,
                                                         kInvalidDevice));

  // When the container starts running, we want it to note that there is no
  // such device as kInvalidDevice.

  AddRunningContainer(container_id_);
  EXPECT_TRUE(
      crostini_shared_devices_->IsVmDeviceShared(container_id_, kMicrophone));
  EXPECT_FALSE(crostini_shared_devices_->IsVmDeviceShared(container_id_,
                                                          kInvalidDevice));
}

}  // namespace crostini
