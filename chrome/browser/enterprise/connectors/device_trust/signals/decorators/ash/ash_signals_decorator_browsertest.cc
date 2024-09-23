// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/ash/ash_signals_decorator.h"

#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using testing::InvokeWithoutArgs;

namespace enterprise_connectors {

namespace {

constexpr char kFakeEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kFakeAffilationID[] = "fake_affiliation_id";

constexpr char kFakeImei[] = "fake_imei";
constexpr char kFakeMeid[] = "fake_meid";
constexpr char kMacAddress[] = "00:00:00:00:00:00";
constexpr char kWifiDevicePath[] = "/device/stub_wifi";
constexpr char kCellularDevicePath[] = "/device/stub_cellular_device";
constexpr char kWifiServicePath[] = "/service/stub_wifi";

constexpr char kFakeSerialNumber[] = "fake_serial_number";
constexpr char kFakeDeviceHostName[] = "fake_device_host_name";

base::Value::List GetExpectedMacAddresses() {
  base::Value::List mac_addresses;
  mac_addresses.Append(kMacAddress);
  return mac_addresses;
}

void SetupFakeNetwork() {
  ash::ShillDeviceClient::TestInterface* shill_device_client =
      ash::ShillDeviceClient::Get()->GetTestInterface();
  ash::ShillServiceClient::TestInterface* shill_service_client =
      ash::ShillServiceClient::Get()->GetTestInterface();
  ash::ShillProfileClient::TestInterface* shill_profile_client =
      ash::ShillProfileClient::Get()->GetTestInterface();

  shill_service_client->ClearServices();
  shill_device_client->ClearDevices();

  shill_device_client->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                                 "stub_cellular_device");
  shill_device_client->SetDeviceProperty(
      kCellularDevicePath, shill::kMeidProperty, base::Value(kFakeMeid),
      /*notify_changed=*/false);
  shill_device_client->SetDeviceProperty(
      kCellularDevicePath, shill::kImeiProperty, base::Value(kFakeImei),
      /*notify_changed=*/false);

  shill_device_client->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                                 "stub_wifi_device");
  shill_device_client->SetDeviceProperty(
      kWifiDevicePath, shill::kAddressProperty, base::Value(kMacAddress),
      /*notify_changed=*/false);

  shill_service_client->AddService(kWifiServicePath, "wifi_guid",
                                   "wifi_network_name", shill::kTypeWifi,
                                   shill::kStateIdle, /*visible=*/true);
  shill_service_client->SetServiceProperty(
      kWifiServicePath, shill::kConnectableProperty, base::Value(true));

  shill_profile_client->AddService(
      ash::ShillProfileClient::GetSharedProfilePath(), kWifiServicePath);

  shill_service_client->SetServiceProperty(kWifiServicePath,
                                           shill::kStateProperty,
                                           base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class AshSignalsDecoratorBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  AshSignalsDecoratorBrowserTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~AshSignalsDecoratorBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    // Register prefs in test pref services.
    prefs_.registry()->RegisterBooleanPref(ash::prefs::kAllowScreenLock, false);

    TestingProfile::Builder profile_builder;
    testing_profile_ = profile_builder.Build();
    connector_ =
        g_browser_process->platform_part()->browser_policy_connector_ash();
  }

  void TearDownOnMainThread() override { testing_profile_.reset(); }

  Profile* testing_profile() { return testing_profile_.get(); }

  std::unique_ptr<TestingProfile> testing_profile_;
  TestingPrefServiceSimple prefs_;
  raw_ptr<policy::BrowserPolicyConnectorAsh, DanglingUntriaged> connector_;

  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(AshSignalsDecoratorBrowserTest,
                       TestStaticPolicySignals) {
  device_policy()->policy_data().set_managed_by(kFakeEnrollmentDomain);

  testing_profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                            false);
  // Set fake serial number.
  fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                kFakeSerialNumber);
  // Set fake device hostname.
  ash::NetworkHandler::Get()->network_state_handler()->SetHostname(
      kFakeDeviceHostName);

  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  base::RunLoop run_loop;
  AshSignalsDecorator decorator(connector_, testing_profile());
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceEnrollmentDomain),
            kFakeEnrollmentDomain);

  const auto* serial_number =
      signals.FindString(device_signals::names::kSerialNumber);
  ASSERT_TRUE(serial_number);
  EXPECT_EQ(*serial_number, kFakeSerialNumber);

  const auto* device_host_name =
      signals.FindString(device_signals::names::kDeviceHostName);
  ASSERT_TRUE(device_host_name);
  EXPECT_EQ(*device_host_name, kFakeDeviceHostName);

  auto disk_encrypted = signals.FindInt(device_signals::names::kDiskEncrypted);
  ASSERT_TRUE(disk_encrypted);
  EXPECT_EQ(disk_encrypted.value(),
            static_cast<int32_t>(enterprise_signals::SettingValue::ENABLED));

  auto screen_lock_secured =
      signals.FindInt(device_signals::names::kScreenLockSecured);
  ASSERT_TRUE(screen_lock_secured);
  EXPECT_EQ(screen_lock_secured.value(),
            static_cast<int32_t>(enterprise_signals::SettingValue::ENABLED));
}

IN_PROC_BROWSER_TEST_F(AshSignalsDecoratorBrowserTest, TestNetworkSignals) {
  device_policy()->policy_data().add_device_affiliation_ids(kFakeAffilationID);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  AshSignalsDecorator decorator(connector_, profile);

  user_manager::UserManager::Get()->SetUserAffiliated(user->GetAccountId(),
                                                      /*is_affiliated=*/true);

  // Test for no network
  {
    base::RunLoop run_loop;
    base::Value::Dict signals;
    decorator.Decorate(signals, run_loop.QuitClosure());

    run_loop.Run();

    base::Value::List* imei_list =
        signals.FindList(device_signals::names::kImei);
    ASSERT_TRUE(imei_list);
    EXPECT_TRUE(imei_list->empty());

    base::Value::List* meid_list =
        signals.FindList(device_signals::names::kMeid);
    ASSERT_TRUE(meid_list);
    EXPECT_TRUE(meid_list->empty());
  }

  // Test for fake network
  {
    SetupFakeNetwork();

    base::RunLoop run_loop;
    base::Value::Dict signals;
    decorator.Decorate(signals, run_loop.QuitClosure());

    run_loop.Run();

    const auto* mac_addresses =
        signals.FindList(device_signals::names::kMacAddresses);
    ASSERT_TRUE(mac_addresses);
    EXPECT_EQ(*mac_addresses, GetExpectedMacAddresses());

    base::Value::List* imei_list =
        signals.FindList(device_signals::names::kImei);
    EXPECT_EQ(imei_list->size(), 1u);
    EXPECT_EQ(imei_list->front(), kFakeImei);

    base::Value::List* meid_list =
        signals.FindList(device_signals::names::kMeid);
    EXPECT_EQ(meid_list->size(), 1u);
    EXPECT_EQ(meid_list->front(), kFakeMeid);
  }
}

IN_PROC_BROWSER_TEST_F(AshSignalsDecoratorBrowserTest, TestSignalTrigger) {
  // Test with user profile
  {
    base::RunLoop run_loop;
    AshSignalsDecorator decorator(connector_, testing_profile());
    base::Value::Dict signals;
    decorator.Decorate(signals, run_loop.QuitClosure());

    run_loop.Run();

    auto browser_context_type =
        signals.FindInt(device_signals::names::kTrigger);
    ASSERT_TRUE(browser_context_type);
    EXPECT_EQ(
        browser_context_type.value(),
        static_cast<int32_t>(device_signals::Trigger::kBrowserNavigation));
  }

  // Test with signin profile
  {
    base::RunLoop run_loop;
    AshSignalsDecorator decorator(
        connector_,
        ash::ProfileHelper::GetSigninProfile()->GetOriginalProfile());
    base::Value::Dict signals;
    decorator.Decorate(signals, run_loop.QuitClosure());

    run_loop.Run();

    auto browser_context_type =
        signals.FindInt(device_signals::names::kTrigger);
    ASSERT_TRUE(browser_context_type);
    EXPECT_EQ(browser_context_type.value(),
              static_cast<int32_t>(device_signals::Trigger::kLoginScreen));
  }
}

}  // namespace enterprise_connectors
