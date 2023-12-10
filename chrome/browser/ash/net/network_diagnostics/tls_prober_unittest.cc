// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/tls_prober.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_service.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_diagnostics {

namespace {

using ProbeExitEnum = TlsProber::ProbeExitEnum;

}  // namespace

class TlsProberWithFakeNetworkContextTest : public ::testing::Test {
 public:
  TlsProberWithFakeNetworkContextTest() = default;
  TlsProberWithFakeNetworkContextTest(
      const TlsProberWithFakeNetworkContextTest&) = delete;
  TlsProberWithFakeNetworkContextTest& operator=(
      const TlsProberWithFakeNetworkContextTest&) = delete;

  void InitializeProberNetworkContext(
      std::unique_ptr<FakeNetworkContext::DnsResult> fake_dns_result,
      std::optional<net::Error> tcp_connect_code,
      std::optional<net::Error> tls_upgrade_code) {
    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    fake_network_context_->set_fake_dns_result(std::move(fake_dns_result));
    fake_network_context_->SetTCPConnectCode(tcp_connect_code);
    fake_network_context_->SetTLSUpgradeCode(tls_upgrade_code);
  }

  void CreateAndExecuteTlsProber(net::HostPortPair host_port_pair,
                                 bool negotiate_tls,
                                 int expected_result,
                                 ProbeExitEnum expected_exit_enum) {
    base::test::TestFuture<int, ProbeExitEnum> future;
    tls_prober_ = std::make_unique<TlsProber>(
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          return fake_network_context();
        }),
        host_port_pair, negotiate_tls, future.GetCallback());
    auto [probe_result, probe_exit_enum] = future.Take();
    ASSERT_EQ(probe_result, expected_result);
    ASSERT_EQ(probe_exit_enum, expected_exit_enum);
  }

  FakeNetworkContext* fake_network_context() {
    return fake_network_context_.get();
  }

 protected:
  const net::HostPortPair kFakeTlsHostPortPair =
      net::HostPortPair::FromString("fake_hostname.com:443");
  const net::HostPortPair kFakeTcpHostPortPair =
      net::HostPortPair::FromString("fake_hostname.com:80");
  const net::IPEndPoint kFakeIPAddress{
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), /*port=*/1234)};

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  std::unique_ptr<TlsProber> tls_prober_;
};

TEST_F(TlsProberWithFakeNetworkContextTest,
       SocketConnectedAndUpgradedSuccessfully) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/std::nullopt);
  net::Error tcp_connect_code = net::OK;
  net::Error tls_upgrade_code = net::OK;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 tls_upgrade_code);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::OK, ProbeExitEnum::kSuccess);
}

TEST_F(TlsProberWithFakeNetworkContextTest, FailedDnsLookup) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt);
  // Neither TCP connect nor TLS upgrade should not be called in this scenario.
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*tcp_connect_code=*/std::nullopt,
                                 /*tls_upgrade_code=*/std::nullopt);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::ERR_NAME_NOT_RESOLVED,
                            ProbeExitEnum::kDnsFailure);
}

TEST_F(TlsProberWithFakeNetworkContextTest, MojoDisconnectDuringDnsLookup) {
  // Host resolution will not be successful due to Mojo disconnect. Neither TCP
  // connect nor TLS upgrade should not be called in this scenario.
  InitializeProberNetworkContext(/*fake_dns_result=*/{},
                                 /*tcp_connect_code=*/std::nullopt,
                                 /*tls_upgrade_code=*/std::nullopt);
  fake_network_context()->set_disconnect_during_host_resolution(true);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::ERR_FAILED, ProbeExitEnum::kDnsFailure);
}

TEST_F(TlsProberWithFakeNetworkContextTest, FailedTcpConnection) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/std::nullopt);
  net::Error tcp_connect_code = net::ERR_CONNECTION_FAILED;
  // TLS upgrade should not be called in this scenario.
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 /*tls_upgrade_code=*/std::nullopt);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::ERR_CONNECTION_FAILED,
                            ProbeExitEnum::kTcpConnectionFailure);
}

TEST_F(TlsProberWithFakeNetworkContextTest, FailedTlsUpgrade) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/std::nullopt);
  net::Error tcp_connect_code = net::OK;
  net::Error tls_upgrade_code = net::ERR_SSL_PROTOCOL_ERROR;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 tls_upgrade_code);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::ERR_SSL_PROTOCOL_ERROR,
                            ProbeExitEnum::kTlsUpgradeFailure);
}

TEST_F(TlsProberWithFakeNetworkContextTest,
       MojoDisconnectedDuringTcpConnectionAttempt) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/std::nullopt);
  // Since the TCP connection is disconnected, no connection codes are needed.
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*tcp_connect_code=*/std::nullopt,
                                 /*tls_upgrade_code=*/std::nullopt);
  fake_network_context()->set_disconnect_during_tcp_connection_attempt(true);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::ERR_FAILED,
                            ProbeExitEnum::kMojoDisconnectFailure);
}

TEST_F(TlsProberWithFakeNetworkContextTest,
       MojoDisconnectedDuringTlsUpgradeAttempt) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/std::nullopt);
  net::Error tcp_connect_code = net::OK;
  // TLS upgrade attempt will fail due to disconnection. |tls_upgrade_code|
  // is only populated to correctly initialize the FakeNetworkContext instance.
  net::Error tls_upgrade_code = net::OK;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 tls_upgrade_code);
  fake_network_context()->set_disconnect_during_tls_upgrade_attempt(true);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/true,
                            net::ERR_FAILED,
                            ProbeExitEnum::kMojoDisconnectFailure);
}

TEST_F(TlsProberWithFakeNetworkContextTest, SuccessfulTcpConnectOnly) {
  auto fake_dns_result = std::make_unique<FakeNetworkContext::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/std::nullopt);
  net::Error tcp_connect_code = net::OK;
  InitializeProberNetworkContext(std::move(fake_dns_result), tcp_connect_code,
                                 /*tls_upgrade_code=*/std::nullopt);
  CreateAndExecuteTlsProber(kFakeTlsHostPortPair, /*negotiate_tls=*/false,
                            net::OK, ProbeExitEnum::kSuccess);
}

class TlsProberWithRealNetworkContextTest : public ::testing::Test {
 public:
  TlsProberWithRealNetworkContextTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  TlsProberWithRealNetworkContextTest(
      const TlsProberWithRealNetworkContextTest&) = delete;
  TlsProberWithRealNetworkContextTest& operator=(
      const TlsProberWithRealNetworkContextTest&) = delete;

  void SetUp() override {
    service_ = network::NetworkService::CreateForTesting();
    service_->Bind(network_service_.BindNewPipeAndPassReceiver());
    auto context_params = network::mojom::NetworkContextParams::New();
    context_params->cert_verifier_params = content::GetCertVerifierParams(
        cert_verifier::mojom::CertVerifierCreationParams::New());
    network_service_->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::NO_CLIENT_CERT;
    test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &TlsProberWithRealNetworkContextTest::ReturnResponse));
    ASSERT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));
  }

  void CreateAndExecuteTlsProber(net::HostPortPair host_port_pair,
                                 bool negotiate_tls,
                                 int expected_result,
                                 ProbeExitEnum expected_exit_enum) {
    base::test::TestFuture<int, ProbeExitEnum> future;
    tls_prober_ = std::make_unique<TlsProber>(
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          return network_context_.get();
        }),
        host_port_pair, negotiate_tls, future.GetCallback());
    auto [probe_result, probe_exit_enum] = future.Take();
    ASSERT_EQ(probe_result, expected_result);
    ASSERT_EQ(probe_exit_enum, expected_exit_enum);
  }

  static std::unique_ptr<net::test_server::HttpResponse> ReturnResponse(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("");
    response->set_content_type("text/plain");
    return response;
  }

  // Returns the net::HostPortPair containing hostname (127.0.0.1) and a random
  // port used by the test server.
  const net::HostPortPair host_port_pair() const {
    return test_server_->host_port_pair();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkService> service_;
  mojo::Remote<network::mojom::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::unique_ptr<TlsProber> tls_prober_;
};

TEST_F(TlsProberWithRealNetworkContextTest,
       TestSuccessfulProbeUsingRealNetworkContext) {
  CreateAndExecuteTlsProber(host_port_pair(), /*negotiate_tls=*/true, net::OK,
                            ProbeExitEnum::kSuccess);
}

}  // namespace ash::network_diagnostics
