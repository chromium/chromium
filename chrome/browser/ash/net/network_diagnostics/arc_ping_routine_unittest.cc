// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/components/arc/test/fake_net_instance.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/network_diagnostics/arc_ping_routine.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/onc/network_onc_utils.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/network/system_token_cert_db_storage.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/onc/onc_pref_names.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

const int kNoProblemDelayMs = 1000;
const int kHighLatencyDelayMs = 1600;

}  // namespace

class ArcPingRoutineTest : public ::testing::Test {
 public:
  ArcPingRoutineTest() {
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    InitializeManagedNetworkConfigurationHandler();
    // Note that |cros_network_config_test_helper_| must be initialized before
    // |arc_ping_routine_| is initialized in SetUpRoutine(). This
    // is because |g_network_config_override| in
    // OverrideInProcessInstanceForTesting() must be called before
    // BindToInProcessInstance() is called. See
    // chromeos/services/network_config/in_process_instance.cc for further
    // details.
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());

    base::RunLoop().RunUntilIdle();
  }

  ArcPingRoutineTest(const ArcPingRoutineTest&) = delete;
  ArcPingRoutineTest& operator=(const ArcPingRoutineTest&) = delete;

  ~ArcPingRoutineTest() override {
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
  }

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::ArcPingProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems, result->problems->get_arc_ping_problems());
    run_loop_.Quit();
  }

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }

 protected:
  void RunRoutine(mojom::RoutineVerdict expected_routine_verdict,
                  const std::vector<mojom::ArcPingProblem>& expected_problems) {
    arc_ping_routine_->RunRoutine(
        base::BindOnce(&ArcPingRoutineTest::CompareResult, weak_ptr(),
                       expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }

  void SetUpRoutine(arc::mojom::ArcPingTestResult result) {
    // Set up the fake NetworkInstance service.
    fake_net_instance_ = std::make_unique<arc::FakeNetInstance>();
    fake_net_instance_->set_ping_test_result(result);

    // Set up routine with fake NetworkInstance service.
    arc_ping_routine_ = std::make_unique<ArcPingRoutine>();
    arc_ping_routine_->set_net_instance_for_testing(fake_net_instance_.get());
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
        NetworkConfigurationHandler::InitializeForTest(
            network_state_helper().network_state_handler(),
            cros_network_config_test_helper().network_device_handler());

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

  base::WeakPtr<ArcPingRoutineTest> weak_ptr() {
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

  // Member order declaration done in a way so that members outlive those that
  // are dependent on them.
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::unique_ptr<ArcPingRoutine> arc_ping_routine_;
  std::unique_ptr<arc::FakeNetInstance> fake_net_instance_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_{
      false};
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::string wifi_path_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  base::WeakPtrFactory<ArcPingRoutineTest> weak_factory_{this};
};

TEST_F(ArcPingRoutineTest, TestNoProblem) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = true;
  result.duration_ms = kNoProblemDelayMs;

  SetUpRoutine(result);
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(ArcPingRoutineTest, TestUnreachableGateway) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = false;

  SetUpRoutine(result);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcPingProblem::kUnreachableGateway});
}

TEST_F(ArcPingRoutineTest, TestFailedToPingDefaultGateway) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = false;

  SetUpRoutine(result);
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcPingProblem::kFailedToPingDefaultNetwork});
}

TEST_F(ArcPingRoutineTest, TestHighLatencyToPingDefaultGateway) {
  arc::mojom::ArcPingTestResult result;
  result.is_successful = true;
  result.duration_ms = kHighLatencyDelayMs;

  SetUpRoutine(result);
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::ArcPingProblem::kDefaultNetworkAboveLatencyThreshold});
}

}  // namespace network_diagnostics
}  // namespace chromeos
