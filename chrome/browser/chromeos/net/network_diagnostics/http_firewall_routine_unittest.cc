// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/http_firewall_routine.h"

#include <deque>
#include <memory>
#include <utility>

#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

// The number of hosts the the routine tries to open socket connections to (if
// DNS resolution is successful). Based on GetHostnamesToQuery() in
// http_firewall_routine.cc.
const int kTotalHosts = 9;
// Represents a fake port number of a fake ip address returned by the
// FakeHostResolver.
const int kFakePortNumber = 1234;
const char kFakeTestProfile[] = "test";

net::IPEndPoint FakeIPAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), kFakePortNumber);
}

class FakeHostResolver : public network::mojom::HostResolver {
 public:
  struct DnsResult {
    DnsResult(int32_t result,
              net::ResolveErrorInfo resolve_error_info,
              base::Optional<net::AddressList> resolved_addresses)
        : result(result),
          resolve_error_info(resolve_error_info),
          resolved_addresses(resolved_addresses) {}

    int result;
    net::ResolveErrorInfo resolve_error_info;
    base::Optional<net::AddressList> resolved_addresses;
  };

  FakeHostResolver(mojo::PendingReceiver<network::mojom::HostResolver> receiver,
                   std::deque<DnsResult*> fake_dns_results)
      : receiver_(this, std::move(receiver)),
        fake_dns_results_(std::move(fake_dns_results)) {}
  ~FakeHostResolver() override {}

  // network::mojom::HostResolver
  void ResolveHost(const net::HostPortPair& host,
                   const net::NetworkIsolationKey& network_isolation_key,
                   network::mojom::ResolveHostParametersPtr optional_parameters,
                   mojo::PendingRemote<network::mojom::ResolveHostClient>
                       pending_response_client) override {
    mojo::Remote<network::mojom::ResolveHostClient> response_client(
        std::move(pending_response_client));
    DnsResult* result = fake_dns_results_.front();
    DCHECK(result);
    fake_dns_results_.pop_front();
    response_client->OnComplete(result->result, result->resolve_error_info,
                                result->resolved_addresses);
  }
  void MdnsListen(
      const net::HostPortPair& host,
      net::DnsQueryType query_type,
      mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override {
    NOTREACHED();
  }

 private:
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  // Use the list of fake dns results to fake different responses for multiple
  // calls to the host_resolver's ResolveHost().
  std::deque<DnsResult*> fake_dns_results_;
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;

  explicit FakeNetworkContext(
      std::deque<FakeHostResolver::DnsResult*> fake_dns_results)
      : fake_dns_results_(std::move(fake_dns_results)) {}

  ~FakeNetworkContext() override {}

  // network::TestNetworkContext:
  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    ASSERT_FALSE(resolver_);
    resolver_ = std::make_unique<FakeHostResolver>(
        std::move(receiver), std::move(fake_dns_results_));
  }

 private:
  std::unique_ptr<FakeHostResolver> resolver_;
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results_;
};

class MockTCPSocket : public net::MockTCPClientSocket {
 public:
  explicit MockTCPSocket(net::SocketDataProvider* socket_data_provider)
      : net::MockTCPClientSocket(net::AddressList(),
                                 nullptr,
                                 socket_data_provider) {}
  MockTCPSocket(const MockTCPSocket&) = delete;
  MockTCPSocket& operator=(const MockTCPSocket&) = delete;

  int Connect(net::CompletionOnceCallback callback) override {
    return net::MockTCPClientSocket::Connect(std::move(callback));
  }
};

class FakeClientSocketFactory : public net::ClientSocketFactory {
 public:
  FakeClientSocketFactory(
      std::deque<net::SocketDataProvider*> fake_socket_data_providers)
      : socket_data_providers_(fake_socket_data_providers) {}
  FakeClientSocketFactory(const FakeClientSocketFactory&) = delete;
  FakeClientSocketFactory& operator=(const FakeClientSocketFactory&) = delete;

  std::unique_ptr<net::DatagramClientSocket> CreateDatagramClientSocket(
      net::DatagramSocket::BindType bind_type,
      net::NetLog* net_log,
      const net::NetLogSource& source) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<net::TransportClientSocket> CreateTransportClientSocket(
      const net::AddressList& addresses,
      std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
      net::NetworkQualityEstimator* network_quality_estimator,
      net::NetLog* net_log,
      const net::NetLogSource& source) override {
    net::SocketDataProvider* socket_data_provider =
        socket_data_providers_.front();
    socket_data_providers_.pop_front();
    return std::make_unique<MockTCPSocket>(socket_data_provider);
  }

  std::unique_ptr<net::SSLClientSocket> CreateSSLClientSocket(
      net::SSLClientContext* context,
      std::unique_ptr<net::StreamSocket> stream_socket,
      const net::HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<net::ProxyClientSocket> CreateProxyClientSocket(
      std::unique_ptr<net::StreamSocket> stream_socket,
      const std::string& user_agent,
      const net::HostPortPair& endpoint,
      const net::ProxyServer& proxy_server,
      net::HttpAuthController* http_auth_controller,
      bool tunnel,
      bool using_spdy,
      net::NextProto negotiated_protocol,
      net::ProxyDelegate* proxy_delegate,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

 private:
  std::deque<net::SocketDataProvider*> socket_data_providers_;
};

}  // namespace

class HttpFirewallRoutineTest : public ::testing::Test {
 public:
  HttpFirewallRoutineTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    session_manager::SessionManager::Get()->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }
  HttpFirewallRoutineTest(const HttpFirewallRoutineTest&) = delete;
  HttpFirewallRoutineTest& operator=(const HttpFirewallRoutineTest&) = delete;

  void RunRoutine(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpFirewallProblem>& expected_problems) {
    http_firewall_routine_->RunRoutine(base::BindOnce(
        &HttpFirewallRoutineTest::CompareVerdict, weak_factory_.GetWeakPtr(),
        expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }

  void CompareVerdict(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::HttpFirewallProblem>& expected_problems,
      mojom::RoutineVerdict actual_verdict,
      const std::vector<mojom::HttpFirewallProblem>& actual_problems) {
    DCHECK(run_loop_.running());
    EXPECT_EQ(expected_verdict, actual_verdict);
    EXPECT_EQ(expected_problems, actual_problems);
    run_loop_.Quit();
  }

  void SetUpFakeProperties(
      std::deque<FakeHostResolver::DnsResult*> fake_dns_results,
      std::deque<net::SocketDataProvider*> fake_socket_data_providers) {
    ASSERT_TRUE(profile_manager_.SetUp());

    fake_network_context_ =
        std::make_unique<FakeNetworkContext>(std::move(fake_dns_results));
    test_profile_ = profile_manager_.CreateTestingProfile(kFakeTestProfile);
    fake_client_socket_factory_ = std::make_unique<FakeClientSocketFactory>(
        std::move(fake_socket_data_providers));
  }

  void SetUpHttpFirewallRoutine() {
    http_firewall_routine_ = std::make_unique<HttpFirewallRoutine>();
    http_firewall_routine_->SetNetworkContextForTesting(
        fake_network_context_.get());
    http_firewall_routine_->SetProfileForTesting(test_profile_);
    http_firewall_routine_->set_client_socket_factory_for_testing(
        fake_client_socket_factory_.get());
  }

  // Sets up required properties (via fakes) and runs the test.
  //
  // Parameters:
  // |fake_dns_results|: Represents the results of a one or more DNS
  // resolutions.
  // |fake_socket_data_providers|: Represents the socket data provider for one
  // or more TCP sockets.
  // |expected_routine_verdict|: Represents the expected verdict
  // reported by this test.
  // |expected_problems|: Represents the expected problem
  // reported by this test.
  void SetUpAndRunRoutine(
      std::deque<FakeHostResolver::DnsResult*> fake_dns_results,
      std::deque<net::SocketDataProvider*> fake_socket_data_providers,
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::HttpFirewallProblem>& expected_problems) {
    SetUpFakeProperties(std::move(fake_dns_results),
                        std::move(fake_socket_data_providers));
    SetUpHttpFirewallRoutine();
    RunRoutine(expected_routine_verdict, expected_problems);
  }

  FakeClientSocketFactory* fake_client_socket_factory() {
    return fake_client_socket_factory_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  std::unique_ptr<FakeClientSocketFactory> fake_client_socket_factory_;
  // Unowned
  Profile* test_profile_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<HttpFirewallRoutine> http_firewall_routine_;
  base::WeakPtrFactory<HttpFirewallRoutineTest> weak_factory_{this};
};

TEST_F(HttpFirewallRoutineTest, TestDnsResolutionFailuresAboveThreshold) {
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::deque<net::SocketDataProvider*> fake_socket_data_providers;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;
  std::vector<std::unique_ptr<net::SocketDataProvider>> providers;

  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    std::unique_ptr<FakeHostResolver::DnsResult> resolution;
    if (i < 2) {
      resolution = std::make_unique<FakeHostResolver::DnsResult>(
          net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
          net::AddressList());
    } else {
      // Having seven successful resolutions out of nine puts us below the
      // threshold needed to attempt socket connections.
      resolution = std::make_unique<FakeHostResolver::DnsResult>(
          net::OK, net::ResolveErrorInfo(net::OK),
          net::AddressList(FakeIPAddress()));
    }
    fake_dns_results.push_back(resolution.get());
    resolutions.emplace_back(std::move(resolution));

    auto socket_data_provider = std::make_unique<net::SequencedSocketData>();
    socket_data_provider->set_connect_data(
        net::MockConnect(net::IoMode::ASYNC, net::ERR_FAILED));
    fake_socket_data_providers.push_back(socket_data_provider.get());
    providers.emplace_back(std::move(socket_data_provider));
  }
  SetUpAndRunRoutine(
      std::move(fake_dns_results), std::move(fake_socket_data_providers),
      mojom::RoutineVerdict::kProblem,
      {mojom::HttpFirewallProblem::kDnsResolutionFailuresAboveThreshold});
}

TEST_F(HttpFirewallRoutineTest, TestFirewallDetection) {
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::deque<net::SocketDataProvider*> fake_socket_data_providers;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;
  std::vector<std::unique_ptr<net::SocketDataProvider>> providers;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    auto successful_resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()));
    fake_dns_results.push_back(successful_resolution.get());
    resolutions.emplace_back(std::move(successful_resolution));

    auto socket_data_provider = std::make_unique<net::SequencedSocketData>();
    // Firewall detection requires all connect attempts to fail.
    socket_data_provider->set_connect_data(
        net::MockConnect(net::IoMode::ASYNC, net::ERR_FAILED));
    fake_socket_data_providers.push_back(socket_data_provider.get());
    providers.emplace_back(std::move(socket_data_provider));
  }
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     std::move(fake_socket_data_providers),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::HttpFirewallProblem::kFirewallDetected});
}

TEST_F(HttpFirewallRoutineTest, TestPotentialFirewallDetection) {
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::deque<net::SocketDataProvider*> fake_socket_data_providers;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;
  std::vector<std::unique_ptr<net::SocketDataProvider>> providers;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    auto successful_resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()));
    fake_dns_results.push_back(successful_resolution.get());
    resolutions.emplace_back(std::move(successful_resolution));

    auto socket_data_provider = std::make_unique<net::SequencedSocketData>();
    if (i < 5) {
      socket_data_provider->set_connect_data(
          net::MockConnect(net::IoMode::ASYNC, net::OK));
    } else {
      // Having five connection failures and four successful connections signals
      // a potential firewall.
      socket_data_provider->set_connect_data(
          net::MockConnect(net::IoMode::ASYNC, net::ERR_FAILED));
    }
    fake_socket_data_providers.push_back(socket_data_provider.get());
    providers.emplace_back(std::move(socket_data_provider));
  }
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     std::move(fake_socket_data_providers),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::HttpFirewallProblem::kPotentialFirewall});
}

TEST_F(HttpFirewallRoutineTest, TestNoFirewallIssues) {
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::deque<net::SocketDataProvider*> fake_socket_data_providers;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;
  std::vector<std::unique_ptr<net::SocketDataProvider>> providers;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    auto successful_resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()));
    fake_dns_results.push_back(successful_resolution.get());
    resolutions.emplace_back(std::move(successful_resolution));

    auto socket_data_provider = std::make_unique<net::SequencedSocketData>();
    if (i < 8) {
      socket_data_provider->set_connect_data(
          net::MockConnect(net::IoMode::ASYNC, net::OK));
    } else {
      // Having one connection failure and eight successful connections puts us
      // above the required threshold.
      socket_data_provider->set_connect_data(
          net::MockConnect(net::IoMode::ASYNC, net::ERR_FAILED));
    }
    fake_socket_data_providers.push_back(socket_data_provider.get());
    providers.emplace_back(std::move(socket_data_provider));
  }
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     std::move(fake_socket_data_providers),
                     mojom::RoutineVerdict::kNoProblem, {});
}

TEST_F(HttpFirewallRoutineTest, TestContinousRetries) {
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results;
  std::deque<net::SocketDataProvider*> fake_socket_data_providers;
  std::vector<std::unique_ptr<FakeHostResolver::DnsResult>> resolutions;
  std::vector<std::unique_ptr<net::SocketDataProvider>> providers;
  // kTotalHosts = 9
  for (int i = 0; i < kTotalHosts; i++) {
    auto successful_resolution = std::make_unique<FakeHostResolver::DnsResult>(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(FakeIPAddress()));
    fake_dns_results.push_back(successful_resolution.get());
    resolutions.emplace_back(std::move(successful_resolution));

    auto socket_data_provider = std::make_unique<net::SequencedSocketData>();
    if (i < 8) {
      socket_data_provider->set_connect_data(
          net::MockConnect(net::IoMode::ASYNC, net::OK));
    } else {
      // Having one socket that continuously retries until failure and eight
      // sockets that make successful connections puts us above the required
      // threshold.
      socket_data_provider->set_connect_data(
          net::MockConnect(net::IoMode::ASYNC, net::ERR_TIMED_OUT));
    }
    fake_socket_data_providers.push_back(socket_data_provider.get());
    providers.emplace_back(std::move(socket_data_provider));
  }
  SetUpAndRunRoutine(std::move(fake_dns_results),
                     std::move(fake_socket_data_providers),
                     mojom::RoutineVerdict::kNoProblem, {});
}

// TODO(khegde): Eventually add unit tests that includes more scenarios with
// retries. This is tough to mock out because each MockTCPClient is initialized
// with a SocketDataProvider*, which cannot be modified during the lifetime of
// the socket. SocketDataProvider* sets the MockConnection that fakes the result
// of the socket's connection attempt. As a result, the same socket cannot be
// used to fake a retry failure, followed by either a successful or unsuccessful
// connection attempt.

}  // namespace network_diagnostics
}  // namespace chromeos
