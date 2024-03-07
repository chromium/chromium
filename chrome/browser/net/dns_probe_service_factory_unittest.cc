// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_service_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/error_page/common/net_error_info.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/secure_dns_mode.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using content::BrowserTaskEnvironment;
using error_page::DnsProbeStatus;

namespace chrome_browser_net {

namespace {

class DnsProbeServiceTest : public testing::Test {
 public:
  DnsProbeServiceTest()
      : callback_called_(false), callback_result_(error_page::DNS_PROBE_MAX) {
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());

    // SystemNetworkContextManager cannot be instantiated here, which normally
    // owns the StubResolverConfigReader instance, so inject a
    // StubResolverConfigReader instance here.
    stub_resolver_config_reader_ =
        std::make_unique<StubResolverConfigReader>(local_state_->Get());
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        stub_resolver_config_reader_.get());
  }

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
      std::vector<FakeHostResolver::SingleResult> current_config_results,
      std::vector<FakeHostResolver::SingleResult> google_config_results) {
    ASSERT_FALSE(network_context_);

    network_context_ = std::make_unique<FakeHostResolverNetworkContext>(
        std::move(current_config_results), std::move(google_config_results));

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

  DnsProbeService* probe_service() const { return service_.get(); }

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  const std::string kDohTemplateGet = "https://bar.test/dns-query{?dns}";
  const std::string kDohTemplatePost = "https://bar.test/dns-query";

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
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<StubResolverConfigReader> stub_resolver_config_reader_;
  bool callback_called_;
  DnsProbeStatus callback_result_;
};

TEST_F(DnsProbeServiceTest, Probe_OK_OK) {
  ConfigureTest({{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Probe_TIMEOUT_OK) {
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_TIMEOUT_TIMEOUT) {
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}},
                {{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, Probe_OK_FAIL) {
  ConfigureTest({{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}},
                {{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
                  FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_OK) {
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_OK_automatic) {
  // Set the DoH prefs using the managed pref store to prevent the mode from
  // being downgraded to off if the test environment is managed.
  local_state()->SetManagedPref(
      prefs::kDnsOverHttpsMode,
      std::make_unique<base::Value>(SecureDnsConfig::kModeAutomatic));
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_BAD_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_OK_secure) {
  // Set the DoH prefs using the managed pref store to prevent the mode from
  // being downgraded to off if the test environment is managed.
  local_state()->SetManagedPref(
      prefs::kDnsOverHttpsMode,
      std::make_unique<base::Value>(SecureDnsConfig::kModeSecure));
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(
                      net::ERR_DNS_SECURE_RESOLVER_HOSTNAME_RESOLUTION_FAILED),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_BAD_SECURE_CONFIG);
}

TEST_F(DnsProbeServiceTest, Probe_FAIL_FAIL) {
  ConfigureTest({{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
                  FakeHostResolver::kNoResponse}},
                {{net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
                  FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE);
}

TEST_F(DnsProbeServiceTest, Cache) {
  ConfigureTest({{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  // Advance clock, but not enough to expire the cache.
  AdvanceTime(base::Seconds(4));
  // Cached NXDOMAIN result should persist, not the result from the new rules.
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
}

TEST_F(DnsProbeServiceTest, Expire) {
  ConfigureTest({{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  // Advance clock enough to trigger cache expiration.
  AdvanceTime(base::Seconds(6));
  // New rules should apply, since a new probe should be run.
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, DnsConfigChange) {
  ConfigureTest({{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}});
  RunTest(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  // Simulate dns config change notification.
  SimulateDnsConfigChange();
  // New rules should apply, since a new probe should be run.
  RunTest(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
}

TEST_F(DnsProbeServiceTest, MojoConnectionError) {
  ConfigureTest({{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}},
                {{net::OK, net::ResolveErrorInfo(net::OK),
                  FakeHostResolver::kOneAddressResponse},
                 {net::ERR_NAME_NOT_RESOLVED,
                  net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                  FakeHostResolver::kNoResponse}});
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

TEST_F(DnsProbeServiceTest, CurrentConfig_Automatic) {
  // Set the DoH prefs using the managed pref store to prevent the mode from
  // being downgraded to off if the test environment is managed.
  local_state()->SetManagedPref(
      prefs::kDnsOverHttpsMode,
      std::make_unique<base::Value>(SecureDnsConfig::kModeAutomatic));
  local_state()->SetManagedPref(
      prefs::kDnsOverHttpsTemplates,
      std::make_unique<base::Value>(kDohTemplateGet + " " + kDohTemplatePost));
  ConfigureTest({}, {});
  net::DnsConfigOverrides overrides =
      probe_service()->GetCurrentConfigOverridesForTesting();

  EXPECT_TRUE(overrides.search.has_value());
  EXPECT_EQ(0u, overrides.search->size());
  EXPECT_TRUE(overrides.attempts.has_value());
  EXPECT_EQ(1, overrides.attempts.value());

  EXPECT_TRUE(overrides.secure_dns_mode.has_value());
  EXPECT_EQ(net::SecureDnsMode::kOff, overrides.secure_dns_mode.value());
  EXPECT_FALSE(overrides.dns_over_https_config.has_value());
}

TEST_F(DnsProbeServiceTest, CurrentConfig_Secure) {
  // Set the DoH prefs using the managed pref store to prevent the mode from
  // being downgraded to off if the test environment is managed.
  local_state()->SetManagedPref(
      prefs::kDnsOverHttpsMode,
      std::make_unique<base::Value>(SecureDnsConfig::kModeSecure));
  local_state()->SetManagedPref(
      prefs::kDnsOverHttpsTemplates,
      std::make_unique<base::Value>(kDohTemplateGet + " " + kDohTemplatePost));

#if BUILDFLAG(IS_CHROMEOS)
  // In a real user session, the pref
  // prefs::kDnsOverHttpsEffectiveTemplatesChromeOS is set by
  // ash::SecureDnsManager.
  local_state()->SetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS,
                           kDohTemplateGet + " " + kDohTemplatePost);
#endif
  ConfigureTest({}, {});
  net::DnsConfigOverrides overrides =
      probe_service()->GetCurrentConfigOverridesForTesting();

  EXPECT_TRUE(overrides.search.has_value());
  EXPECT_EQ(0u, overrides.search->size());
  EXPECT_TRUE(overrides.attempts.has_value());
  EXPECT_EQ(1, overrides.attempts.value());

  EXPECT_THAT(overrides.secure_dns_mode,
              testing::Optional(net::SecureDnsMode::kSecure));
  EXPECT_THAT(
      overrides.dns_over_https_config,
      testing::Optional(*net::DnsOverHttpsConfig::FromTemplatesForTesting(
          {kDohTemplateGet, kDohTemplatePost})));
}

}  // namespace

}  // namespace chrome_browser_net
