// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/captive_portal_routine.h"

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

class CaptivePortalRoutineTest : public ::testing::Test {
 public:
  CaptivePortalRoutineTest() {
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    InitializeManagedNetworkConfigurationHandler();
    // Note that |cros_network_config_test_helper_| must be initialized before
    // |captive_portal_routine_| is initialized in SetUpRoutine(). This
    // is because |g_network_config_override| in
    // OverrideInProcessInstanceForTesting() must be called before
    // BindToInProcessInstance() is called. See
    // chromeos/services/network_config/in_process_instance.cc for further
    // details.
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());
    captive_portal_routine_ = std::make_unique<CaptivePortalRoutine>();

    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();
  }

  CaptivePortalRoutineTest(const CaptivePortalRoutineTest&) = delete;
  CaptivePortalRoutineTest& operator=(const CaptivePortalRoutineTest&) = delete;

  ~CaptivePortalRoutineTest() override {
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
  }

  void CompareVerdict(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::CaptivePortalProblem>& expected_problems,
      mojom::RoutineVerdict actual_verdict,
      const std::vector<mojom::CaptivePortalProblem>& actual_problems) {
    DCHECK(run_loop_.running());
    EXPECT_EQ(expected_verdict, actual_verdict);
    EXPECT_EQ(expected_problems, actual_problems);
    run_loop_.Quit();
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

    // Wait until the |managed_network_configuration_handler_| is initialized
    // and set up.
    base::RunLoop().RunUntilIdle();
  }

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }
  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }
  CaptivePortalRoutine* captive_portal_routine() {
    return captive_portal_routine_.get();
  }

 protected:
  base::WeakPtr<CaptivePortalRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }
  base::RunLoop& run_loop() { return run_loop_; }

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
  base::RunLoop run_loop_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<CaptivePortalRoutine> captive_portal_routine_;
  std::string wifi_path_;
  base::WeakPtrFactory<CaptivePortalRoutineTest> weak_factory_{this};
};

// Test whether an online active network successfully passes.
TEST_F(CaptivePortalRoutineTest, TestNoCaptivePortal) {
  SetUpWiFi(shill::kStateOnline);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kNoProblem, expected_problems));
  run_loop().Run();
}

// Test whether no active networks is reported correctly.
TEST_F(CaptivePortalRoutineTest, TestNoActiveNetworks) {
  SetUpWiFi(shill::kStateOffline);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kNoActiveNetworks};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  run_loop().Run();
}

// Test that an active network with a suspected portal state is detected.
TEST_F(CaptivePortalRoutineTest, TestPortalSuspected) {
  SetUpWiFi(shill::kStatePortalSuspected);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kPortalSuspected};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  run_loop().Run();
}

// Test that an active network behind a portal is detected.
TEST_F(CaptivePortalRoutineTest, TestPortalDetected) {
  SetUpWiFi(shill::kStateRedirectFound);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kPortal};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  run_loop().Run();
}

// Test that an active network with no internet is detected.
TEST_F(CaptivePortalRoutineTest, TestNoInternet) {
  SetUpWiFi(shill::kStateNoConnectivity);
  std::vector<mojom::CaptivePortalProblem> expected_problems = {
      mojom::CaptivePortalProblem::kNoInternet};
  captive_portal_routine()->RunRoutine(
      base::BindOnce(&CaptivePortalRoutineTest::CompareVerdict, weak_ptr(),
                     mojom::RoutineVerdict::kProblem, expected_problems));
  run_loop().Run();
}

// TODO(khegde): Add a test for unknown captive portal state.

}  // namespace network_diagnostics
}  // namespace chromeos
