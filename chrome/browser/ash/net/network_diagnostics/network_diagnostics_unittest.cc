// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_test_helper.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// The IP v4 config path specified here must match the IP v4 config path
// specified in NetworkStateTestHelper::ResetDevicesAndServices(), which itself
// is based on the IP v4 config path used to set up IP v4 configs in
// FakeShillManagerClient::SetupDefaultEnvironment().
const char kIPv4ConfigPath[] = "ipconfig_v4_path";
const std::vector<std::string> kWellFormedDnsServers = {
    "192.168.1.100", "192.168.1.101", "192.168.1.102"};

// This fakes a DebugDaemonClient by serving fake ICMP results when the
// DebugDaemonClient calls TestICMP().
class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  TestDebugDaemonClient(const TestDebugDaemonClient&) = delete;
  TestDebugDaemonClient& operator=(const TestDebugDaemonClient&) = delete;

  ~TestDebugDaemonClient() override {}

  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override {
    // Invoke the test callback with fake output.
    std::move(callback).Run(std::optional<std::string>{icmp_output_});
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

class NetworkDiagnosticsTest : public NetworkDiagnosticsTestHelper {
 public:
  NetworkDiagnosticsTest() {
    // Set TestDebugDaemonClient
    test_debug_daemon_client_ = std::make_unique<TestDebugDaemonClient>();
    network_diagnostics_ =
        std::make_unique<NetworkDiagnostics>(test_debug_daemon_client_.get());
    network_diagnostics_->BindReceiver(
        network_diagnostics_remote_.BindNewPipeAndPassReceiver());

    // Set up properties for the WiFi service.
    SetUpWiFi(shill::kStateOnline);
    SetServiceProperty(wifi_path(), shill::kSecurityClassProperty,
                       base::Value(shill::kSecurityClassPsk));

    base::RunLoop().RunUntilIdle();
  }

  ~NetworkDiagnosticsTest() override = default;

  // Set up the name servers and change the IPConfigs for the WiFi device and
  // service by overwriting the initial IPConfigs that are set up in
  // FakeShillManagerClient::SetupDefaultEnvironment(). Attach name
  // servers to the IP config.
  void SetUpNameServers(const std::vector<std::string>& name_servers) {
    DCHECK(!wifi_path().empty());
    // Set up the name servers
    base::Value::List dns_servers;
    for (const std::string& name_server : name_servers) {
      dns_servers.Append(name_server);
    }

    // Set up the IP v4 config
    auto ip_config_v4_properties = base::Value::Dict().Set(
        shill::kNameServersProperty, std::move(dns_servers));
    helper()->ip_config_test()->AddIPConfig(kIPv4ConfigPath,
                                            ip_config_v4_properties.Clone());
    std::string wifi_device_path =
        helper()->device_test()->GetDevicePathForType(shill::kTypeWifi);
    helper()->device_test()->SetDeviceProperty(
        wifi_device_path, shill::kIPConfigsProperty,
        base::Value(std::move(ip_config_v4_properties)),
        /*notify_changed=*/true);
    SetServiceProperty(wifi_path(), shill::kIPConfigProperty,
                       base::Value(kIPv4ConfigPath));
  }

 protected:
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
  std::unique_ptr<TestDebugDaemonClient> test_debug_daemon_client_;
  mojo::Remote<mojom::NetworkDiagnosticsRoutines> network_diagnostics_remote_;
  std::unique_ptr<NetworkDiagnostics> network_diagnostics_;
  base::WeakPtrFactory<NetworkDiagnosticsTest> weak_factory_{this};
};

// Test whether NetworkDiagnostics can successfully invoke the
// LanConnectivity routine.
TEST_F(NetworkDiagnosticsTest, RunLanConnectivityReachability) {
  base::RunLoop run_loop;
  mojom::RoutineResultPtr result;
  network_diagnostics()->RunLanConnectivity(
      mojom::RoutineCallSource::kDiagnosticsUI,
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr response) {
        result = std::move(response);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::LanConnectivityProblem> no_problems;
  EXPECT_EQ(result->problems->get_lan_connectivity_problems(), no_problems);
  EXPECT_EQ(result->source, mojom::RoutineCallSource::kDiagnosticsUI);
}

// Test whether NetworkDiagnostics can successfully invoke the
// SignalStrength routine.
TEST_F(NetworkDiagnosticsTest, RunSignalStrengthReachability) {
  base::RunLoop run_loop;
  mojom::RoutineResultPtr result;
  network_diagnostics()->RunSignalStrength(
      mojom::RoutineCallSource::kDiagnosticsUI,
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr response) {
        result = std::move(response);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::SignalStrengthProblem> no_problems;
  EXPECT_EQ(result->problems->get_signal_strength_problems(), no_problems);
  EXPECT_EQ(result->source, mojom::RoutineCallSource::kDiagnosticsUI);
}

// Test whether NetworkDiagnostics can successfully invoke the
// GatewayCanBePinged routine.
TEST_F(NetworkDiagnosticsTest, RunGatewayCanBePingedReachability) {
  test_debug_daemon_client()->set_icmp_output(kFakeValidICMPOutput);
  base::RunLoop run_loop;
  mojom::RoutineResultPtr result;
  network_diagnostics()->RunGatewayCanBePinged(
      mojom::RoutineCallSource::kDiagnosticsUI,
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr response) {
        result = std::move(response);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::GatewayCanBePingedProblem> no_problems;
  EXPECT_EQ(result->problems->get_gateway_can_be_pinged_problems(),
            no_problems);
  EXPECT_EQ(result->source, mojom::RoutineCallSource::kDiagnosticsUI);
}

// Test whether NetworkDiagnostics can successfully invoke the
// HasSecureWiFiConnection routine.
TEST_F(NetworkDiagnosticsTest, RunHasSecureWiFiConnectionReachability) {
  base::RunLoop run_loop;
  mojom::RoutineResultPtr result;
  network_diagnostics()->RunHasSecureWiFiConnection(
      mojom::RoutineCallSource::kDiagnosticsUI,
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr response) {
        result = std::move(response);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::HasSecureWiFiConnectionProblem> no_problems;
  EXPECT_EQ(result->problems->get_has_secure_wifi_connection_problems(),
            no_problems);
  EXPECT_EQ(result->source, mojom::RoutineCallSource::kDiagnosticsUI);
}

// Test whether NetworkDiagnostics can successfully invoke the
// DnsResolverPresent routine.
TEST_F(NetworkDiagnosticsTest, RunDnsResolverPresentReachability) {
  // Attach nameservers to the IPConfigs.
  SetUpNameServers(kWellFormedDnsServers);

  base::RunLoop run_loop;
  mojom::RoutineResultPtr result;
  network_diagnostics()->RunDnsResolverPresent(
      mojom::RoutineCallSource::kDiagnosticsUI,
      base::BindLambdaForTesting([&](mojom::RoutineResultPtr response) {
        result = std::move(response);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(result->verdict, mojom::RoutineVerdict::kNoProblem);
  std::vector<mojom::DnsResolverPresentProblem> no_problems;
  EXPECT_EQ(result->problems->get_dns_resolver_present_problems(), no_problems);
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
}  // namespace ash
