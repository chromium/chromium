// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_network_state_observer.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

struct WifiInfo {
  std::string service_path;
  std::string wifi_guid;
  std::string wifi_name;
  bool is_active;
  std::optional<std::string> passphrase;
  bool auto_connect = true;
  bool save_credentials = true;
};

const WifiInfo kActiveWifi = {"stub_wifi", "wifi_guid_test",
                              "wifi-test-network",
                              /*is_active=*/true, "test-password"};

const WifiInfo kInactiveWifi = {"stub_wifi2", "wifi_guid_test2",
                                "wifi-test-network2",
                                /*is_active=*/false};

const char kDevicePath[] = "/device/stub_wifi_device";
const char kDeviceName[] = "stub_wifi_device";

bool IsPropertyValueEqualsTo(std::string key,
                             base::Value expected_value,
                             const base::Value::Dict* service_properties) {
  const base::Value* value = service_properties->Find(key);
  return !!value && (*value == expected_value);
}

}  // namespace

class KioskNetworkStateObserverTest : public WebKioskBaseTest {
 public:
  KioskNetworkStateObserverTest() = default;

  KioskNetworkStateObserverTest(const KioskNetworkStateObserverTest&) = delete;
  KioskNetworkStateObserverTest& operator=(
      const KioskNetworkStateObserverTest&) = delete;

  void SetUpOnMainThread() override {
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    WebKioskBaseTest::SetUpOnMainThread();

    network_helper_->device_test()->AddDevice(kDevicePath, shill::kTypeWifi,
                                              kDeviceName);
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    WebKioskBaseTest::TearDownOnMainThread();
  }

  void UpdateActiveWiFiCredentialsScopeChangePolicy(bool enable) {
    profile()->GetPrefs()->SetBoolean(
        prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled, enable);
  }

  KioskNetworkStateObserver& network_state_observer() const {
    return kiosk_system_session()->network_state_observer_for_testing();
  }

  void SetUpWifi(WifiInfo wifi_info) {
    network_helper_->service_test()->AddService(
        wifi_info.service_path, wifi_info.wifi_guid, wifi_info.wifi_name,
        shill::kTypeWifi,
        (wifi_info.is_active ? shill::kStateOnline : shill::kStateIdle), true);
    network_helper_->service_test()->SetServiceProperty(
        wifi_info.service_path, shill::kConnectableProperty, base::Value(true));
    network_helper_->service_test()->SetServiceProperty(
        wifi_info.service_path, shill::kProfileProperty,
        base::Value(network_helper_->ProfilePathUser()));

    if (wifi_info.passphrase.has_value()) {
      network_helper_->service_test()->SetServiceProperty(
          wifi_info.service_path, shill::kPassphraseProperty,
          base::Value(wifi_info.passphrase.value()));
      network_helper_->service_test()->SetServiceProperty(
          kActiveWifi.service_path, shill::kSecurityClassProperty,
          base::Value(shill::kSecurityClassPsk));
    }
    network_helper_->service_test()->SetServiceProperty(
        kActiveWifi.service_path, shill::kAutoConnectProperty,
        base::Value(kActiveWifi.auto_connect));
    network_helper_->service_test()->SetServiceProperty(
        kActiveWifi.service_path, shill::kSaveCredentialsProperty,
        base::Value(kActiveWifi.save_credentials));
  }

  void InitializeKiosk() {
    InitializeRegularOnlineKiosk(/*initialize_network=*/false);
    network_state_observer().SetWifiExposureAttemptCallbackForTesting(
        wifi_exposure_attempt_callback_.GetRepeatingCallback());
  }

  bool IsWiFiSuccessfullyExposedToDeviceLevel(WifiInfo wifi) {
    // Taking the value is required here. Otherwise next time this function will
    // not wait for a new callback call.
    if (!wifi_exposure_attempt_callback_.Take()) {
      return false;
    }
    const base::Value::Dict* service_properties =
        network_helper_->service_test()->GetServiceProperties(
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
        IsPropertyValueEqualsTo(shill::kGuidProperty,
                                base::Value(wifi.wifi_guid),
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

  void CreateWifiToTriggerObservers(std::string name_suffix) {
    WifiInfo new_wifi =
        WifiInfo(base::StrCat({"ServicePath_", name_suffix}),
                 base::StrCat({"WifiGuid_", name_suffix}),
                 base::StrCat({"WifiNetworkName_", name_suffix}),
                 /*is_active=*/true);
    SetUpWifi(new_wifi);
  }

  void RetryWifiExposureMaxAttempts() {
    for (size_t attempt = 1; attempt <= kMaxWifiExposureAttempts; ++attempt) {
      if (attempt == 1) {
        // First attempt is with newly set up WiFi.
      } else {
        CreateWifiToTriggerObservers(base::NumberToString(attempt));
      }
      // WiFi exposure is failed, but it continues to observe the WiFi changes.
      EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));

      if (attempt == kMaxWifiExposureAttempts) {
        // After the last attempt the observation should be stopped.
        EXPECT_FALSE(
            NetworkHandler::Get()->network_state_handler()->HasObserver(
                &network_state_observer()));
      } else {
        EXPECT_TRUE(NetworkHandler::Get()->network_state_handler()->HasObserver(
            &network_state_observer()));
      }
    }
  }

 protected:
  std::unique_ptr<NetworkStateTestHelper> network_helper_;
  base::test::TestFuture<bool> wifi_exposure_attempt_callback_;
};

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, DefaultDisabled) {
  SetUpWifi(kActiveWifi);
  InitializeKiosk();

  // The policy is disabled, so WiFi should not be exposed and we should not
  // observe WiFi changes.
  EXPECT_FALSE(network_state_observer().IsPolicyEnabled());
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, NoActiveWiFi) {
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  // When policy is enabled, observe active WiFis.
  EXPECT_TRUE(network_state_observer().IsPolicyEnabled());
  EXPECT_TRUE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, ExposeWiFi) {
  SetUpWifi(kActiveWifi);
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, ObserveNetworkChange) {
  InitializeKiosk();
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));

  // WiFi is not set up, so `KioskNetworkStateObserver` will not expose any
  // WiFi, but will observe the network change.
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);
  // When the policy is updated, the kiosk observer will try to expose an active
  // WiFi. But since there is no active WiFi, it will call the callback with a
  // result of failed attempt.
  EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
  EXPECT_TRUE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));

  SetUpWifi(kActiveWifi);
  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
  // After the successful WiFi scope change, stop observing the network.
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, ExposeOnlyActiveWiFi) {
  SetUpWifi(kInactiveWifi);
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
  const base::Value::Dict* service_properties =
      network_helper_->service_test()->GetServiceProperties(
          kInactiveWifi.service_path);
  CHECK(!!service_properties);
  // Check that we didn't change the profile for the inactive WiFi.
  EXPECT_TRUE(IsPropertyValueEqualsTo(
      shill::kProfileProperty, base::Value(network_helper_->ProfilePathUser()),
      service_properties));

  SetUpWifi(kActiveWifi);
  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest,
                       NoPassphraseMaxWifiExposureAttempts) {
  WifiInfo without_passphrase = kActiveWifi;
  without_passphrase.passphrase = {};
  SetUpWifi(without_passphrase);

  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  // Without passphrase the kiosk observer cannot copy the WiFi configuration,
  // so it will try the maximum number of attempts and them give up on exposing
  // the WiFi.
  RetryWifiExposureMaxAttempts();
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest,
                       TemporaryServiceConfiguredButNotUsable) {
  // On real devices we treat the error callback with
  // `kTemporaryServiceConfiguredButNotUsable` message as success.
  network_helper_->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  network_helper_->manager_test()->SetSimulateConfigurationError(
      shill::kErrorResultNotFound, kTemporaryServiceConfiguredButNotUsable);

  SetUpWifi(kActiveWifi);
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(wifi_exposure_attempt_callback_.Get());
  // `FakeShillManagerClient::ConfigureService` behavior is different from the
  // real one. It does nothing when `FakeShillSimulatedResult::kFailure` is set,
  // so we cannot check that the WiFi is exposed to the device level. But we can
  // check that we stopped the WiFi change observation, because
  // `KioskNetworkStateObserver` think the WiFi was successfully exposed.
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest,
                       ConfigFailureMaxWifiExposureAttempts) {
  network_helper_->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  SetUpWifi(kActiveWifi);
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  RetryWifiExposureMaxAttempts();
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest, SuccessfulSecondAttempt) {
  network_helper_->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  SetUpWifi(kActiveWifi);
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_FALSE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));

  network_helper_->manager_test()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kSuccess);

  CreateWifiToTriggerObservers("first");

  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
  // After the last successful attempt the observation should be stopped.
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));
}

IN_PROC_BROWSER_TEST_F(KioskNetworkStateObserverTest,
                       PolicyChangeRespectsPreviousWiFiExposureAttempt) {
  SetUpWifi(kActiveWifi);
  InitializeKiosk();
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  EXPECT_TRUE(IsWiFiSuccessfullyExposedToDeviceLevel(kActiveWifi));
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));

  UpdateActiveWiFiCredentialsScopeChangePolicy(false);
  UpdateActiveWiFiCredentialsScopeChangePolicy(true);

  // Do not start the WiFi observation because the WiFi was already exposed.
  EXPECT_FALSE(NetworkHandler::Get()->network_state_handler()->HasObserver(
      &network_state_observer()));
}

}  // namespace ash
