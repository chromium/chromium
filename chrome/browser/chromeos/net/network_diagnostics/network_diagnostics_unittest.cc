// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
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
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
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

// This fakes a DebugDaemonClient by serving fake ICMP results when the
// DebugDaemonClient calls TestICMP().
class TestDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  TestDebugDaemonClient(const TestDebugDaemonClient&) = delete;
  TestDebugDaemonClient& operator=(const TestDebugDaemonClient&) = delete;

  ~TestDebugDaemonClient() override {}

  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override {
    // Invoke the test callback with fake output.
    std::move(callback).Run(base::Optional<std::string>{icmp_output_});
  }

  void set_icmp_output(const std::string& icmp_output) {
    icmp_output_ = icmp_output;
  }

 private:
  std::string icmp_output_;
};

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

}  // namespace

class NetworkDiagnosticsTest : public ::testing::Test {
 public:
  NetworkDiagnosticsTest() {
    // Set TestDebugDaemonClient
    test_debug_daemon_client_ = std::make_unique<TestDebugDaemonClient>();
    network_diagnostics_ =
        std::make_unique<NetworkDiagnostics>(test_debug_daemon_client_.get());
    network_diagnostics_->BindReceiver(
        network_diagnostics_remote_.BindNewPipeAndPassReceiver());

    // Initialize the ManagedNetworkConfigurationHandler and any associated
    // properties.
    LoginState::Initialize();
    SystemTokenCertDbStorage::Initialize();
    NetworkCertLoader::Initialize();
    InitializeManagedNetworkConfigurationHandler();
    // Note that |cros_network_config_test_helper_| must be initialized before
    // any routine is initialized (routine initialization is done in
    // NetworkDiagnostics). This is because |g_network_config_override| in
    // OverrideInProcessInstanceForTesting() must be set up before the routines
    // invoke BindToInProcessInstance(). See
    // chromeos/services/network_config/in_process_instance.cc for further
    // details.
    cros_network_config_test_helper().Initialize(
        managed_network_configuration_handler_.get());
    // Wait until |cros_network_config_test_helper_| has initialized.
    base::RunLoop().RunUntilIdle();

    // Set up properties for the WiFi service.
    SetUpWiFi();
  }

  ~NetworkDiagnosticsTest() override {
    NetworkCertLoader::Shutdown();
    SystemTokenCertDbStorage::Shutdown();
    LoginState::Shutdown();
    managed_network_configuration_handler_.reset();
    ui_proxy_config_service_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
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

  void SetUpWiFi() {
    DCHECK(wifi_path_.empty());
    // By default, NetworkStateTestHelper already adds a WiFi device, so, we
    // do not need to add one here. All that remains to be done is configuring
    // the WiFi service.
    wifi_path_ = ConfigureService(
        R"({"GUID": "wifi_guid", "Type": "wifi", "State": "online"})");
    SetServiceProperty(wifi_path_, shill::kSignalStrengthProperty,
                       base::Value(100));
    SetServiceProperty(wifi_path_, shill::kSecurityClassProperty,
                       base::Value(shill::kSecurityPsk));
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

 protected:
  std::string ConfigureService(const std::string& shill_json_string) {
    return network_state_helper().ConfigureService(shill_json_string);
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    network_state_helper().SetServiceProperty(service_path, key, value);
  }

  network_config::CrosNetworkConfigTestHelper&
  cros_network_config_test_helper() {
    return cros_network_config_test_helper_;
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }

  base::WeakPtr<NetworkDiagnosticsTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  NetworkDiagnostics* network_diagnostics() {
    return network_diagnostics_.get();
  }

  TestDebugDaemonClient* test_debug_daemon_client() {
    return test_debug_daemon_client_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::string wifi_path_;
  std::unique_ptr<TestDebugDaemonClient> test_debug_daemon_client_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;

  // |cros_network_config_test_helper_| must be initialized with the
  // ManagedConfigurationHandler. This is done in
  // InitializeManagedNetworkConfigurationHandler().
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_{
      false};
  mojo::Remote<mojom::NetworkDiagnosticsRoutines> network_diagnostics_remote_;
  std::unique_ptr<NetworkDiagnostics> network_diagnostics_;
  base::WeakPtrFactory<NetworkDiagnosticsTest> weak_factory_{this};
};

// Test whether NetworkDiagnostics can successfully invoke the
// LanConnectivity routine.
TEST_F(NetworkDiagnosticsTest, LanConnectivityReachability) {
  mojom::RoutineVerdict received_verdict;
  base::RunLoop run_loop;
  network_diagnostics()->LanConnectivity(base::BindOnce(
      [](mojom::RoutineVerdict* received_verdict,
         base::OnceClosure quit_closure, mojom::RoutineVerdict actual_verdict) {
        *received_verdict = actual_verdict;
        std::move(quit_closure).Run();
      },
      &received_verdict, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(received_verdict, mojom::RoutineVerdict::kNoProblem);
}

// Test whether NetworkDiagnostics can successfully invoke the
// SignalStrength routine.
TEST_F(NetworkDiagnosticsTest, SignalStrengthReachability) {
  mojom::RoutineVerdict received_verdict;
  std::vector<mojom::SignalStrengthProblem> received_problems;
  base::RunLoop run_loop;
  network_diagnostics()->SignalStrength(base::BindOnce(
      [](mojom::RoutineVerdict* received_verdict,
         std::vector<mojom::SignalStrengthProblem>* received_problems,
         base::OnceClosure quit_closure, mojom::RoutineVerdict actual_verdict,
         const std::vector<mojom::SignalStrengthProblem>& actual_problems) {
        *received_verdict = actual_verdict;
        *received_problems = std::move(actual_problems);
        std::move(quit_closure).Run();
      },
      &received_verdict, &received_problems, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(received_verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::SignalStrengthProblem> no_problems;
  EXPECT_EQ(received_problems, no_problems);
}

// Test whether NetworkDiagnostics can successfully invoke the
// GatewayCanBePinged routine.
TEST_F(NetworkDiagnosticsTest, GatewayCanBePingedReachability) {
  test_debug_daemon_client()->set_icmp_output(kFakeValidICMPOutput);
  mojom::RoutineVerdict received_verdict;
  std::vector<mojom::GatewayCanBePingedProblem> received_problems;
  base::RunLoop run_loop;
  network_diagnostics()->GatewayCanBePinged(base::BindOnce(
      [](mojom::RoutineVerdict* received_verdict,
         std::vector<mojom::GatewayCanBePingedProblem>* received_problems,
         base::OnceClosure quit_closure, mojom::RoutineVerdict actual_verdict,
         const std::vector<mojom::GatewayCanBePingedProblem>& actual_problems) {
        *received_verdict = actual_verdict;
        *received_problems = std::move(actual_problems);
        std::move(quit_closure).Run();
      },
      &received_verdict, &received_problems, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(received_verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::GatewayCanBePingedProblem> no_problems;
  EXPECT_EQ(received_problems, no_problems);
}

// Test whether NetworkDiagnostics can successfully invoke the
// HasSecureWiFiConnection routine.
TEST_F(NetworkDiagnosticsTest, HasSecureWiFiConnectionReachability) {
  mojom::RoutineVerdict received_verdict;
  std::vector<mojom::HasSecureWiFiConnectionProblem> received_problems;
  base::RunLoop run_loop;
  network_diagnostics()->HasSecureWiFiConnection(base::BindOnce(
      [](mojom::RoutineVerdict* received_verdict,
         std::vector<mojom::HasSecureWiFiConnectionProblem>* received_problems,
         base::OnceClosure quit_closure, mojom::RoutineVerdict actual_verdict,
         const std::vector<mojom::HasSecureWiFiConnectionProblem>&
             actual_problems) {
        *received_verdict = actual_verdict;
        *received_problems = std::move(actual_problems);
        std::move(quit_closure).Run();
      },
      &received_verdict, &received_problems, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(received_verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::HasSecureWiFiConnectionProblem> no_problems;
  EXPECT_EQ(received_problems, no_problems);
}

// Test whether NetworkDiagnostics can successfully invoke the
// DnsResolverPresent routine.
TEST_F(NetworkDiagnosticsTest, DnsResolverPresentReachability) {
  // Attach nameservers to the IPConfigs.
  SetUpNameServers(kWellFormedDnsServers);

  mojom::RoutineVerdict received_verdict;
  std::vector<mojom::DnsResolverPresentProblem> received_problems;
  base::RunLoop run_loop;
  network_diagnostics()->DnsResolverPresent(base::BindOnce(
      [](mojom::RoutineVerdict* received_verdict,
         std::vector<mojom::DnsResolverPresentProblem>* received_problems,
         base::OnceClosure quit_closure, mojom::RoutineVerdict actual_verdict,
         const std::vector<mojom::DnsResolverPresentProblem>& actual_problems) {
        *received_verdict = actual_verdict;
        *received_problems = std::move(actual_problems);
        std::move(quit_closure).Run();
      },
      &received_verdict, &received_problems, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(received_verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::DnsResolverPresentProblem> no_problems;
  EXPECT_EQ(received_problems, no_problems);
}

// TODO(khegde): Test whether NetworkDiagnostics can successfully invoke the
// DnsLatency routine. This would require a way to fake and inject the following
// into the DnsLatency routine: base::TickClock, network::mojom::HostResolver,
// and network::TestNetworkContext.
// TEST_F(NetworkDiagnosticsTest, DnsLatencyReachability) {}

// TODO(khegde): Test whether NetworkDiagnostics can successfully invoke the
// DnsResolution routine. This would require a way to fake and inject the
// following into the DnsResolution routine: network::mojom::HostResolver and
// network::TestNetworkContext.
// TEST_F(NetworkDiagnosticsTest, DnsResolutionReachability) {}

}  // namespace network_diagnostics
}  // namespace chromeos
