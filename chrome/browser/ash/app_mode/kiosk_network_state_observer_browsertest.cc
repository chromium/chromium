// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_network_state_observer.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

using kiosk::test::OfflineEnabledChromeAppV1;
using kiosk::test::WaitKioskLaunched;

namespace {

// Encapsulates the data needed by `NetworkStateTestHelper` to add a new test
// WiFi network.
struct WiFiServiceInfo {
  std::string service_path;
  std::string guid;
  std::string name;
  std::string state = shill::kStateIdle;
  std::optional<std::string> passphrase;
  bool auto_connect = true;
  bool save_credentials = true;
};

constexpr WiFiServiceInfo kOnlineService = {/*service_path=*/"stub_wifi",
                                            /*guid=*/"wifi_guid_test",
                                            /*name=*/"test-wifi-network",
                                            /*state=*/shill::kStateOnline,
                                            /*passphrase=*/"test-password"};

constexpr WiFiServiceInfo kIdleService = {/*service_path=*/"stub_wifi2",
                                          /*guid=*/"wifi_guid_test2",
                                          /*name=*/"test-wifi-network2",
                                          /*state=*/shill::kStateIdle};

WiFiServiceInfo ServiceInfoWithSuffix(std::string suffix) {
  return {/*service_path=*/base::StrCat({"ServicePath_", suffix}),
          /*guid=*/base::StrCat({"Guid_", suffix}),
          /*name=*/base::StrCat({"NetworkName_", suffix}),
          /*state=*/shill::kStateOnline};
}

bool IsPropertyValueEqualsTo(std::string key,
                             base::Value expected_value,
                             const base::Value::Dict* service_properties) {
  const base::Value* value = service_properties->Find(key);
  return !!value && (*value == expected_value);
}

KioskNetworkStateObserver& GetNetworkStateObserver() {
  return KioskController::Get()
      .GetKioskSystemSession()
      ->network_state_observer_for_testing();
}

bool KioskIsObservingNetworkState() {
  return NetworkHandler::Get()->network_state_handler()->HasObserver(
      &GetNetworkStateObserver());
}

}  // namespace

class KioskNetworkStateObserverTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskNetworkStateObserverTest() = default;

  KioskNetworkStateObserverTest(const KioskNetworkStateObserverTest&) = delete;
  KioskNetworkStateObserverTest& operator=(
      const KioskNetworkStateObserverTest&) = delete;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    network_helper().device_test()->AddDevice(
        /*device_path=*/"/device/stub_wifi_device",
        /*type=*/shill::kTypeWifi,
        /*name=*/"stub_wifi_device");
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void UpdateActiveWiFiCredentialsScopeChangePolicy(bool enable) {
    kiosk::test::CurrentProfile().GetPrefs()->SetBoolean(
        prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled, enable);
  }

  void AddNetworkService(WiFiServiceInfo info) {
    network_helper().service_test()->AddService(info.service_path, info.guid,
                                                info.name, shill::kTypeWifi,
                                                info.state, /*visible=*/true);
    network_helper().service_test()->SetServiceProperty(
        info.service_path, shill::kConnectableProperty, base::Value(true));
    network_helper().service_test()->SetServiceProperty(
        info.service_path, shill::kProfileProperty,
        base::Value(network_helper().ProfilePathUser()));

    if (info.passphrase.has_value()) {
      network_helper().service_test()->SetServiceProperty(
          info.service_path, shill::kPassphraseProperty,
          base::Value(info.passphrase.value()));
      network_helper().service_test()->SetServiceProperty(
          info.service_path, shill::kSecurityClassProperty,
          base::Value(shill::kSecurityClassPsk));
    }
    network_helper().service_test()->SetServiceProperty(
        info.service_path, shill::kAutoConnectProperty,
        base::Value(info.auto_connect));
    network_helper().service_test()->SetServiceProperty(
        info.service_path, shill::kSaveCredentialsProperty,
        base::Value(info.save_credentials));
  }

  void MonitorWifiExposureAttempts() {
    GetNetworkStateObserver().SetWifiExposureAttemptCallbackForTesting(
        exposure_attempt_.GetRepeatingCallback());
  }

  bool IsWiFiSuccessfullyExposedToDeviceLevel(WiFiServiceInfo wifi) {
    // Taking the value is required here. Otherwise next time this function will
    // not wait for a new callback call.
    if (!exposure_attempt_.Take()) {
      return false;
    }
    const base::Value::Dict* service_properties =
        network_helper().service_test()->GetServiceProperties(
            wifi.service_path);
    if (!service_properties) {
      return false;
    }

    return
        // Shared profile means the WiFi is configured on the device level.
        IsPropertyValueEqualsTo(
            shill::kProfileProperty,
            base::Value(NetworkProfileHandler::GetSharedProfilePath()),
            service_properties) &&
        IsPropertyValueEqualsTo(shill::kPassphraseProperty,
                                base::Value(wifi.passphrase.value()),
                                service_properties) &&
        IsPropertyValueEqualsTo(shill::kSecurityClassProperty,
                                base::Value(shill::kSecurityClassPsk),
                                service_properties) &&
        IsPropertyValueEqualsTo(shill::kGuidProperty, base::Value(wifi.guid),
                                service_properties) &&
        IsPropertyValueEqualsTo(shill::kAutoConnectProperty,
                                base::Value(wifi.auto_connect),
                                service_properties) &&
        IsPropertyValueEqualsTo(shill::kTypeProperty,
                                base::Value(shill::kTypeWifi),
                                service_properties) &&
        IsPropertyValueEqualsTo(shill::kSaveCredentialsProperty,
                                base::Value(wifi.save_credentials),
                                service_properties) &&
        IsPropertyValueEqualsTo(shill::kModeProperty,
                                base::Value(shill::kModeManaged),
                                service_properties);
  }

  void RetryWifiExposureMaxAttempts() {
    for (size_t attempt = 1; attempt <= kMaxWifiExposureAttempts; ++attempt) {
      // The first attempt happens from an existing network. Create new networks
      // to trigger following attempts.
      if (attempt > 1) {
        AddNetworkService(ServiceInfoWithSuffix(base::NumberToString(attempt)));
      }

      EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));

      // The observer remains registered until the last attempt.
      if (attempt < kMaxWifiExposureAttempts) {
        EXPECT_TRUE(KioskIsObservingNetworkState());
      } else {
        EXPECT_FALSE(KioskIsObservingNetworkState());
      }
    }
  }

  NetworkStateTestHelper& network_helper() const {
    return CHECK_DEREF(network_helper_.get());
  }

 protected:
  base::test::TestFuture<bool> exposure_attempt_;

  std::unique_ptr<NetworkStateTestHelper> network_helper_;

  KioskMixin kiosk_{&mixin_host_, /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, DefaultDisabled) {
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();

  // The policy is disabled, so WiFi should not be exposed and we should not
  // observe WiFi changes.
  EXPECT_FALSE(GetNetworkStateObserver().IsPolicyEnabled());
  EXPECT_FALSE(KioskIsObservingNetworkState());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, PRE_NoActiveWiFi) {
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, NoActiveWiFi) {
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  // When policy is enabled, observe active WiFis.
  EXPECT_TRUE(GetNetworkStateObserver().IsPolicyEnabled());
  EXPECT_TRUE(KioskIsObservingNetworkState());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, ExposeWiFi) {
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest,
                       PRE_ObserveNetworkChange) {
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, ObserveNetworkChange) {
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  EXPECT_FALSE(KioskIsObservingNetworkState());

  // WiFi is not set up, so `KioskNetworkStateObserver` will not expose any
  // WiFi, but will observe the network change.
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);
  // When the policy is updated, the kiosk observer will try to expose an active
  // WiFi. But since there is no active WiFi, it will call the callback with a
  // result of failed attempt.
  EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
  EXPECT_TRUE(KioskIsObservingNetworkState());

  AddNetworkService(kOnlineService);
  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
  // After the successful WiFi scope change, stop observing the network.
  EXPECT_FALSE(KioskIsObservingNetworkState());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest,
                       PRE_ExposeOnlyActiveWiFi) {
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, ExposeOnlyActiveWiFi) {
  AddNetworkService(kIdleService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
  const base::Value::Dict* service_properties =
      network_helper().service_test()->GetServiceProperties(
          kIdleService.service_path);
  ASSERT_NE(service_properties, nullptr);
  // Check that we didn't change the profile for the inactive WiFi.
  EXPECT_TRUE(IsPropertyValueEqualsTo(
      shill::kProfileProperty, base::Value(network_helper().ProfilePathUser()),
      service_properties));

  AddNetworkService(kOnlineService);
  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest,
                       NoPassphraseMaxWifiExposureAttempts) {
  WiFiServiceInfo without_passphrase = kOnlineService;
  without_passphrase.passphrase = {};
  AddNetworkService(without_passphrase);

  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  // Without passphrase the kiosk observer cannot copy the WiFi configuration,
  // so it will try the maximum number of attempts and them give up on exposing
  // the WiFi.
  RetryWifiExposureMaxAttempts();
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest,
                       TemporaryServiceConfiguredButNotUsable) {
  // On real devices we treat the error callback with
  // `kTemporaryServiceConfiguredButNotUsable` message as success.
  network_helper().manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  network_helper().manager_test()->SetSimulateConfigurationError(
      shill::kErrorResultNotFound, kTemporaryServiceConfiguredButNotUsable);

  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(exposure_attempt_.Get());
  // `FakeShillManagerClient::ConfigureService` behavior is different from the
  // real one. It does nothing when `FakeShillSimulatedResult::kFailure` is set,
  // so we cannot check that the WiFi is exposed to the device level. But we can
  // check that we stopped the WiFi change observation, because
  // `KioskNetworkStateObserver` think the WiFi was successfully exposed.
  EXPECT_FALSE(KioskIsObservingNetworkState());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest,
                       ConfigFailureMaxWifiExposureAttempts) {
  network_helper().manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  RetryWifiExposureMaxAttempts();
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest, SuccessfulSecondAttempt) {
  network_helper().manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));

  network_helper().manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kSuccess);

  AddNetworkService(ServiceInfoWithSuffix("first"));

  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
  // After the last successful attempt the observation should be stopped.
  EXPECT_FALSE(KioskIsObservingNetworkState());
}

IN_PROC_BROWSER_TEST_P(KioskNetworkStateObserverTest,
                       PolicyChangeRespectsPreviousWiFiExposureAttempt) {
  AddNetworkService(kOnlineService);
  ASSERT_TRUE(WaitKioskLaunched());
  MonitorWifiExposureAttempts();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kOnlineService));
  EXPECT_FALSE(KioskIsObservingNetworkState());

  UpdateActiveWiFiCredentialsScopeChangePolicy(false);
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  // Do not start the WiFi observation because the WiFi was already exposed.
  EXPECT_FALSE(KioskIsObservingNetworkState());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskNetworkStateObserverTest,
    testing::Values(
        // TODO(crbug.com/379633748): Add IWA.
        KioskMixin::Config{/*name=*/"WebApp",
                           KioskMixin::AutoLaunchAccount{
                               KioskMixin::SimpleWebAppOption().account_id},
                           {KioskMixin::SimpleWebAppOption()}},
        KioskMixin::Config{/*name=*/"ChromeApp",
                           KioskMixin::AutoLaunchAccount{
                               OfflineEnabledChromeAppV1().account_id},
                           // The Chrome app needs to be offline enabled because
                           // some tests will launch it while offline.
                           {OfflineEnabledChromeAppV1()}}),
    KioskMixin::ConfigName);

}  // namespace ash
