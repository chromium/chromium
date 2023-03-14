// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/apn_migrator.h"

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/mock_managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/mock_network_metadata_store.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/cpp/fake_cros_network_config.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

using ::chromeos::network_config::FakeCrosNetworkConfig;
using ::chromeos::network_config::mojom::ApnPropertiesPtr;
using ::chromeos::network_config::mojom::ApnState;
using network_config::OverrideInProcessInstanceForTesting;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Truly;
using ::testing::WithArg;

constexpr char kCellularName1[] = "cellular_device_1";
constexpr char kTestCellularPath1[] = "/device/cellular_device_1";
constexpr char kTestCellularIccid1[] = "test_iccid_1";
constexpr char kTestCellularGuid1[] = "test_guid_1";

constexpr char kCellularName2[] = "cellular_device_2";
constexpr char kTestCellularPath2[] = "/device/cellular_device_2";
constexpr char kTestCellularIccid2[] = "test_iccid_2";
constexpr char kTestCellularGuid2[] = "test_guid_2";

constexpr char kCellularName3[] = "cellular_device_3";
constexpr char kTestCellularPath3[] = "/device/cellular_device_3";
constexpr char kTestCellularIccid3[] = "test_iccid_3";
constexpr char kTestCellularGuid3[] = "test_guid_3";

constexpr char kCellularServicePattern[] =
    R"({"GUID": "%s", "Type": "cellular",  "State": "idle",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated", "Cellular.ICCID": "%s",
            "Profile": "%s"%s})";
constexpr char kUiData[] =
    R"(, "UIData": "{\"onc_source\": \"device_policy\"}")";

}  // namespace

class ApnMigratorTest : public testing::Test {
 protected:
  ApnMigratorTest() = default;

  ApnMigratorTest(const ApnMigratorTest&) = delete;
  ApnMigratorTest& operator=(const ApnMigratorTest&) = delete;
  ~ApnMigratorTest() override = default;

  // testing::Test
  void SetUp() override {
    LoginState::Initialize();
    managed_cellular_pref_handler_ =
        base::WrapUnique(new testing::NiceMock<MockManagedCellularPrefHandler>);
    managed_network_configuration_handler_ = base::WrapUnique(
        new testing::NiceMock<MockManagedNetworkConfigurationHandler>);
    network_metadata_store_ =
        base::WrapUnique(new testing::NiceMock<MockNetworkMetadataStore>());
    cros_network_config_ = std::make_unique<FakeCrosNetworkConfig>();
    OverrideInProcessInstanceForTesting(cros_network_config_.get());

    apn_migrator_ = std::make_unique<ApnMigrator>(
        managed_cellular_pref_handler_.get(),
        managed_network_configuration_handler_.get(),
        network_state_helper_.network_state_handler(),
        network_metadata_store_.get());

    network_state_helper_.manager_test()->AddTechnology(shill::kTypeCellular,
                                                        /*enabled=*/true);
    network_state_helper_.network_state_handler()
        ->set_stub_cellular_networks_provider(
            &stub_cellular_networks_provider_);
  }

  void TearDown() override {
    apn_migrator_.reset();
    managed_network_configuration_handler_.reset();
    managed_cellular_pref_handler_.reset();
    LoginState::Shutdown();
  }

  void TriggerNetworkListChanged() {
    static_cast<NetworkStateHandlerObserver*>(apn_migrator_.get())
        ->NetworkListChanged();
  }

  void AddStub(const std::string& stub_iccid, const std::string& eid) {
    stub_cellular_networks_provider_.AddStub(stub_iccid, eid);
    network_state_helper_.network_state_handler()->SyncStubCellularNetworks();
  }

  // Creates a fake cellular device and a fake cellular service. The path of
  // the fake cellular service is returned.
  std::string AddTestCellularDeviceAndService(const std::string& device_name,
                                              const std::string& device_path,
                                              const std::string& device_iccid,
                                              const std::string& device_guid,
                                              bool is_managed = false) {
    network_state_helper_.device_test()->AddDevice(
        device_path, shill::kTypeCellular, device_name);
    network_state_helper_.device_test()->SetDeviceProperty(
        device_path, shill::kIccidProperty, base::Value(device_iccid),
        /*notify_changed=*/false);

    std::string ui_data = is_managed ? kUiData : "";
    return network_state_helper_.ConfigureService(base::StringPrintf(
        kCellularServicePattern, device_guid.c_str(), device_iccid.c_str(),
        NetworkProfileHandler::GetSharedProfilePath().c_str(),
        ui_data.c_str()));
  }

  void ClearCellularServices() { return network_state_helper_.ClearServices(); }

  const std::vector<ApnPropertiesPtr>& GetCustomApns() {
    return cros_network_config_->custom_apns();
  }

  MockManagedCellularPrefHandler* managed_cellular_pref_handler() const {
    return managed_cellular_pref_handler_.get();
  }
  MockManagedNetworkConfigurationHandler*
  managed_network_configuration_handler() const {
    return managed_network_configuration_handler_.get();
  }
  MockNetworkMetadataStore* network_metadata_store() const {
    return network_metadata_store_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper network_state_helper_{
      /*use_default_devices_and_services=*/true};
  NetworkHandlerTestHelper handler_test_helper_;
  FakeStubCellularNetworksProvider stub_cellular_networks_provider_;

  std::unique_ptr<MockManagedCellularPrefHandler>
      managed_cellular_pref_handler_;
  std::unique_ptr<MockManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<MockNetworkMetadataStore> network_metadata_store_;
  std::unique_ptr<FakeCrosNetworkConfig> cros_network_config_;

  // Class under test
  std::unique_ptr<ApnMigrator> apn_migrator_;
};

TEST_F(ApnMigratorTest, ApnRevampFlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 =
      AddTestCellularDeviceAndService(kCellularName1, kTestCellularPath1,
                                      kTestCellularIccid1, kTestCellularGuid1);
  const std::string cellular_service_path_2 =
      AddTestCellularDeviceAndService(kCellularName2, kTestCellularPath2,
                                      kTestCellularIccid2, kTestCellularGuid2);

  // Every network should be evaluated, simulate the first one as migrated, and
  // the second one as not.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1)
      .WillOnce(Return(false));

  // For the migrated network, the routine should not check for the current
  // custom APN list, but rather just resets the UserApnList.
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid1))
      .Times(0);
  base::Value::Dict expected_onc1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, /*user_apn_list=*/nullptr);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_1,
                    Truly([&expected_onc1](const base::Value::Dict& value) {
                      return expected_onc1 == value;
                    }),
                    _, _))
      .Times(1);

  // Ensure that the function does not modify the non-migrated network.
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid2))
      .Times(0);
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_2, _, _, _))
      .Times(0);

  // Function under test
  TriggerNetworkListChanged();
}

TEST_F(ApnMigratorTest, AlreadyMigratedNetworks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 =
      AddTestCellularDeviceAndService(kCellularName1, kTestCellularPath1,
                                      kTestCellularIccid1, kTestCellularGuid1);
  const std::string cellular_service_path_2 =
      AddTestCellularDeviceAndService(kCellularName2, kTestCellularPath2,
                                      kTestCellularIccid2, kTestCellularGuid2);
  const std::string cellular_service_path_3 =
      AddTestCellularDeviceAndService(kCellularName3, kTestCellularPath3,
                                      kTestCellularIccid3, kTestCellularGuid3);
  const char kTestStubIccid[] = "test_stub_iccid";
  const char kTestStubEid[] = "test_stub_eid";
  AddStub(kTestStubIccid, kTestStubEid);

  // The migrator routine will iterate through cellular networks. Stub networks
  // must be ignored. For this test, pretend that all non-stub cellular network
  // have been migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid3)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestStubIccid)))
      .Times(0);

  // Return nullptr and empty list for the first two networks.
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(nullptr));
  base::Value::List empty_apn_list;
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid2))
      .Times(1)
      .WillOnce(Return(&empty_apn_list));

  // For the third network, simulate a populated custom APN list.
  base::Value::Dict custom_apn_1;
  custom_apn_1.Set(::onc::cellular_apn::kAccessPointName, "apn_1");
  base::Value::Dict custom_apn_2;
  custom_apn_2.Set(::onc::cellular_apn::kAccessPointName, "apn_2");
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn_1));
  populated_apn_list.Append(std::move(custom_apn_2));
  EXPECT_CALL(*network_metadata_store(), GetCustomApnList(kTestCellularGuid3))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));

  // For the first and second networks, the function should update Shill with
  // empty user APN lists.
  base::Value::Dict expected_onc_1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, &empty_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_1,
                    Truly([&expected_onc_1](const base::Value::Dict& value) {
                      return expected_onc_1 == value;
                    }),
                    _, _))
      .Times(1);
  base::Value::Dict expected_onc_2 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid2, &empty_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_2,
                    Truly([&expected_onc_2](const base::Value::Dict& value) {
                      return expected_onc_2 == value;
                    }),
                    _, _))
      .Times(1);

  // Verify that Shill receives the user APNs for the third list.
  base::Value::Dict expected_onc_3 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid3, &populated_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_3,
                    Truly([&expected_onc_3](const base::Value::Dict& value) {
                      return expected_onc_3 == value;
                    }),
                    _, _))
      .Times(1);

  // Function under test.
  TriggerNetworkListChanged();
}

TEST_F(ApnMigratorTest, MigrateNetworksWithoutCustomApns) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 =
      AddTestCellularDeviceAndService(kCellularName1, kTestCellularPath1,
                                      kTestCellularIccid1, kTestCellularGuid1);
  const std::string cellular_service_path_2 =
      AddTestCellularDeviceAndService(kCellularName2, kTestCellularPath2,
                                      kTestCellularIccid2, kTestCellularGuid2);
  // Every network should be evaluated, pretend that all network need to be
  // migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid2)))
      .WillRepeatedly(Return(false));

  // Simulate that all networks do not have custom APNs
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(nullptr));
  base::Value::List empty_apn_list;
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid2))
      .Times(1)
      .WillOnce(Return(&empty_apn_list));

  // The function should only update Shill with empty user APN lists.
  base::Value::Dict expected_onc_1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, &empty_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_1,
                    Truly([&expected_onc_1](const base::Value::Dict& value) {
                      return expected_onc_1 == value;
                    }),
                    _, _))
      .Times(1);
  base::Value::Dict expected_onc_2 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid2, &empty_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_2,
                    Truly([&expected_onc_2](const base::Value::Dict& value) {
                      return expected_onc_2 == value;
                    }),
                    _, _))
      .Times(1);

  // All network should be marked as migrated
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid2)))
      .Times(1);

  // Function under test.
  TriggerNetworkListChanged();
}

TEST_F(ApnMigratorTest, MigrateNetworkAlreadyMigrating) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 =
      AddTestCellularDeviceAndService(kCellularName1, kTestCellularPath1,
                                      kTestCellularIccid1, kTestCellularGuid1);

  // We will use this delegate to simulate a late async reply
  network_handler::PropertiesCallback get_managed_properties_callback;

  // The first call to the migrator should start the migration process for
  // |cellular_service_path_1|. This will trigger a GetManagedProperties call.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  base::Value::Dict custom_apn_1;
  custom_apn_1.Set(::onc::cellular_apn::kAccessPointName, "apn_1");
  base::Value::Dict custom_apn_2;
  custom_apn_2.Set(::onc::cellular_apn::kAccessPointName, "apn_2");
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn_1));
  populated_apn_list.Append(std::move(custom_apn_2));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&get_managed_properties_callback](
                                network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // A second call should not trigger a GetManagedProperties, as the network is
  // already waiting for the async callback response.
  {
    EXPECT_CALL(*managed_cellular_pref_handler(),
                ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
        .Times(1)
        .WillOnce(Return(false));

    EXPECT_CALL(*network_metadata_store(),
                GetPreRevampCustomApnList(kTestCellularGuid1))
        .Times(0);
    EXPECT_CALL(*managed_network_configuration_handler(),
                GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                     cellular_service_path_1, _))
        .Times(0);
    // Function under test.
    TriggerNetworkListChanged();
  }

  // Execute the GetManagedProperties callback with a failure, expect that the
  // migration service does not mark the network as migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(0);
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, base::Value::Dict(),
           /*error=*/"error");
  get_managed_properties_callback.Reset();

  // A third call should trigger GetManagedProperties, as the network is no
  // longer migrating.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&get_managed_properties_callback](
                                network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();
}

TEST_F(ApnMigratorTest, MigrateNetworkNoPropertiesOrNotFound) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 =
      AddTestCellularDeviceAndService(kCellularName1, kTestCellularPath1,
                                      kTestCellularIccid1, kTestCellularGuid1);

  network_handler::PropertiesCallback get_managed_properties_callback;

  // Start the migration process for |cellular_service_path_1|.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  base::Value::Dict custom_apn;
  custom_apn.Set(::onc::cellular_apn::kAccessPointName, "apn_1");
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&get_managed_properties_callback](
                                network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // Execute the GetManagedProperties callback with no properties, expect that
  // the migration service does not mark the network as migrated.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(0);
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, /*properties=*/absl::nullopt,
           /*error=*/absl::nullopt);
  get_managed_properties_callback.Reset();

  // Start the migration process for |cellular_service_path_1| again.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&](network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // Remove the network.
  ClearCellularServices();

  // Execute the GetManagedProperties callback, expect that the migration
  // service does not mark the network as migrated as the network is no longer
  // found.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(0);
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, /*properties=*/base::Value::Dict(),
           /*error=*/absl::nullopt);
}

TEST_F(ApnMigratorTest, MigrateNetworkCustomApnRemovedDuringMigration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 =
      AddTestCellularDeviceAndService(kCellularName1, kTestCellularPath1,
                                      kTestCellularIccid1, kTestCellularGuid1);

  network_handler::PropertiesCallback get_managed_properties_callback;

  // Start the migration process for |cellular_service_path_1|.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  base::Value::Dict custom_apn;
  custom_apn.Set(::onc::cellular_apn::kAccessPointName, "apn_1");
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&get_managed_properties_callback](
                                network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // During the GetManagedProperties call, set the custom APN list to be empty.
  base::Value::List empty_apn_list;
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(1)
      .WillOnce(Return(&empty_apn_list));

  // Execute the GetManagedProperties callback, Shill should be updated with an
  // empty APN list. The network should be marked as migrated.
  base::Value::Dict expected_onc_1 = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, &empty_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_1,
                    Truly([&expected_onc_1](const base::Value::Dict& value) {
                      return expected_onc_1 == value;
                    }),
                    _, _))
      .Times(1);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1);
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, /*properties=*/base::Value::Dict(),
           /*error=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetCustomApns().empty());
}

TEST_F(ApnMigratorTest, MigrateManagedNetwork_NonMatchingSelectedApn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 = AddTestCellularDeviceAndService(
      kCellularName1, kTestCellularPath1, kTestCellularIccid1,
      kTestCellularGuid1, /*is_managed=*/true);

  // We will use this delegate to simulate a late async reply
  network_handler::PropertiesCallback get_managed_properties_callback;

  // The first call to the migrator should start the migration process for
  // |cellular_service_path_1|. This will trigger a GetManagedProperties call.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  base::Value::Dict custom_apn;
  custom_apn.Set(::onc::cellular_apn::kAccessPointName, "apn_1");
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(2)
      .WillRepeatedly(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&](network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // Execute the GetManagedProperties callback with no selected_apn, Shill
  // should be updated with an empty APN list. The network should be marked as
  // migrated.
  base::Value::List empty_apn_list;
  base::Value::Dict expected_onc = chromeos::network_config::UserApnListToOnc(
      kTestCellularGuid1, &empty_apn_list);
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_1,
                    Truly([&expected_onc](const base::Value::Dict& value) {
                      return expected_onc == value;
                    }),
                    _, _))
      .Times(1);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1);
  absl::optional<base::Value::Dict> properties = base::Value::Dict();
  properties->Set(::onc::network_config::kCellular, base::Value::Dict());
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, std::move(properties),
           /*error=*/absl::nullopt);
  get_managed_properties_callback.Reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetCustomApns().empty());

  // Attempt to migrate |cellular_service_path_1| again.
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(2)
      .WillRepeatedly(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&get_managed_properties_callback](
                                network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // Execute the GetManagedProperties callback with a non-matching selected_apn,
  // Shill should be updated with an empty APN list. The network should be
  // marked as migrated.
  EXPECT_CALL(
      *managed_network_configuration_handler(),
      SetProperties(cellular_service_path_1,
                    Truly([&expected_onc](const base::Value::Dict& value) {
                      return expected_onc == value;
                    }),
                    _, _))
      .Times(1);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1);
  base::Value::Dict selected_apn;
  selected_apn.Set(::onc::cellular_apn::kAccessPointName, "apn_2");
  base::Value::Dict cellular;
  cellular.Set(::onc::cellular::kAPN, std::move(selected_apn));
  properties = base::Value::Dict();
  properties->Set(::onc::network_config::kCellular, std::move(cellular));
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, std::move(properties),
           /*error=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetCustomApns().empty());
}

TEST_F(ApnMigratorTest, MigrateManagedNetwork_MatchingSelectedApn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kApnRevamp);

  const std::string cellular_service_path_1 = AddTestCellularDeviceAndService(
      kCellularName1, kTestCellularPath1, kTestCellularIccid1,
      kTestCellularGuid1, /*is_managed=*/true);

  // We will use this delegate to simulate a late async reply
  network_handler::PropertiesCallback get_managed_properties_callback;

  // Start the migration process for |cellular_service_path_1|. This will
  // trigger a GetManagedProperties call.
  EXPECT_CALL(*managed_cellular_pref_handler(),
              ContainsApnMigratedIccid(Eq(kTestCellularIccid1)))
      .WillRepeatedly(Return(false));
  const std::string access_point_name = "apn_1";
  base::Value::Dict custom_apn;
  custom_apn.Set(::onc::cellular_apn::kAccessPointName, access_point_name);
  base::Value::List populated_apn_list;
  populated_apn_list.Append(std::move(custom_apn));
  EXPECT_CALL(*network_metadata_store(),
              GetPreRevampCustomApnList(kTestCellularGuid1))
      .Times(2)
      .WillRepeatedly(Return(&populated_apn_list));
  EXPECT_CALL(*managed_network_configuration_handler(),
              GetManagedProperties(LoginState::Get()->primary_user_hash(),
                                   cellular_service_path_1, _))
      .Times(1)
      .WillOnce(
          WithArg<2>(Invoke([&](network_handler::PropertiesCallback callback) {
            ASSERT_TRUE(get_managed_properties_callback.is_null());
            get_managed_properties_callback = std::move(callback);
            ASSERT_FALSE(get_managed_properties_callback.is_null());
          })));
  // Function under test.
  TriggerNetworkListChanged();

  // Execute the GetManagedProperties callback with a selected_apn that matches
  // the persisted APN. This should trigger a call to CreateCustomApns().
  EXPECT_CALL(*managed_network_configuration_handler(),
              SetProperties(cellular_service_path_1, _, _, _))
      .Times(0);
  EXPECT_CALL(*managed_cellular_pref_handler(),
              AddApnMigratedIccid(Eq(kTestCellularIccid1)))
      .Times(1);
  EXPECT_TRUE(GetCustomApns().empty());
  absl::optional<base::Value::Dict> properties = base::Value::Dict();
  base::Value::Dict selected_apn;
  selected_apn.Set(::onc::cellular_apn::kAccessPointName, access_point_name);
  base::Value::Dict cellular;
  cellular.Set(::onc::cellular::kAPN, std::move(selected_apn));
  properties->Set(::onc::network_config::kCellular, std::move(cellular));
  std::move(get_managed_properties_callback)
      .Run(cellular_service_path_1, std::move(properties),
           /*error=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  const std::vector<ApnPropertiesPtr>& custom_apns = GetCustomApns();
  ASSERT_EQ(1u, custom_apns.size());
  EXPECT_EQ(access_point_name, custom_apns[0]->access_point_name);
  EXPECT_EQ(ApnState::kEnabled, custom_apns[0]->state);
}

}  // namespace ash
