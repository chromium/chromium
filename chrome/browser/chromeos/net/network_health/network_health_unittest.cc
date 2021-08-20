// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health/network_health.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using chromeos::network_config::mojom::NetworkType;

namespace {

// Constant values for fake devices and services.
constexpr char kEthServicePath[] = "/service/eth/0";
constexpr char kEthServiceName[] = "eth_service_name";
constexpr char kEthGuid[] = "eth_guid";
constexpr char kEthDevicePath[] = "/device/eth1";
constexpr char kEthName[] = "eth-name";
constexpr char kWifiServicePath[] = "service/wifi/0";
constexpr char kWifiServiceName[] = "wifi_service_name";
constexpr char kWifiGuid[] = "wifi_guid";
constexpr char kWifiDevicePath[] = "/device/wifi1";
constexpr char kWifiName[] = "wifi_device1";

class FakeNetworkEventsObserver
    : public chromeos::network_health::mojom::NetworkEventsObserver {
 public:
  // chromeos::network_health::mojom::NetworkEventsObserver:
  void OnConnectionStateChanged(
      const std::string& guid,
      chromeos::network_health::mojom::NetworkState state) override {
    connection_state_changed_event_received_ = true;
  }
  void OnSignalStrengthChanged(const std::string& guid,
                               chromeos::network_health::mojom::UInt32ValuePtr
                                   signal_strength) override {
    signal_strength_changed_event_received_ = true;
  }

  mojo::PendingRemote<chromeos::network_health::mojom::NetworkEventsObserver>
  pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  bool connection_state_changed_event_received() {
    return connection_state_changed_event_received_;
  }

  bool signal_strength_changed_event_received() {
    return signal_strength_changed_event_received_;
  }

  void reset_connection_state_changed_event_received() {
    connection_state_changed_event_received_ = false;
  }

  void reset_signal_strength_changed_event_received() {
    signal_strength_changed_event_received_ = false;
  }

 private:
  mojo::Receiver<chromeos::network_health::mojom::NetworkEventsObserver>
      receiver_{this};
  bool connection_state_changed_event_received_ = false;
  bool signal_strength_changed_event_received_ = false;
};

}  // namespace

namespace chromeos {
namespace network_health {

class NetworkHealthTest : public ::testing::Test {
 public:
  NetworkHealthTest() = default;

  void SetUp() override {
    // Wait until CrosNetworkConfigTestHelper is fully setup.
    task_environment_.RunUntilIdle();

    // Remove the default WiFi device created by network_state_helper.
    cros_network_config_test_helper_.network_state_helper().ClearDevices();
    cros_network_config_test_helper_.network_state_helper()
        .manager_test()
        ->RemoveTechnology(shill::kTypeWifi);
    task_environment_.RunUntilIdle();
  }

 protected:
  void CreateDefaultWifiDevice() {
    // Reset the network_state_helper to include the default wifi device.
    cros_network_config_test_helper_.network_state_helper()
        .ResetDevicesAndServices();
    task_environment_.RunUntilIdle();

    const auto& initial_network_health_state =
        network_health_.GetNetworkHealthState();

    ASSERT_EQ(std::size_t(1), initial_network_health_state->networks.size());

    // Check that the default wifi device created by CrosNetworkConfigTestHelper
    // exists.
    ASSERT_EQ(network_config::mojom::NetworkType::kWiFi,
              initial_network_health_state->networks[0]->type);
    ASSERT_EQ(network_health::mojom::NetworkState::kNotConnected,
              initial_network_health_state->networks[0]->state);
  }

  mojom::NetworkPtr GetNetworkHealthStateByType(
      network_config::mojom::NetworkType type) {
    const auto& network_health_state = network_health_.GetNetworkHealthState();
    for (auto& network : network_health_state->networks) {
      if (network->type == type) {
        return std::move(network);
      }
    }
    return nullptr;
  }

  void ValidateNetworkState(
      network_config::mojom::NetworkType type,
      network_health::mojom::NetworkState expected_state) {
    task_environment_.RunUntilIdle();

    const auto network_health_state = GetNetworkHealthStateByType(type);
    ASSERT_TRUE(network_health_state);
    ASSERT_EQ(expected_state, network_health_state->state);
  }

  void ValidateNetworkName(network_config::mojom::NetworkType type,
                           std::string expected_name) {
    task_environment_.RunUntilIdle();

    const auto network_health_state = GetNetworkHealthStateByType(type);
    ASSERT_TRUE(network_health_state);
    ASSERT_EQ(expected_name, network_health_state->name);
  }

  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  NetworkHealth network_health_;
};

// Test that all Network states can be represented by NetworkHealth.

TEST_F(NetworkHealthTest, NetworkStateUninitialized) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, false);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->SetTechnologyInitializing(shill::kTypeWifi, true);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kUninitialized);
}

TEST_F(NetworkHealthTest, NetworkStateDisabled) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, false);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kDisabled);
}

TEST_F(NetworkHealthTest, NetworkStateProhibited) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, true);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->SetTechnologyProhibited(shill::kTypeWifi, true);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kProhibited);
}

TEST_F(NetworkHealthTest, NetworkStateNotConnected) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, true);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kNotConnected);
}

TEST_F(NetworkHealthTest, NetworkStateConnecting) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateAssociation, true);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kConnecting);
}

TEST_F(NetworkHealthTest, NetworkStatePortal) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStatePortal, true);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kPortal);
}

TEST_F(NetworkHealthTest, NetworkStateConnected) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateReady, true);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kConnected);
}

TEST_F(NetworkHealthTest, OneWifiNetworkConnected) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kOnline);
  ValidateNetworkName(network_config::mojom::NetworkType::kWiFi,
                      kWifiServiceName);
}

TEST_F(NetworkHealthTest, MultiWifiNetwork) {
  CreateDefaultWifiDevice();
  // Create multiple wifi networks that are not connected.
  for (int i = 0; i < 3; i++) {
    std::string idx = base::NumberToString(i);
    cros_network_config_test_helper_.network_state_helper()
        .service_test()
        ->AddService(kWifiDevicePath + idx, kWifiGuid + idx,
                     kWifiServiceName + idx, shill::kTypeWifi,
                     shill::kStateIdle, true);
  }

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kNotConnected);
}

TEST_F(NetworkHealthTest, MultiWifiNetworkConnected) {
  CreateDefaultWifiDevice();
  // Create one wifi service that is online.
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiServicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);

  // Create multiple wifi services that are not connected.
  for (int i = 1; i < 3; i++) {
    std::string idx = base::NumberToString(i);
    cros_network_config_test_helper_.network_state_helper()
        .service_test()
        ->AddService(kWifiServicePath + idx, kWifiGuid + idx,
                     kWifiServiceName + idx, shill::kTypeWifi,
                     shill::kStateOffline, true);
  }

  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kOnline);
}

TEST_F(NetworkHealthTest, CreateActiveEthernet) {
  CreateDefaultWifiDevice();
  // Create an ethernet device and service, and make it the active network.
  cros_network_config_test_helper_.network_state_helper().AddDevice(
      kEthDevicePath, shill::kTypeEthernet, kEthName);

  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kEthServicePath, kEthGuid, kEthServiceName,
                   shill::kTypeEthernet, shill::kStateOnline, true);

  // Wait until the network and service have been created and configured.
  task_environment_.RunUntilIdle();

  ValidateNetworkState(network_config::mojom::NetworkType::kEthernet,
                       network_health::mojom::NetworkState::kOnline);
  ValidateNetworkName(network_config::mojom::NetworkType::kEthernet,
                      kEthServiceName);
  ValidateNetworkState(network_config::mojom::NetworkType::kWiFi,
                       network_health::mojom::NetworkState::kNotConnected);
}

TEST_F(NetworkHealthTest, ConnectionStateChangeEvent) {
  FakeNetworkEventsObserver fake_network_events_observer;
  network_health_.AddObserver(fake_network_events_observer.pending_remote());

  CreateDefaultWifiDevice();
  // Create one wifi service that is online.
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiServicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);
  // Wait until the network and service have been created and configured.
  task_environment_.RunUntilIdle();

  // A new network is online so a connection state event should have fired.
  EXPECT_EQ(
      fake_network_events_observer.connection_state_changed_event_received(),
      true);

  fake_network_events_observer.reset_connection_state_changed_event_received();
  EXPECT_EQ(
      fake_network_events_observer.connection_state_changed_event_received(),
      false);

  // Change the connection state of the service.
  cros_network_config_test_helper_.network_state_helper().SetServiceProperty(
      kWifiServicePath, shill::kStateProperty,
      base::Value(shill::kStateOffline));
  // Wait until the connection state change event has been fired.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(
      fake_network_events_observer.connection_state_changed_event_received(),
      true);
}

TEST_F(NetworkHealthTest, SignalStrengthChangeEvent) {
  FakeNetworkEventsObserver fake_network_events_observer;
  network_health_.AddObserver(fake_network_events_observer.pending_remote());

  CreateDefaultWifiDevice();
  // Create one wifi service that is online.
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiServicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);
  // Wait until the network and service have been created and configured.
  task_environment_.RunUntilIdle();

  // Since there is a new network, a signal strength event should have been
  // fired.
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      true);

  // Test for signal strength changes both below and above the allowed
  // threshold.

  // Set the signal strength to some known value.
  int signal_strength = 40;
  cros_network_config_test_helper_.network_state_helper().SetServiceProperty(
      kWifiServicePath, shill::kSignalStrengthProperty,
      base::Value(signal_strength));

  // Ensure that FakeNetworkEventsObserver instance is set correctly.
  fake_network_events_observer.reset_signal_strength_changed_event_received();
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      false);

  signal_strength = signal_strength +
                    NetworkHealth::kMaxSignalStrengthFluctuationTolerance + 1;
  cros_network_config_test_helper_.network_state_helper().SetServiceProperty(
      kWifiServicePath, shill::kSignalStrengthProperty,
      base::Value(signal_strength));

  // Wait until the signal strength value has been set in the network state
  // helper.
  task_environment_.RunUntilIdle();

  // Signal strength change fires a new event.
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      true);

  // Reset the FakeNetworkEventsObserver instance.
  fake_network_events_observer.reset_signal_strength_changed_event_received();
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      false);

  cros_network_config_test_helper_.network_state_helper().SetServiceProperty(
      kWifiServicePath, shill::kSignalStrengthProperty,
      base::Value(signal_strength +
                  NetworkHealth::kMaxSignalStrengthFluctuationTolerance));

  // Wait until the signal strength property has been set in the network state
  // helper.
  task_environment_.RunUntilIdle();

  // No event expected since the change is less than the allowed threshold.
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      false);
}

TEST_F(NetworkHealthTest, NoSignalStrengthChangeEventAfterInitialSetup) {
  FakeNetworkEventsObserver fake_network_events_observer;
  network_health_.AddObserver(fake_network_events_observer.pending_remote());

  CreateDefaultWifiDevice();
  // Create one wifi service that is online.
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiServicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);
  // Wait until the network and service have been created and configured.
  task_environment_.RunUntilIdle();

  // Since there is a new network, a signal strength event should have been
  // fired.
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      true);

  // Test for signal strength changes both below and above the allowed
  // threshold.

  // Set the signal strength to some known value.
  int signal_strength = 40;
  cros_network_config_test_helper_.network_state_helper().SetServiceProperty(
      kWifiServicePath, shill::kSignalStrengthProperty,
      base::Value(signal_strength));

  // Ensure that FakeNetworkEventsObserver instance is set correctly.
  fake_network_events_observer.reset_signal_strength_changed_event_received();
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      false);

  cros_network_config_test_helper_.network_state_helper().SetServiceProperty(
      kWifiServicePath, shill::kSignalStrengthProperty,
      base::Value(signal_strength +
                  NetworkHealth::kMaxSignalStrengthFluctuationTolerance));

  // Wait until the signal strength property has been set in the network state
  // helper.
  task_environment_.RunUntilIdle();

  // No event expected since the change is less than the allowed threshold.
  EXPECT_EQ(
      fake_network_events_observer.signal_strength_changed_event_received(),
      false);
}

}  // namespace network_health
}  // namespace chromeos
