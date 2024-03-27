// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/dns_resolver_present_routine.h"
#include "base/memory/values_equivalent.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_test_helper.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// The IP config path specified here must match the IP config path specified in
// NetworkStateTestHelper::ResetDevicesAndServices(), which itself is based on
// the IP config path used to set up IP configs in
// FakeShillManagerClient::SetupDefaultEnvironment().
constexpr char kIPConfigPath[] = "ipconfig_path";

const std::vector<std::string>& GetWellFormedDnsServers() {
  static const base::NoDestructor<std::vector<std::string>>
      wellFormedDnsServers{{"192.168.1.100", "192.168.1.101", "192.168.1.102"}};
  return *wellFormedDnsServers;
}

}  // namespace

class DnsResolverPresentRoutineTest : public NetworkDiagnosticsTestHelper {
 public:
  DnsResolverPresentRoutineTest() {
    dns_resolver_present_routine_ = std::make_unique<DnsResolverPresentRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
  }
  DnsResolverPresentRoutineTest(const DnsResolverPresentRoutineTest&) = delete;
  DnsResolverPresentRoutineTest& operator=(
      const DnsResolverPresentRoutineTest&) = delete;

  ~DnsResolverPresentRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::DnsResolverPresentProblem>& expected_problems,
      base::OnceClosure quit_closure,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_dns_resolver_present_problems());
    std::move(quit_closure).Run();
  }

  // Set up the name servers and change the IPConfigs for the WiFi device and
  // service by overwriting the initial IPConfigs that are set up in
  // FakeShillManagerClient::SetupDefaultEnvironment(). Attach name
  // servers to the IP config.
  void SetUpNameServers(const std::vector<std::string>& name_servers,
                        const std::string& type = shill::kTypeIPv4) {
    DCHECK(!wifi_path().empty());
    // Set up the name servers
    base::Value::List dns_servers;
    for (const std::string& name_server : name_servers) {
      dns_servers.Append(name_server);
    }

    // Set up the IP config
    auto ip_config_properties =
        base::Value::Dict()
            .Set(shill::kMethodProperty, type)
            .Set(shill::kNameServersProperty, dns_servers.Clone());
    helper()->ip_config_test()->AddIPConfig(kIPConfigPath,
                                            ip_config_properties.Clone());
    std::string wifi_device_path =
        helper()->device_test()->GetDevicePathForType(shill::kTypeWifi);
    helper()->device_test()->SetDeviceProperty(
        wifi_device_path, shill::kIPConfigsProperty,
        base::Value(std::move(ip_config_properties)),
        /*notify_changed=*/true);
    SetServiceProperty(wifi_path(), shill::kIPConfigProperty,
                       base::Value(kIPConfigPath));

    // Wait until the changed name servers have been notified (notification
    // triggered by call to SetDeviceProperty() above) and that the |wifi_path_|
    // has been set up.
    base::RunLoop().RunUntilIdle();
  }

  void RunRoutine(
      mojom::RoutineVerdict routine_verdict,
      const std::vector<mojom::DnsResolverPresentProblem>& expected_problems) {
    base::RunLoop run_loop;
    dns_resolver_present_routine_->RunRoutine(base::BindOnce(
        &DnsResolverPresentRoutineTest::CompareResult, weak_ptr(),
        routine_verdict, expected_problems, run_loop.QuitClosure()));
    run_loop.Run();
  }
  DnsResolverPresentRoutine* dns_resolver_present_routine() {
    return dns_resolver_present_routine_.get();
  }

 protected:
  base::WeakPtr<DnsResolverPresentRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<DnsResolverPresentRoutine> dns_resolver_present_routine_;
  std::unique_ptr<FakeDebugDaemonClient> debug_daemon_client_;
  base::WeakPtrFactory<DnsResolverPresentRoutineTest> weak_factory_{this};
};

TEST_F(DnsResolverPresentRoutineTest, TestValidNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers(GetWellFormedDnsServers());
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(DnsResolverPresentRoutineTest, TestValidIpv6NameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers({"2001:db8:3333:4444:5555:6666:7777:8888", "::1234:5678"},
                   shill::kTypeIPv6);
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(DnsResolverPresentRoutineTest, TestNoResolverPresent) {
  SetUpWiFi(shill::kStateOnline);
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kNoNameServersFound});
}

TEST_F(DnsResolverPresentRoutineTest, TestDefaultNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers({"0.0.0.0", "::/0"});
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kNoNameServersFound});
}

TEST_F(DnsResolverPresentRoutineTest, TestEmptyNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers({"", ""});
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kNoNameServersFound});
}

TEST_F(DnsResolverPresentRoutineTest, TestValidAndEmptyNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers({"192.168.1.100", ""});
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(DnsResolverPresentRoutineTest, TestMalformedNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers({"take.me.to.the.internets"});
  RunRoutine(mojom::RoutineVerdict::kProblem,
             {mojom::DnsResolverPresentProblem::kMalformedNameServers});
}

TEST_F(DnsResolverPresentRoutineTest, TestValidAndMalformedNameServers) {
  SetUpWiFi(shill::kStateOnline);
  SetUpNameServers({"192.168.1.100", "take.me.to.the.internets"});
  RunRoutine(mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(DnsResolverPresentRoutineTest, TestNoActiveNetwork) {
  SetUpWiFi(shill::kStateDisconnecting);
  SetUpNameServers(GetWellFormedDnsServers());
  RunRoutine(mojom::RoutineVerdict::kNotRun, {});
}

}  // namespace network_diagnostics
}  // namespace ash
