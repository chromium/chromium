// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/lan_connectivity_routine.h"

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace mojom = ::chromeos::network_diagnostics::mojom;

class LanConnectivityRoutineTest : public ::testing::Test {
 public:
  LanConnectivityRoutineTest() {
    lan_connectivity_routine_ = std::make_unique<LanConnectivityRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
  }

  LanConnectivityRoutineTest(const LanConnectivityRoutineTest&) = delete;
  LanConnectivityRoutineTest& operator=(const LanConnectivityRoutineTest&) =
      delete;

  void CompareVerdict(mojom::RoutineVerdict expected_verdict,
                      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
  }

  void SetUpEthernet() {
    DCHECK(ethernet_path_.empty());
    network_state_helper().device_test()->AddDevice(
        "/device/stub_eth_device", shill::kTypeEthernet, "stub_eth_device");
    ethernet_path_ = ConfigureService(
        R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
    base::RunLoop().RunUntilIdle();
  }

  void SetUpWiFi(const char* state) {
    DCHECK(wifi_path_.empty());
    // By default, NetworkStateTestHelper already adds a WiFi device, so, we
    // do not need to add one here. All that remains to be done is configuring
    // the WiFi service.
    wifi_path_ = ConfigureService(
        R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle"})");
    SetServiceProperty(wifi_path_, shill::kStateProperty, base::Value(state));
    SetServiceProperty(wifi_path_, shill::kSignalStrengthProperty,
                       base::Value(100));
    base::RunLoop().RunUntilIdle();
  }

  LanConnectivityRoutine* lan_connectivity_routine() {
    return lan_connectivity_routine_.get();
  }

 protected:
  base::WeakPtr<LanConnectivityRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string ConfigureService(const std::string& shill_json_string) {
    return network_state_helper().ConfigureService(shill_json_string);
  }
  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    network_state_helper().SetServiceProperty(service_path, key, value);
  }
  const std::string& ethernet_path() const { return ethernet_path_; }
  NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }
  const std::string& wifi_path() const { return wifi_path_; }

  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  std::unique_ptr<LanConnectivityRoutine> lan_connectivity_routine_;
  std::string ethernet_path_;
  std::string wifi_path_;
  base::WeakPtrFactory<LanConnectivityRoutineTest> weak_factory_{this};
};

TEST_F(LanConnectivityRoutineTest, TestConnectedLan) {
  SetUpEthernet();
  SetUpWiFi(shill::kStateOnline);
  lan_connectivity_routine()->RunRoutine(
      base::BindOnce(&LanConnectivityRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kNoProblem));
  base::RunLoop().RunUntilIdle();
}

TEST_F(LanConnectivityRoutineTest, TestDisconnectedLan) {
  SetUpWiFi(shill::kStateIdle);
  lan_connectivity_routine()->RunRoutine(
      base::BindOnce(&LanConnectivityRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem));
  base::RunLoop().RunUntilIdle();
}

}  // namespace network_diagnostics
}  // namespace ash
