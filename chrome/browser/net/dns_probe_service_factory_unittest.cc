// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "components/error_page/common/net_error_info.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::BrowserTaskEnvironment;
using error_page::DnsProbeStatus;

namespace chrome_browser_net {

namespace {

class DnsProbeServiceTest : public testing::Test {
 public:
  DnsProbeServiceTest()
      : callback_called_(false), callback_result_(error_page::DNS_PROBE_MAX) {}

  void Probe() {
    service_->ProbeDns(base::BindOnce(&DnsProbeServiceTest::ProbeCallback,
                                      base::Unretained(this)));
  }

  void Reset() { callback_called_ = false; }

 protected:
  network::mojom::NetworkContext* GetNetworkContext() {
    return network_context_.get();
  }

  mojo::Remote<network::mojom::DnsConfigChangeManager>
  GetDnsConfigChangeManager() {
    mojo::Remote<network::mojom::DnsConfigChangeManager>
        dns_config_change_manager_remote;
    dns_config_change_manager_ = std::make_unique<FakeDnsConfigChangeManager>(
        dns_config_change_manager_remote.BindNewPipeAndPassReceiver());
    return dns_config_change_manager_remote;
  }

  void ConfigureTest(
      std::vector<FakeHostResolver::SingleResult> system_results,
      std::vector<FakeHostResolver::SingleResult> public_results) {
    ASSERT_FALSE(network_context_);

    network_context_ = std::make_unique<FakeHostResolverNetworkContext>(
        std::move(system_results), std::move(public_results));

    service_ = DnsProbeServiceFactory::CreateForTesting(
        base::BindRepeating(&DnsProbeServiceTest::GetNetworkContext,
                            base::Unretained(this)),
        base::BindRepeating(&DnsProbeServiceTest::GetDnsConfigChangeManager,
                            base::Unretained(this)),
        &tick_clock_);
  }

  void RunTest(DnsProbeStatus expected_result) {
    Reset();

    Probe();
    RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called_);
    EXPECT_EQ(expected_result, callback_result_);
  }

  void AdvanceTime(base::TimeDelta delta) { tick_clock_.Advance(delta); }

  void SimulateDnsConfigChange() {
    dns_config_change_manager_->SimulateDnsConfigChange();
    RunLoop().RunUntilIdle();
  }

  void DestroyDnsConfigChangeManager() { dns_config_change_manager_ = nullptr; }

  bool has_dns_config_change_manager() const {
    return !!dns_config_change_manager_;
  }

 private:
  void ProbeCallback(DnsProbeStatus result) {
    EXPECT_FALSE(callback_called_);
    callback_called_ = true;
    callback_result_ = result;
  }

  base::SimpleTestTickClock tick_clock_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeHostResolverNetworkContext> network_context_;
  std::unique_ptr<FakeDnsConfigChangeManager> dns_config_change_manager_;
  std::unique_ptr<DnsProbeService> service_;
  bool callback_called_;
  DnsProbeStatus callback_result_;
};

TEST_F(DnsProbeServiceTest, Probe_OK_OK) {
  ConfigureTest({{net::OK, FakeHostResolver::kOneAddressResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Probe_TIMEOUT_OK) {
  ConfigureTest({{net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_TIMEOUT_TIMEOUT) {
  ConfigureTest({{net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}},
                {{net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, Probe_OK_FAIL) {
  ConfigureTest({{net::OK, FakeHostResolver::kOneAddressResponse}},
                {{net::ERR_NAME_NOT_RESOLVED, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_OK) {
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED, FakeHostResolver::kNoResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_FAIL) {
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED, FakeHostResolver::kNoResponse}},
                {{net::ERR_NAME_NOT_RESOLVED, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE);
}

TEST_F(DnsProbeServiceTest, Cache) {
  ConfigureTest({{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  // Advance clock, but not enough to expire the cache.
  AdvanceTime(base::TimeDelta::FromSeconds(4));
  // Cached NXDOMAIN result should persist, not the result from the new rules.
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Expire) {
  ConfigureTest({{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  // Advance clock enough to trigger cache expiration.
  AdvanceTime(base::TimeDelta::FromSeconds(6));
  // New rules should apply, since a new probe should be run.
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, DnsConfigChange) {
  ConfigureTest({{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  // Simulate dns config change notification.
  SimulateDnsConfigChange();
  // New rules should apply, since a new probe should be run.
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, MojoConnectionError) {
  ConfigureTest({{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}},
                {{net::OK, FakeHostResolver::kOneAddressResponse},
                 {net::ERR_DNS_TIMED_OUT, FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  DestroyDnsConfigChangeManager();
  RunLoop().RunUntilIdle();
  // DnsProbeService should have detected the mojo connection error and
  // automatically called the DnsConfigChangeManagerGetter again.
  ASSERT_TRUE(has_dns_config_change_manager());
  // New rules should apply, since recreating the DnsConfigChangeManagerClient
  // should also clear the cache (can't tell if a config change might have
  // happened while not getting notifications).
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

}  // namespace

}  // namespace chrome_browser_net
