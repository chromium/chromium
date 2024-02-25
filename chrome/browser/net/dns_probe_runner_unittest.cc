// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_runner.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::BrowserTaskEnvironment;

namespace chrome_browser_net {

namespace {

class TestDnsProbeRunnerCallback {
 public:
  TestDnsProbeRunnerCallback() : called_(false) {}

  base::OnceClosure callback() {
    return base::BindOnce(&TestDnsProbeRunnerCallback::OnCalled,
                          base::Unretained(this));
  }
  bool called() const { return called_; }

 private:
  void OnCalled() {
    EXPECT_FALSE(called_);
    called_ = true;
  }

  bool called_;
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  explicit FakeNetworkContext(
      std::vector<FakeHostResolver::SingleResult> result_list)
      : result_list_(std::move(result_list)) {}

  void CreateHostResolver(
      const std::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    ASSERT_FALSE(resolver_);
    resolver_ = std::make_unique<FakeHostResolver>(std::move(receiver),
                                                   std::move(result_list_));
  }

 private:
  std::unique_ptr<FakeHostResolver> resolver_;
  std::vector<FakeHostResolver::SingleResult> result_list_;
};

class FirstHangingThenFakeResolverNetworkContext
    : public network::TestNetworkContext {
 public:
  explicit FirstHangingThenFakeResolverNetworkContext(
      std::vector<FakeHostResolver::SingleResult> result_list)
      : result_list_(std::move(result_list)) {}

  void CreateHostResolver(
      const std::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    if (call_num == 0) {
      resolver_ = std::make_unique<HangingHostResolver>(std::move(receiver));
    } else {
      resolver_ = std::make_unique<FakeHostResolver>(std::move(receiver),
                                                     std::move(result_list_));
    }
    call_num++;
  }

  void DestroyHostResolver() { resolver_ = nullptr; }

 private:
  int call_num = 0;
  std::unique_ptr<network::mojom::HostResolver> resolver_;
  std::vector<FakeHostResolver::SingleResult> result_list_;
};

class DnsProbeRunnerTest : public testing::Test {
 protected:
  void SetupTest(int query_result,
                 net::ResolveErrorInfo resolve_error_info,
                 FakeHostResolver::Response query_response);
  void SetupTest(std::vector<FakeHostResolver::SingleResult> result_list);
  void RunTest(DnsProbeRunner::Result expected_probe_results);

  network::mojom::NetworkContext* network_context() const {
    return network_context_.get();
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<DnsProbeRunner> runner_;
};

void DnsProbeRunnerTest::SetupTest(int query_result,
                                   net::ResolveErrorInfo resolve_error_info,
                                   FakeHostResolver::Response query_response) {
  SetupTest({{query_result, resolve_error_info, query_response}});
}

void DnsProbeRunnerTest::SetupTest(
    std::vector<FakeHostResolver::SingleResult> result_list) {
  network_context_ =
      std::make_unique<FakeNetworkContext>(std::move(result_list));
  runner_ = std::make_unique<DnsProbeRunner>(
      net::DnsConfigOverrides(),
      base::BindRepeating(&DnsProbeRunnerTest::network_context,
                          base::Unretained(this)));
}

void DnsProbeRunnerTest::RunTest(DnsProbeRunner::Result expected_probe_result) {
  TestDnsProbeRunnerCallback callback;
  runner_->RunProbe(callback.callback());
  EXPECT_TRUE(runner_->IsRunning());

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(expected_probe_result, runner_->result());
}

TEST_F(DnsProbeRunnerTest, Probe_OK) {
  SetupTest(net::OK, net::ResolveErrorInfo(net::OK),
            FakeHostResolver::kOneAddressResponse);
  RunTest(DnsProbeRunner::CORRECT);
}

TEST_F(DnsProbeRunnerTest, Probe_EMPTY) {
  SetupTest(net::OK, net::ResolveErrorInfo(net::OK),
            FakeHostResolver::kEmptyResponse);
  RunTest(DnsProbeRunner::INCORRECT);
}

TEST_F(DnsProbeRunnerTest, Probe_TIMEOUT) {
  SetupTest(net::ERR_NAME_NOT_RESOLVED,
            net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
            FakeHostResolver::kNoResponse);
  RunTest(DnsProbeRunner::UNREACHABLE);
}

TEST_F(DnsProbeRunnerTest, Probe_NXDOMAIN) {
  SetupTest(net::ERR_NAME_NOT_RESOLVED,
            net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
            FakeHostResolver::kNoResponse);
  RunTest(DnsProbeRunner::INCORRECT);
}

TEST_F(DnsProbeRunnerTest, Probe_FAILING) {
  SetupTest(net::ERR_NAME_NOT_RESOLVED,
            net::ResolveErrorInfo(net::ERR_DNS_SERVER_FAILED),
            FakeHostResolver::kNoResponse);
  RunTest(DnsProbeRunner::FAILING);
}

TEST_F(DnsProbeRunnerTest, Probe_DnsNotRun) {
  SetupTest(net::ERR_NAME_NOT_RESOLVED,
            net::ResolveErrorInfo(net::ERR_DNS_CACHE_MISS),
            FakeHostResolver::kNoResponse);
  RunTest(DnsProbeRunner::UNKNOWN);
}

TEST_F(DnsProbeRunnerTest, Probe_SecureDnsHostnameNotResolved) {
  SetupTest(net::ERR_NAME_NOT_RESOLVED,
            net::ResolveErrorInfo(
                net::ERR_DNS_SECURE_RESOLVER_HOSTNAME_RESOLUTION_FAILED),
            FakeHostResolver::kNoResponse);
  RunTest(DnsProbeRunner::UNREACHABLE);
}

TEST_F(DnsProbeRunnerTest, Probe_SecureDnsCertificateError) {
  SetupTest(net::ERR_NAME_NOT_RESOLVED,
            net::ResolveErrorInfo(net::ERR_CERT_COMMON_NAME_INVALID),
            FakeHostResolver::kNoResponse);
  RunTest(DnsProbeRunner::UNREACHABLE);
}

TEST_F(DnsProbeRunnerTest, TwoProbes) {
  SetupTest({{net::OK, net::ResolveErrorInfo(net::OK),
              FakeHostResolver::kOneAddressResponse},
             {net::ERR_NAME_NOT_RESOLVED,
              net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
              FakeHostResolver::kNoResponse}});
  RunTest(DnsProbeRunner::CORRECT);
  RunTest(DnsProbeRunner::INCORRECT);
}

TEST_F(DnsProbeRunnerTest, MojoConnectionError) {
  // Use a HostResolverGetter that returns a HangingHostResolver on the first
  // call, and a FakeHostResolver on the second call.
  FirstHangingThenFakeResolverNetworkContext network_context(
      {{net::OK, net::ResolveErrorInfo(net::OK),
        FakeHostResolver::kOneAddressResponse}});
  runner_ = std::make_unique<DnsProbeRunner>(
      net::DnsConfigOverrides(),
      base::BindRepeating(
          [](network::mojom::NetworkContext* context) { return context; },
          base::Unretained(&network_context)));

  TestDnsProbeRunnerCallback callback;
  runner_->RunProbe(callback.callback());
  EXPECT_TRUE(runner_->IsRunning());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(runner_->IsRunning());
  EXPECT_FALSE(callback.called());
  // Destroy the HangingHostResolver while runner_ is still waiting for a
  // response. The set_connection_error_handler callback should be invoked.
  network_context.DestroyHostResolver();
  RunLoop().RunUntilIdle();
  // That should cause the RunProbe callback to be called with an UNKNOWN
  // status since the HostResolver request never returned.
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(DnsProbeRunner::UNKNOWN, runner_->result());

  // Try another probe. The DnsProbeRunner should call the HostResolverGetter
  // again, this time getting a FakeHostResolver that will successfully return
  // an OK result.
  RunTest(DnsProbeRunner::CORRECT);
}

}  // namespace

}  // namespace chrome_browser_net
