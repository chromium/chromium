// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/network_roaming_state_migration_handler_impl.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/tpm/install_attributes.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace policy {
namespace {

const char kCellularDevicePath[] = "/device/stub_cellular_device";
const char kCellularServiceGuid1[] = "cellular_1";
const char kCellularServiceGuid2[] = "cellular_2";
const char kCellularServicePattern[] = R"({
  "GUID": "%s",
  "Type": "cellular",
  "%s": %s,
  "Cellular.ICCID": "0123456789"
})";

class FakeNetworkRoamingStateMigrationHandlerObserver
    : public NetworkRoamingStateMigrationHandler::Observer {
 public:
  FakeNetworkRoamingStateMigrationHandlerObserver(
      NetworkRoamingStateMigrationHandler*
          network_roaming_state_migration_handler) {
    scoped_observer_.Observe(network_roaming_state_migration_handler);
  }

  ~FakeNetworkRoamingStateMigrationHandlerObserver() override = default;

  void OnFoundCellularNetwork(bool roaming_enabled) override {
    did_last_network_have_roaming_enabled_ = roaming_enabled;
    found_cellular_network_count_++;
  }

  int found_cellular_network_count() const {
    return found_cellular_network_count_;
  }

  bool did_last_network_have_roaming_enabled() const {
    return did_last_network_have_roaming_enabled_;
  }

 private:
  int found_cellular_network_count_ = 0;
  bool did_last_network_have_roaming_enabled_ = false;
  base::ScopedObservation<NetworkRoamingStateMigrationHandler,
                          NetworkRoamingStateMigrationHandler::Observer>
      scoped_observer_{this};
};

class NetworkRoamingStateMigrationHandlerTest : public testing::Test {
 protected:
  NetworkRoamingStateMigrationHandlerTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kCellularAllowPerNetworkRoaming);
    network_handler_test_helper_ =
        std::make_unique<chromeos::NetworkHandlerTestHelper>();
    network_handler_test_helper()->AddDefaultProfiles();
    network_handler_test_helper()->ResetDevicesAndServices();
    network_handler_test_helper()->manager_test()->AddTechnology(
        shill::kTypeCellular,
        /*enabled=*/true);
    network_handler_test_helper()->device_test()->AddDevice(
        kCellularDevicePath, shill::kTypeCellular, "stub_cellular_device");
    base::RunLoop().RunUntilIdle();
  }

  ~NetworkRoamingStateMigrationHandlerTest() override {
    network_handler_test_helper_.reset();
  }

  std::string ConfigureCellularNetwork(const std::string& guid,
                                       bool allow_roaming) {
    const std::string service_path =
        network_handler_test_helper()->ConfigureService(
            base::StringPrintf(kCellularServicePattern, guid.c_str(),
                               shill::kCellularAllowRoamingProperty,
                               allow_roaming ? "true" : "false"));
    return service_path;
  }

  bool GetCellularAllowRoamingProperty(const std::string& service_path) {
    const chromeos::ShillServiceClient::TestInterface* test_interface =
        network_handler_test_helper()->service_test();
    const base::Value* properties =
        test_interface->GetServiceProperties(service_path);
    const base::Value* maybe_allow_roaming =
        properties->FindKey(shill::kCellularAllowRoamingProperty);
    return maybe_allow_roaming && maybe_allow_roaming->GetBool();
  }

  ash::ScopedCrosSettingsTestHelper* scoped_cros_settings_test_helper() {
    return &scoped_cros_settings_test_helper_;
  }

  chromeos::NetworkHandlerTestHelper* network_handler_test_helper() {
    return network_handler_test_helper_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ash::ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<chromeos::NetworkHandlerTestHelper>
      network_handler_test_helper_;
};

TEST_F(NetworkRoamingStateMigrationHandlerTest,
       NetworksHaveAllowRoamingPropertyCleared) {
  const std::string cellular_service_path_1 =
      ConfigureCellularNetwork(kCellularServiceGuid1, /*allow_roaming=*/true);

  EXPECT_TRUE(GetCellularAllowRoamingProperty(cellular_service_path_1));

  policy::NetworkRoamingStateMigrationHandlerImpl
      network_roaming_state_migration_handler;
  FakeNetworkRoamingStateMigrationHandlerObserver fake_observer(
      &network_roaming_state_migration_handler);
  const std::string cellular_service_path_2 =
      ConfigureCellularNetwork(kCellularServiceGuid2, /*allow_roaming=*/true);

  EXPECT_FALSE(GetCellularAllowRoamingProperty(cellular_service_path_1));
  EXPECT_FALSE(GetCellularAllowRoamingProperty(cellular_service_path_2));
  EXPECT_TRUE(fake_observer.did_last_network_have_roaming_enabled());
  EXPECT_EQ(2, fake_observer.found_cellular_network_count());
}

TEST_F(NetworkRoamingStateMigrationHandlerTest,
       NetworksHaveAllowRoamingPropertyClearedSingleTime) {
  policy::NetworkRoamingStateMigrationHandlerImpl
      network_roaming_state_migration_handler;
  FakeNetworkRoamingStateMigrationHandlerObserver fake_observer(
      &network_roaming_state_migration_handler);
  const std::string cellular_service_path =
      ConfigureCellularNetwork(kCellularServiceGuid1, /*allow_roaming=*/true);

  EXPECT_FALSE(GetCellularAllowRoamingProperty(cellular_service_path));
  EXPECT_TRUE(fake_observer.did_last_network_have_roaming_enabled());
  EXPECT_EQ(1, fake_observer.found_cellular_network_count());

  // Explicitly reset the |shill::kCellularAllowRoamingProperty| on the
  // cellular network that has already been handled.
  chromeos::ShillServiceClient::TestInterface* test_interface =
      network_handler_test_helper()->service_test();
  EXPECT_TRUE(test_interface->SetServiceProperty(
      cellular_service_path, shill::kCellularAllowRoamingProperty,
      base::Value(true)));

  ConfigureCellularNetwork(kCellularServiceGuid2, /*allow_roaming=*/true);

  EXPECT_TRUE(GetCellularAllowRoamingProperty(cellular_service_path));
  EXPECT_TRUE(fake_observer.did_last_network_have_roaming_enabled());
  EXPECT_EQ(2, fake_observer.found_cellular_network_count());
}

TEST_F(NetworkRoamingStateMigrationHandlerTest,
       NetworksHaveAllowRoamingPropertyClearedOnManagedDevice) {
  scoped_cros_settings_test_helper()->InstallAttributes()->SetCloudManaged(
      policy::PolicyBuilder::kFakeDomain, policy::PolicyBuilder::kFakeDeviceId);
  const std::string cellular_service_path_1 =
      ConfigureCellularNetwork(kCellularServiceGuid1, /*allow_roaming=*/true);

  EXPECT_TRUE(GetCellularAllowRoamingProperty(cellular_service_path_1));

  policy::NetworkRoamingStateMigrationHandlerImpl
      network_roaming_state_migration_handler;
  FakeNetworkRoamingStateMigrationHandlerObserver fake_observer(
      &network_roaming_state_migration_handler);
  const std::string cellular_service_path_2 =
      ConfigureCellularNetwork(kCellularServiceGuid2, /*allow_roaming=*/false);

  EXPECT_FALSE(GetCellularAllowRoamingProperty(cellular_service_path_1));
  EXPECT_FALSE(GetCellularAllowRoamingProperty(cellular_service_path_2));
  EXPECT_FALSE(fake_observer.did_last_network_have_roaming_enabled());
  EXPECT_EQ(2, fake_observer.found_cellular_network_count());
}

}  // namespace
}  // namespace policy
