// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/gateway_can_be_pinged_routine.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/network/system_token_cert_db_storage.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

// Fake ICMP output. For more details, see:
// https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc.
const char kFakeValidICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 4,
        "recvd": 4,
        "time": 3005,
        "min": 5.789000,
        "avg": 5.913000,
        "max": 6.227000,
        "dev": 0.197000 }
    })";
const char kFakeInvalidICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 4,
        "recvd": 4,
        "time": 3005,
        "min": 5.789000,
        "max": 6.227000,
        "dev": 0.197000 }
    })";
const char kFakeLongLatencyICMPOutput[] = R"(
    { "4.3.2.1":
      { "sent": 4,
        "recvd": 4,
        "time": 3005000,
        "min": 5789.000,
        "avg": 5913.000,
        "max": 6227.000,
        "dev": 0.197000 }
    })";

// This fakes a DebugDaemonClient by serving fake ICMP results when the
// DebugDaemonClient calls TestICMP().
class FakeDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  FakeDebugDaemonClient() = default;

  explicit FakeDebugDaemonClient(const std::string& icmp_output)
      : icmp_output_(icmp_output) {}

  FakeDebugDaemonClient(const FakeDebugDaemonClient&) = delete;
  FakeDebugDaemonClient& operator=(const FakeDebugDaemonClient&) = delete;

  ~FakeDebugDaemonClient() override {}

  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override {
    // Invoke the test callback with fake output.
    std::move(callback).Run(base::Optional<std::string>{icmp_output_});
  }

 private:
  std::string icmp_output_;
};

}  // namespace

class GatewayCanBePingedRoutineTest : public ::testing::Test {
 public:
  GatewayCanBePingedRoutineTest() {
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    InitializeManagedNetworkConfigurationHandler();
    // Note that |cros_network_config_test_helper_| must be initialized before
    // |gateway_can_be_pinged_routine_| is initialized in SetUpRoutine(). This
    // is because |g_network_config_override| in
    // OverrideInProcessInstanceForTesting() must be called before
    // BindToInProcessInstance() is called. See
    // chromeos/services/network_config/in_process_instance.cc for further
    // details.
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());

    base::RunLoop().RunUntilIdle();
  }

  GatewayCanBePingedRoutineTest(const GatewayCanBePingedRoutineTest&) = delete;
  GatewayCanBePingedRoutineTest& operator=(
      const GatewayCanBePingedRoutineTest&) = delete;

  ~GatewayCanBePingedRoutineTest() override {
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
    managed_network_configuration_handler_.reset();
    ui_proxy_config_service_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
  }

  void CompareVerdict(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::GatewayCanBePingedProblem>& expected_problems,
      mojom::RoutineVerdict actual_verdict,
      const std::vector<mojom::GatewayCanBePingedProblem>& actual_problems) {
    EXPECT_EQ(expected_verdict, actual_verdict);
    EXPECT_EQ(expected_problems, actual_problems);
  }

  void SetUpRoutine(const std::string& icmp_output) {
    debug_daemon_client_ = std::make_unique<FakeDebugDaemonClient>(icmp_output);
    gateway_can_be_pinged_routine_ =
        std::make_unique<GatewayCanBePingedRoutine>(debug_daemon_client_.get());
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
    base::RunLoop().RunUntilIdle();
  }

  void InitializeManagedNetworkConfigurationHandler() {
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_configuration_handler_ =
        base::WrapUnique<NetworkConfigurationHandler>(
            NetworkConfigurationHandler::InitializeForTest(
                network_state_helper().network_state_handler(),
                cros_network_config_test_helper().network_device_handler()));

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());

    ui_proxy_config_service_ = std::make_unique<chromeos::UIProxyConfigService>(
        &user_prefs_, &local_state_,
        network_state_helper().network_state_handler(),
        network_profile_handler_.get());

    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_helper().network_state_handler(),
            network_profile_handler_.get(),
            cros_network_config_test_helper().network_device_handler(),
            network_configuration_handler_.get(),
            ui_proxy_config_service_.get());

    managed_network_configuration_handler_->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::ListValue(),
        /*global_network_config=*/base::DictionaryValue());

    base::RunLoop().RunUntilIdle();
  }

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }
  GatewayCanBePingedRoutine* gateway_can_be_pinged_routine() {
    return gateway_can_be_pinged_routine_.get();
  }

 protected:
  base::WeakPtr<GatewayCanBePingedRoutineTest> weak_ptr() {
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
  const std::string& wifi_path() const { return wifi_path_; }

  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_{
      false};
  std::unique_ptr<GatewayCanBePingedRoutine> gateway_can_be_pinged_routine_;
  std::unique_ptr<FakeDebugDaemonClient> debug_daemon_client_;
  std::string wifi_path_;
  std::string ethernet_path_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  base::WeakPtrFactory<GatewayCanBePingedRoutineTest> weak_factory_{this};
};

TEST_F(GatewayCanBePingedRoutineTest, TestSingleActiveNetwork) {
  SetUpRoutine(kFakeValidICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {};
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kNoProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(GatewayCanBePingedRoutineTest, TestNoActiveNetworks) {
  SetUpRoutine(kFakeValidICMPOutput);
  SetUpWiFi(shill::kStateOffline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kUnreachableGateway};
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(GatewayCanBePingedRoutineTest, TestFailureToPingDefaultNetwork) {
  // Use |kFakeInvalidICMPOutput| to handle the scenario where a bad ICMP result
  // is received.
  SetUpRoutine(kFakeInvalidICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kFailedToPingDefaultNetwork};
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(GatewayCanBePingedRoutineTest, TestDefaultNetworkAboveLatencyThreshold) {
  // Use |kFakeLongLatencyICMPOutput| to handle the scenario where the ICMP
  // result for the default network is above the threshold.
  SetUpRoutine(kFakeLongLatencyICMPOutput);
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::GatewayCanBePingedProblem> expected_problems = {
      mojom::GatewayCanBePingedProblem::kDefaultNetworkAboveLatencyThreshold};
  gateway_can_be_pinged_routine()->RunRoutine(
      base::BindOnce(&GatewayCanBePingedRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

}  // namespace network_diagnostics
}  // namespace chromeos
