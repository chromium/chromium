// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/dns_resolver_present_routine.h"

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

// The IP v4 config path specified here must match the IP v4 config path
// specified in NetworkStateTestHelper::ResetDevicesAndServices(), which itself
// is based on the IP v4 config path used to set up IP v4 configs in
// FakeShillManagerClient::SetupDefaultEnvironment().
const char kIPv4ConfigPath[] = "ipconfig_v4_path";
const std::vector<std::string> kWellFormedDnsServers = {
    "192.168.1.100", "192.168.1.101", "192.168.1.102"};
const std::vector<std::string> kMalformedDnsServers = {"0.0.0.0",
                                                       "192.168.1.100", "::/0"};
const std::vector<std::string> kEmptyDnsServers = {"192.168.1.100", ""};

}  // namespace

class DnsResolverPresentRoutineTest : public ::testing::Test {
 public:
  DnsResolverPresentRoutineTest() {
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    InitializeManagedNetworkConfigurationHandler();
    // Note that |cros_network_config_test_helper_| must be initialized before
    // |dns_resolver_present_routine_| is initialized. This is because
    // |g_network_config_override| in OverrideInProcessInstanceForTesting() must
    // be called before BindToInProcessInstance() is called. See
    // chromeos/services/network_config/in_process_instance.cc for further
    // details.
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());
    dns_resolver_present_routine_ =
        std::make_unique<DnsResolverPresentRoutine>();

    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();
  }

  DnsResolverPresentRoutineTest(const DnsResolverPresentRoutineTest&) = delete;
  DnsResolverPresentRoutineTest& operator=(
      const DnsResolverPresentRoutineTest&) = delete;

  ~DnsResolverPresentRoutineTest() override {
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
      const std::vector<mojom::DnsResolverPresentProblem>& expected_problems,
      mojom::RoutineVerdict actual_verdict,
      const std::vector<mojom::DnsResolverPresentProblem>& actual_problems) {
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
    // Wait until the |wifi_path_| is set up.
    base::RunLoop().RunUntilIdle();
  }

  // Set up the name servers and change the IPConfigs for the WiFi device and
  // service by overwriting the initial IPConfigs that are set up in
  // FakeShillManagerClient::SetupDefaultEnvironment(). Attach name
  // servers to the IP config.
  void SetUpNameServers(const std::vector<std::string>& name_servers) {
    DCHECK(!wifi_path_.empty());
    // Set up the name servers
    base::ListValue dns_servers;
    for (const std::string& name_server : name_servers) {
      dns_servers.AppendString(name_server);
    }

    // Set up the IP v4 config
    base::DictionaryValue ip_config_v4_properties;
    ip_config_v4_properties.SetKey(shill::kNameServersProperty,
                                   base::Value(dns_servers.Clone()));
    network_state_helper().ip_config_test()->AddIPConfig(
        kIPv4ConfigPath, ip_config_v4_properties);
    std::string wifi_device_path =
        network_state_helper().device_test()->GetDevicePathForType(
            shill::kTypeWifi);
    network_state_helper().device_test()->SetDeviceProperty(
        wifi_device_path, shill::kIPConfigsProperty, ip_config_v4_properties,
        /*notify_changed=*/true);
    SetServiceProperty(wifi_path_, shill::kIPConfigProperty,
                       base::Value(kIPv4ConfigPath));

    // Wait until the changed name servers have been notified (notification
    // triggered by call to SetDeviceProperty() above) and that the |wifi_path_|
    // has been set up.
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

  void RunRoutine(
      mojom::RoutineVerdict routine_verdict,
      const std::vector<mojom::DnsResolverPresentProblem>& expected_problems) {
    dns_resolver_present_routine_->RunRoutine(
        base::BindOnce(&DnsResolverPresentRoutineTest::CompareVerdict,
                       weak_ptr(), routine_verdict, expected_problems));
    run_loop().Run();
  }

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }
  DnsResolverPresentRoutine* dns_resolver_present_routine() {
    return dns_resolver_present_routine_.get();
  }
  base::RunLoop& run_loop() { return run_loop_; }

 protected:
  base::WeakPtr<DnsResolverPresentRoutineTest> weak_ptr() {
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
  base::RunLoop run_loop_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_{
      false};
  std::unique_ptr<DnsResolverPresentRoutine> dns_resolver_present_routine_;
  std::unique_ptr<FakeDebugDaemonClient> debug_daemon_client_;
  std::string wifi_path_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  base::WeakPtrFactory<DnsResolverPresentRoutineTest> weak_factory_{this};
};

TEST_F(DnsResolverPresentRoutineTest, TestResolverPresent) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers(kWellFormedDnsServers);
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(DnsResolverPresentRoutineTest, TestNoResolverPresent) {
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kNoNameServersFound});
}

TEST_F(DnsResolverPresentRoutineTest, TestMalformedNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers(kMalformedDnsServers);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kMalformedNameServers});
}

TEST_F(DnsResolverPresentRoutineTest, TestEmptyNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers(kEmptyDnsServers);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kEmptyNameServers});
}

}  // namespace network_diagnostics
}  // namespace chromeos
