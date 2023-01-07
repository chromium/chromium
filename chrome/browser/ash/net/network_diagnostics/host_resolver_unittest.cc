// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/host_resolver.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_host_resolver.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/resolve_error_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace network_diagnostics {

class HostResolverTest : public ::testing::Test {
 public:
  HostResolverTest() = default;
  HostResolverTest(const HostResolverTest&) = delete;
  HostResolverTest& operator=(const HostResolverTest&) = delete;

  void InitializeNetworkContext(
      std::unique_ptr<FakeHostResolver::DnsResult> fake_dns_result) {
    fake_network_context_.set_fake_dns_result(std::move(fake_dns_result));
  }

  FakeNetworkContext* fake_network_context() { return &fake_network_context_; }

 protected:
  const net::HostPortPair kFakeHostPortPair =
      net::HostPortPair::FromString("fake_stun_server.com:80");
  const net::IPEndPoint kFakeIPAddress{
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), /*port=*/1234)};
  std::unique_ptr<HostResolver> host_resolver_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeNetworkContext fake_network_context_;
};

TEST_F(HostResolverTest, TestSuccessfulResolution) {
  auto address_list = net::AddressList(kFakeIPAddress);
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), address_list,
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeNetworkContext(std::move(fake_dns_result));
  HostResolver::ResolutionResult resolution_result{
      net::ERR_FAILED, net::ResolveErrorInfo(net::OK),
      /*resolved_addresses=*/absl::nullopt,
      /*endpoint_results_with_metadata=*/absl::nullopt};
  base::RunLoop run_loop;
  host_resolver_ = std::make_unique<HostResolver>(
      kFakeHostPortPair, fake_network_context(),
      base::BindOnce(
          [](HostResolver::ResolutionResult* resolution_result,
             base::OnceClosure quit_closure,
             HostResolver::ResolutionResult& res_result) {
            resolution_result->result = res_result.result;
            resolution_result->resolve_error_info =
                res_result.resolve_error_info;
            resolution_result->resolved_addresses =
                res_result.resolved_addresses;
            resolution_result->endpoint_results_with_metadata =
                res_result.endpoint_results_with_metadata;
            std::move(quit_closure).Run();
          },
          &resolution_result, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(resolution_result.result, net::OK);
  EXPECT_EQ(resolution_result.resolve_error_info,
            net::ResolveErrorInfo(net::OK));
  EXPECT_EQ(resolution_result.resolved_addresses.value().size(), 1u);
  EXPECT_EQ(resolution_result.resolved_addresses.value().front(),
            address_list.front());
}

TEST_F(HostResolverTest, TestFailedHostResolution) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
      /*resolved_addresses=*/absl::nullopt,
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeNetworkContext(std::move(fake_dns_result));
  HostResolver::ResolutionResult resolution_result{
      net::ERR_FAILED, net::ResolveErrorInfo(net::OK),
      /*resolved_addresses=*/absl::nullopt,
      /*endpoint_results_with_metadata=*/absl::nullopt};
  base::RunLoop run_loop;
  host_resolver_ = std::make_unique<HostResolver>(
      kFakeHostPortPair, fake_network_context(),
      base::BindOnce(
          [](HostResolver::ResolutionResult* resolution_result,
             base::OnceClosure quit_closure,
             HostResolver::ResolutionResult& res_result) {
            resolution_result->result = res_result.result;
            resolution_result->resolve_error_info =
                res_result.resolve_error_info;
            resolution_result->resolved_addresses =
                res_result.resolved_addresses;
            resolution_result->endpoint_results_with_metadata =
                res_result.endpoint_results_with_metadata;
            std::move(quit_closure).Run();
          },
          &resolution_result, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(resolution_result.result, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(resolution_result.resolve_error_info,
            net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED));
  ASSERT_FALSE(resolution_result.resolved_addresses.has_value());
}

TEST_F(HostResolverTest, TestMojoDisconnectDuringHostResolution) {
  InitializeNetworkContext(/*fake_dns_result=*/{});
  fake_network_context()->set_disconnect_during_host_resolution(true);
  HostResolver::ResolutionResult resolution_result{
      net::ERR_FAILED, net::ResolveErrorInfo(net::OK),
      /*resolved_addresses=*/absl::nullopt,
      /*endpoint_results_with_metadata=*/absl::nullopt};
  base::RunLoop run_loop;
  host_resolver_ = std::make_unique<HostResolver>(
      kFakeHostPortPair, fake_network_context(),
      base::BindOnce(
          [](HostResolver::ResolutionResult* resolution_result,
             base::OnceClosure quit_closure,
             HostResolver::ResolutionResult& res_result) {
            resolution_result->result = res_result.result;
            resolution_result->resolve_error_info =
                res_result.resolve_error_info;
            resolution_result->resolved_addresses =
                res_result.resolved_addresses;
            resolution_result->endpoint_results_with_metadata =
                res_result.endpoint_results_with_metadata;
            std::move(quit_closure).Run();
          },
          &resolution_result, run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(resolution_result.result, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(resolution_result.resolve_error_info,
            net::ResolveErrorInfo(net::ERR_FAILED));
  ASSERT_FALSE(resolution_result.resolved_addresses.has_value());
}

}  // namespace network_diagnostics
}  // namespace ash
