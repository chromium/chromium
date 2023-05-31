// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_test_helper.h"

#include "base/values.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "components/user_manager/fake_user_manager.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

NetworkDiagnosticsTestHelper::NetworkDiagnosticsTestHelper()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  // TODO(b/278643115) Remove LoginState dependency.
  LoginState::Initialize();

  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<user_manager::FakeUserManager>());

  helper_ = std::make_unique<NetworkHandlerTestHelper>();
  helper_->AddDefaultProfiles();
  helper_->ResetDevicesAndServices();
  helper_->RegisterPrefs(user_prefs_.registry(), local_state_.registry());

  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
  helper_->InitializePrefs(&user_prefs_, &local_state_);

  NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      /*userhash=*/std::string(),
      /*network_configs_onc=*/base::Value::List(),
      /*global_network_config=*/base::Value::Dict());

  cros_network_config_ = std::make_unique<network_config::CrosNetworkConfig>();
  network_config::OverrideInProcessInstanceForTesting(
      cros_network_config_.get());
  task_environment_.RunUntilIdle();
}

NetworkDiagnosticsTestHelper::~NetworkDiagnosticsTestHelper() {
  cros_network_config_.reset();
  helper_.reset();
  scoped_user_manager_.reset();
  LoginState::Shutdown();
}

void NetworkDiagnosticsTestHelper::SetUpWiFi(const char* state) {
  ASSERT_TRUE(wifi_path_.empty());
  // By default, NetworkStateTestHelper already adds a WiFi device, so, we
  // do not need to add one here. All that remains to be done is configuring
  // the WiFi service.
  wifi_guid_ = "wifi_guid";
  wifi_path_ = helper_->ConfigureService(
      R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle",
            "SSID": "wifi", "Strength": 100, "AutoConnect": true,
            "WiFi.HiddenSSID": false,
            "TrafficCounterResetTime": 0})");
  SetServiceProperty(wifi_path_, shill::kStateProperty, base::Value(state));
  helper_->profile_test()->AddService(
      NetworkProfileHandler::GetSharedProfilePath(), wifi_path_);
  task_environment_.RunUntilIdle();
}

std::string NetworkDiagnosticsTestHelper::ConfigureService(
    const std::string& shill_json_string) {
  return helper_->ConfigureService(shill_json_string);
}

void NetworkDiagnosticsTestHelper::SetServiceProperty(
    const std::string& service_path,
    const std::string& key,
    const base::Value& value) {
  helper_->SetServiceProperty(service_path, key, value);
}

}  // namespace network_diagnostics
}  // namespace ash
