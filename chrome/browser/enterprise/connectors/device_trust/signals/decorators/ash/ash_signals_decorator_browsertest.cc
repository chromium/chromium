// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/ash/ash_signals_decorator.h"

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using testing::InvokeWithoutArgs;

namespace enterprise_connectors {

namespace {

constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kFakeCustomerId[] = "fake_obfuscated_customer_id";
constexpr char kFakeEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kFakeAffilationID[] = "fake_affiliation_id";

constexpr char kFakeImei[] = "fake_imei";
constexpr char kFakeMeid[] = "fake_meid";
constexpr char kMacAddress[] = "0123456789AB";
constexpr char kIpv4Address[] = "192.168.0.42";
constexpr char kIpv6Address[] = "fe80::1262:d0ff:fef5:e8a9";
constexpr char kWifiDevicePath[] = "/device/stub_wifi";
constexpr char kWifiServicePath[] = "/service/stub_wifi";
constexpr char kWifiIPConfigV4Path[] = "/ipconfig/stub_wifi-ipv4";
constexpr char kWifiIPConfigV6Path[] = "/ipconfig/stub_wifi-ipv6";

void SetupFakeNetwork() {
  ash::ShillDeviceClient::TestInterface* shill_device_client =
      ash::ShillDeviceClient::Get()->GetTestInterface();
  ash::ShillIPConfigClient::TestInterface* shill_ipconfig_client =
      ash::ShillIPConfigClient::Get()->GetTestInterface();
  ash::ShillServiceClient::TestInterface* shill_service_client =
      ash::ShillServiceClient::Get()->GetTestInterface();
  ash::ShillProfileClient::TestInterface* shill_profile_client =
      ash::ShillProfileClient::Get()->GetTestInterface();

  shill_service_client->ClearServices();
  shill_device_client->ClearDevices();

  shill_device_client->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                                 "stub_wifi_device");
  shill_device_client->SetDeviceProperty(
      kWifiDevicePath, shill::kAddressProperty, base::Value(kMacAddress),
      /* notify_changed= */ false);

  shill_device_client->SetDeviceProperty(kWifiDevicePath, shill::kMeidProperty,
                                         base::Value(kFakeMeid),
                                         /*notify_changed=*/false);
  shill_device_client->SetDeviceProperty(kWifiDevicePath, shill::kImeiProperty,
                                         base::Value(kFakeImei),
                                         /*notify_changed=*/false);

  base::DictionaryValue ipconfig_v4_dictionary;
  ipconfig_v4_dictionary.SetKey(shill::kAddressProperty,
                                base::Value(kIpv4Address));
  ipconfig_v4_dictionary.SetKey(shill::kMethodProperty,
                                base::Value(shill::kTypeIPv4));
  shill_ipconfig_client->AddIPConfig(kWifiIPConfigV4Path,
                                     ipconfig_v4_dictionary);

  base::DictionaryValue ipconfig_v6_dictionary;
  ipconfig_v6_dictionary.SetKey(shill::kAddressProperty,
                                base::Value(kIpv6Address));
  ipconfig_v6_dictionary.SetKey(shill::kMethodProperty,
                                base::Value(shill::kTypeIPv6));
  shill_ipconfig_client->AddIPConfig(kWifiIPConfigV6Path,
                                     ipconfig_v6_dictionary);

  base::ListValue ip_configs;
  ip_configs.Append(kWifiIPConfigV4Path);
  ip_configs.Append(kWifiIPConfigV6Path);
  shill_device_client->SetDeviceProperty(kWifiDevicePath,
                                         shill::kIPConfigsProperty, ip_configs,
                                         /*notify_changed=*/false);

  shill_service_client->AddService(kWifiServicePath, "wifi_guid",
                                   "wifi_network_name", shill::kTypeWifi,
                                   shill::kStateIdle, /* visible= */ true);
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
  policy::BrowserPolicyConnectorAsh* connector_;
};

IN_PROC_BROWSER_TEST_F(AshSignalsDecoratorBrowserTest,
                       TestStaticPolicySignals) {
  device_policy()->policy_data().set_directory_api_id(kFakeDeviceId);
  device_policy()->policy_data().set_obfuscated_customer_id(kFakeCustomerId);
  device_policy()->policy_data().set_managed_by(kFakeEnrollmentDomain);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  testing_profile()->GetPrefs()->SetBoolean(ash::prefs::kAllowScreenLock,
                                            false);

  base::RunLoop run_loop;
  AshSignalsDecorator decorator(connector_, testing_profile());
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceId),
            kFakeDeviceId);
  EXPECT_EQ(*signals.FindString(device_signals::names::kObfuscatedCustomerId),
            kFakeCustomerId);
  EXPECT_EQ(*signals.FindString(device_signals::names::kEnrollmentDomain),
            kFakeEnrollmentDomain);
}

IN_PROC_BROWSER_TEST_F(AshSignalsDecoratorBrowserTest, TestNetworkSignals) {
  device_policy()->policy_data().add_device_affiliation_ids(kFakeAffilationID);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  AshSignalsDecorator decorator(connector_, profile);

  std::set<std::string> user_affiliation_ids;
  user_affiliation_ids.insert(kFakeAffilationID);

  ash::ChromeUserManager::Get()->SetUserAffiliation(user->GetAccountId(),
                                                    user_affiliation_ids);

  SetupFakeNetwork();

  base::RunLoop run_loop;
  base::Value::Dict signals;
  decorator.Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(*signals.FindString(device_signals::names::kIpAddress),
            kIpv6Address);

  base::Value::List* imei_list = signals.FindList(device_signals::names::kImei);
  EXPECT_EQ(imei_list->size(), 1);
  EXPECT_EQ(imei_list->front(), kFakeImei);

  base::Value::List* meid_list = signals.FindList(device_signals::names::kMeid);
  EXPECT_EQ(meid_list->size(), 1);
  EXPECT_EQ(meid_list->front(), kFakeMeid);
}

}  // namespace enterprise_connectors
