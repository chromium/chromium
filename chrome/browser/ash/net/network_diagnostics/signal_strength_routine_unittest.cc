// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/signal_strength_routine.h"

#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

constexpr int kGoodWiFiSignal = 80;
constexpr int kBadWiFiSignal = 20;

}  // namespace

class SignalStrengthRoutineTest : public ::testing::Test {
 public:
  SignalStrengthRoutineTest() {
    signal_strength_routine_ = std::make_unique<SignalStrengthRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
  }

  SignalStrengthRoutineTest(const SignalStrengthRoutineTest&) = delete;
  SignalStrengthRoutineTest& operator=(const SignalStrengthRoutineTest&) =
      delete;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::SignalStrengthProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_signal_strength_problems());
  }

  void SetUpWiFi(const char* state, int signal_strength) {
    DCHECK(wifi_path_.empty());
    // By default, NetworkStateTestHelper already adds a WiFi device, so, we
    // do not need to add one here. All that remains to be done is configuring
    // the WiFi service.
    wifi_path_ = ConfigureService(
        R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle"})");
    SetServiceProperty(wifi_path_, shill::kStateProperty, base::Value(state));
    SetServiceProperty(wifi_path_, shill::kSignalStrengthProperty,
                       base::Value(signal_strength));
    base::RunLoop().RunUntilIdle();
  }

  NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }
  SignalStrengthRoutine* signal_strength_routine() {
    return signal_strength_routine_.get();
  }

 protected:
  base::WeakPtr<SignalStrengthRoutineTest> weak_ptr() {
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
  const std::string& wifi_path() const { return wifi_path_; }

  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  std::unique_ptr<SignalStrengthRoutine> signal_strength_routine_;
  std::string wifi_path_;
  base::WeakPtrFactory<SignalStrengthRoutineTest> weak_factory_{this};
};

TEST_F(SignalStrengthRoutineTest, TestGoodWiFiSignal) {
  SetUpWiFi(shill::kStateOnline, kGoodWiFiSignal);
  std::vector<mojom::SignalStrengthProblem> expected_problems = {};
  signal_strength_routine()->RunRoutine(
      base::BindOnce(&SignalStrengthRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kNoProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SignalStrengthRoutineTest, TestBadWiFiSignal) {
  SetUpWiFi(shill::kStateOnline, kBadWiFiSignal);
  std::vector<mojom::SignalStrengthProblem> expected_problems = {
      mojom::SignalStrengthProblem::kWeakSignal};
  signal_strength_routine()->RunRoutine(
      base::BindOnce(&SignalStrengthRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SignalStrengthRoutineTest, TestNoWiFiConnection) {
  SetUpWiFi(shill::kStateIdle, kGoodWiFiSignal);
  std::vector<mojom::SignalStrengthProblem> expected_problems = {};
  signal_strength_routine()->RunRoutine(
      base::BindOnce(&SignalStrengthRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kNotRun, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SignalStrengthRoutineTest, TestEthernet) {
  network_state_helper().device_test()->AddDevice(
      "/device/stub_eth_device", shill::kTypeEthernet, "stub_eth_device");
  network_state_helper().ConfigureService(
      R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
  base::RunLoop().RunUntilIdle();

  std::vector<mojom::SignalStrengthProblem> expected_problems = {};
  signal_strength_routine()->RunRoutine(
      base::BindOnce(&SignalStrengthRoutineTest::CompareResult, weak_ptr(),
                     mojom::RoutineVerdict::kNotRun, expected_problems));
  base::RunLoop().RunUntilIdle();
}

}  // namespace network_diagnostics
}  // namespace ash
