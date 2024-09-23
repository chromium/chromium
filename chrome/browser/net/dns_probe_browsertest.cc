// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/enterprise_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_probe_service_factory.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/pref_names.h"
#include "components/error_page/common/net_error_info.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

using base::BindOnce;
using base::BindRepeating;
using base::FilePath;
using base::Unretained;
using content::BrowserThread;
using content::WebContents;
using error_page::DnsProbeStatus;
using net::URLRequestFailedJob;
using ui_test_utils::NavigateToURL;

namespace chrome_browser_net {

namespace {

// Wraps DnsProbeService and delays probes until someone calls
// StartDelayedProbes.  This allows the DnsProbeBrowserTest to enforce a
// stricter ordering of events.
class DelayingDnsProbeService : public DnsProbeService {
 public:
  DelayingDnsProbeService(
      const network::NetworkContextGetter& network_context_getter,
      const DnsProbeServiceFactory::DnsConfigChangeManagerGetter&
          dns_config_change_manager_getter)
      : dns_probe_service_impl_(DnsProbeServiceFactory::CreateForTesting(
            network_context_getter,
            dns_config_change_manager_getter,
            base::DefaultTickClock::GetInstance())) {}

  ~DelayingDnsProbeService() override { EXPECT_TRUE(delayed_probes_.empty()); }

  static std::unique_ptr<KeyedService> Create(
      const network::NetworkContextGetter& network_context_getter,
      const DnsProbeServiceFactory::DnsConfigChangeManagerGetter&
          dns_config_change_manager_getter,
      content::BrowserContext* context) {
    return std::make_unique<DelayingDnsProbeService>(
        network_context_getter, dns_config_change_manager_getter);
  }

  void ProbeDns(ProbeCallback callback) override {
    delayed_probes_.push_back(std::move(callback));
  }

  net::DnsConfigOverrides GetCurrentConfigOverridesForTesting() override {
    return dns_probe_service_impl_->GetCurrentConfigOverridesForTesting();
  }

  void StartDelayedProbes() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    std::vector<ProbeCallback> probes;
    probes.swap(delayed_probes_);

    for (std::vector<ProbeCallback>::iterator i = probes.begin();
         i != probes.end(); ++i) {
      dns_probe_service_impl_->ProbeDns(std::move(*i));
    }
  }

  int delayed_probe_count() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return delayed_probes_.size();
  }

 private:
  std::unique_ptr<DnsProbeService> dns_probe_service_impl_;
  std::vector<ProbeCallback> delayed_probes_;
};

class DnsProbeBrowserTest : public InProcessBrowserTest {
 public:
  DnsProbeBrowserTest();
  ~DnsProbeBrowserTest() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  bool InterceptURLLoaderRequest(
      content::URLLoaderInterceptor::RequestParams* params);

  // Sets the browser object that other methods apply to, and that has the
  // DnsProbeStatus messages of its currently active tab monitored.
  void SetActiveBrowser(Browser* browser);

  // Sets the results the FakeHostResolver will return for the current config
  // and Google config DnsProbeRunners. Since this mocks out the NetworkContext
  // and HostResolver used by the DnsProbeService it doesn't really give an
  // end-to-end test, but content::TestHostResolver mocks don't affect the
  // probes since they use HostResolverSource::DNS, so this is the best that
  // can be done currently.
  void SetFakeHostResolverResults(
      std::vector<FakeHostResolver::SingleResult> current_config_results,
      std::vector<FakeHostResolver::SingleResult> google_config_results);

  void NavigateToDnsError();
  void NavigateToOtherError();

  void StartDelayedProbes(int expected_delayed_probe_count);
  DnsProbeStatus WaitForSentStatus();
  int pending_status_count() const { return dns_probe_status_queue_.size(); }

  std::string Title();
  bool PageContains(const std::string& expected);

  // Checks that the error page is being displayed with the specified status
  // text.  The status text should be either a network error or DNS probe
  // status.
  void ExpectDisplayingErrorPage(const std::string& status_text);

 private:
  void OnDnsProbeStatusSent(DnsProbeStatus dns_probe_status);

  network::mojom::NetworkContext* GetNetworkContext() {
    return network_context_.get();
  }

  mojo::Remote<network::mojom::DnsConfigChangeManager>
  GetDnsConfigChangeManager();

  std::unique_ptr<FakeHostResolverNetworkContext> network_context_;
  std::unique_ptr<FakeDnsConfigChangeManager> dns_config_change_manager_;
  raw_ptr<DelayingDnsProbeService, AcrossTasksDanglingUntriaged>
      delaying_dns_probe_service_;

  // Browser that methods apply to.
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> active_browser_;
  // Helper that current has its DnsProbeStatus messages monitored.
  raw_ptr<NetErrorTabHelper, AcrossTasksDanglingUntriaged>
      monitored_tab_helper_;

  std::unique_ptr<base::RunLoop> awaiting_dns_probe_status_run_loop_;
  // Queue of statuses received but not yet consumed by WaitForSentStatus().
  std::list<DnsProbeStatus> dns_probe_status_queue_;

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

DnsProbeBrowserTest::DnsProbeBrowserTest()
    : active_browser_(nullptr), monitored_tab_helper_(nullptr) {}

DnsProbeBrowserTest::~DnsProbeBrowserTest() {
  // No tests should have any unconsumed probe statuses.
  EXPECT_EQ(0, pending_status_count());
}

void DnsProbeBrowserTest::SetUpOnMainThread() {
  NetErrorTabHelper::set_state_for_testing(NetErrorTabHelper::TESTING_DEFAULT);

  browser()->profile()->GetPrefs()->SetBoolean(
      embedder_support::kAlternateErrorPagesEnabled, true);

  ASSERT_TRUE(embedded_test_server()->Start());

  url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindRepeating(&DnsProbeBrowserTest::InterceptURLLoaderRequest,
                          base::Unretained(this)));

  SetActiveBrowser(browser());
}

void DnsProbeBrowserTest::TearDownOnMainThread() {
  url_loader_interceptor_.reset();

  NetErrorTabHelper::set_state_for_testing(NetErrorTabHelper::TESTING_DEFAULT);
}

bool DnsProbeBrowserTest::InterceptURLLoaderRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  if (params->url_request.url.spec() == "http://mock.http/title2.html") {
    content::URLLoaderInterceptor::WriteResponse("chrome/test/data/title2.html",
                                                 params->client.get());
    return true;
  }

  // Just returning false is enough to respond to http(s)://mock.failed.request
  // requests, which are the only requests that come in besides the LinkDoctor
  // requests.
  return false;
}

void DnsProbeBrowserTest::SetActiveBrowser(Browser* browser) {
  delaying_dns_probe_service_ = static_cast<DelayingDnsProbeService*>(
      DnsProbeServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser->profile(),
          base::BindRepeating(
              &DelayingDnsProbeService::Create,
              base::BindRepeating(&DnsProbeBrowserTest::GetNetworkContext,
                                  base::Unretained(this)),
              base::BindRepeating(
                  &DnsProbeBrowserTest::GetDnsConfigChangeManager,
                  base::Unretained(this)))));
  // If currently watching a NetErrorTabHelper, stop doing so before start
  // watching another.
  if (monitored_tab_helper_) {
    monitored_tab_helper_->set_dns_probe_status_snoop_callback_for_testing(
        NetErrorTabHelper::DnsProbeStatusSnoopCallback());
  }
  active_browser_ = browser;
  monitored_tab_helper_ = NetErrorTabHelper::FromWebContents(
      active_browser_->tab_strip_model()->GetActiveWebContents());
  monitored_tab_helper_->set_dns_probe_status_snoop_callback_for_testing(
      BindRepeating(&DnsProbeBrowserTest::OnDnsProbeStatusSent,
                    Unretained(this)));
}

void DnsProbeBrowserTest::SetFakeHostResolverResults(
    std::vector<FakeHostResolver::SingleResult> current_config_results,
    std::vector<FakeHostResolver::SingleResult> google_config_results) {
  ASSERT_FALSE(network_context_);

  network_context_ = std::make_unique<FakeHostResolverNetworkContext>(
      std::move(current_config_results), std::move(google_config_results));
}

void DnsProbeBrowserTest::NavigateToDnsError() {
  ASSERT_TRUE(NavigateToURL(
      active_browser_,
      URLRequestFailedJob::GetMockHttpsUrl(net::ERR_NAME_NOT_RESOLVED)));
}

void DnsProbeBrowserTest::NavigateToOtherError() {
  ASSERT_TRUE(NavigateToURL(
      active_browser_,
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_REFUSED)));
}

void DnsProbeBrowserTest::StartDelayedProbes(int expected_delayed_probe_count) {
  ASSERT_TRUE(delaying_dns_probe_service_);

  int actual_delayed_probe_count =
      delaying_dns_probe_service_->delayed_probe_count();
  EXPECT_EQ(expected_delayed_probe_count, actual_delayed_probe_count);

  delaying_dns_probe_service_->StartDelayedProbes();
}

DnsProbeStatus DnsProbeBrowserTest::WaitForSentStatus() {
  CHECK(!awaiting_dns_probe_status_run_loop_);
  while (dns_probe_status_queue_.empty()) {
    awaiting_dns_probe_status_run_loop_ = std::make_unique<base::RunLoop>();
    awaiting_dns_probe_status_run_loop_->Run();
    awaiting_dns_probe_status_run_loop_ = nullptr;
  }

  CHECK(!dns_probe_status_queue_.empty());
  DnsProbeStatus status = dns_probe_status_queue_.front();
  dns_probe_status_queue_.pop_front();
  return status;
}

// Check title by roundtripping to renderer, to make sure any probe results
// sent before this have been applied.
std::string DnsProbeBrowserTest::Title() {
  std::string title;

  WebContents* contents =
      active_browser_->tab_strip_model()->GetActiveWebContents();

  return content::EvalJs(contents, "document.title;").ExtractString();
}

// Check text by roundtripping to renderer, to make sure any probe results
// sent before this have been applied.
bool DnsProbeBrowserTest::PageContains(const std::string& expected) {
  std::string text_content =
      content::EvalJs(
          active_browser_->tab_strip_model()->GetActiveWebContents(),
          "document.body.textContent;")
          .ExtractString();

  return text_content.find(expected) != std::string::npos;
}

void DnsProbeBrowserTest::ExpectDisplayingErrorPage(
    const std::string& status_text) {
  EXPECT_FALSE(PageContains("http://mock.http/title2.html"));
  EXPECT_TRUE(PageContains(status_text));
}

void DnsProbeBrowserTest::OnDnsProbeStatusSent(
    DnsProbeStatus dns_probe_status) {
  dns_probe_status_queue_.push_back(dns_probe_status);
  if (awaiting_dns_probe_status_run_loop_)
    awaiting_dns_probe_status_run_loop_->Quit();
}

mojo::Remote<network::mojom::DnsConfigChangeManager>
DnsProbeBrowserTest::GetDnsConfigChangeManager() {
  mojo::Remote<network::mojom::DnsConfigChangeManager>
      dns_config_change_manager_remote;
  dns_config_change_manager_ = std::make_unique<FakeDnsConfigChangeManager>(
      dns_config_change_manager_remote.BindNewPipeAndPassReceiver());
  return dns_config_change_manager_remote;
}

// Test Fixture for tests where the DNS probes should succeed.
class DnsProbeSuccessfulProbesTest : public DnsProbeBrowserTest {
  void SetUpOnMainThread() override {
    SetFakeHostResolverResults({{net::OK, net::ResolveErrorInfo(net::OK),
                                 FakeHostResolver::kOneAddressResponse}},
                               {{net::OK, net::ResolveErrorInfo(net::OK),
                                 FakeHostResolver::kOneAddressResponse}});
    DnsProbeBrowserTest::SetUpOnMainThread();
  }
};

// Test Fixture for tests where the DNS probes should not resolve.
class DnsProbeFailingProbesTest : public DnsProbeBrowserTest {
  void SetUpOnMainThread() override {
    SetFakeHostResolverResults(
        {{net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
          FakeHostResolver::kNoResponse}},
        {{net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
          FakeHostResolver::kNoResponse}});
    DnsProbeBrowserTest::SetUpOnMainThread();
  }
};

class DnsProbeCurrentConfigFailingProbesTest : public DnsProbeBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    SetFakeHostResolverResults(
        {{net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
          FakeHostResolver::kNoResponse}},
        {{net::OK, net::ResolveErrorInfo(net::OK),
          FakeHostResolver::kOneAddressResponse}});
    DnsProbeBrowserTest::SetUpOnMainThread();
  }
};

class DnsProbeCurrentSecureConfigFailingProbesTest
    : public DnsProbeBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // Normal boilerplate to setup a MockConfigurationPolicyProvider.
    ON_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(policy_provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_WIN)
    // Mark as not enterprise managed to prevent the secure DNS mode from
    // being downgraded to off.
    base::win::ScopedDomainStateForTesting scoped_domain(false);
    // TODO(crbug.com/40229843): What is the correct function to use here?
    EXPECT_FALSE(base::win::IsEnrolledToDomain());
#endif

    // Set the mocked policy provider to act as if no policies are in use by
    // updating to an empty PolicyMap. Done to prevent potential unintended
    // Secure DNS downgrade.
    policy_provider_.UpdateChromePolicy(policy::PolicyMap());

    // Override parental-controls detection to not detect anything, to prevent a
    // potential Secure DNS downgrade of off. Trigger an update to network
    // service to ensure the override takes effect if parental controls have
    // already been read.
    StubResolverConfigReader* config_reader =
        SystemNetworkContextManager::GetStubResolverConfigReader();
    config_reader->OverrideParentalControlsForTesting(
        /*parental_controls_override=*/false);
    config_reader->UpdateNetworkService(/*record_metrics=*/false);
    content::FlushNetworkServiceInstanceForTesting();

    // Update prefs to enable Secure DNS in secure mode.
    PrefService* pref_service = g_browser_process->local_state();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Chrome OS, the local_state is shared between all users so the user-set
    // pref is stored in the profile's pref service.
    pref_service = browser()->profile()->GetPrefs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    pref_service->SetString(prefs::kDnsOverHttpsMode,
                            SecureDnsConfig::kModeSecure);
    pref_service->SetString(prefs::kDnsOverHttpsTemplates,
                            "https://bar.test/dns-query{?dns}");

    SetFakeHostResolverResults(
        {{net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
          FakeHostResolver::kNoResponse}},
        {{net::OK, net::ResolveErrorInfo(net::OK),
          FakeHostResolver::kOneAddressResponse}});
    DnsProbeBrowserTest::SetUpOnMainThread();
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Test Fixture for tests where the DNS probes should fail to connect to a DNS
// server (timeout or unreachable host).
class DnsProbeUnreachableProbesTest : public DnsProbeBrowserTest {
  void SetUpOnMainThread() override {
    SetFakeHostResolverResults({{net::ERR_NAME_NOT_RESOLVED,
                                 net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                                 FakeHostResolver::kNoResponse}},
                               {{net::ERR_NAME_NOT_RESOLVED,
                                 net::ResolveErrorInfo(net::ERR_DNS_TIMED_OUT),
                                 FakeHostResolver::kNoResponse}});
    DnsProbeBrowserTest::SetUpOnMainThread();
  }
};

// Make sure probes don't break non-DNS error.
IN_PROC_BROWSER_TEST_F(DnsProbeSuccessfulProbesTest, OtherErrorWith) {
  NavigateToOtherError();
  ExpectDisplayingErrorPage("ERR_CONNECTION_REFUSED");
}

IN_PROC_BROWSER_TEST_F(DnsProbeCurrentConfigFailingProbesTest, BadConfig) {
  NavigateToDnsError();

  EXPECT_EQ(error_page::DNS_PROBE_STARTED, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_STARTED");
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_BAD_CONFIG, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_FINISHED_BAD_CONFIG");
}

IN_PROC_BROWSER_TEST_F(DnsProbeCurrentSecureConfigFailingProbesTest,
                       BadSecureConfig) {
  NavigateToDnsError();

  EXPECT_EQ(error_page::DNS_PROBE_STARTED, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_STARTED");
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_BAD_SECURE_CONFIG,
            WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_FINISHED_BAD_SECURE_CONFIG");
}

// Make sure probes update DNS error page properly when they're supposed to.
IN_PROC_BROWSER_TEST_F(DnsProbeUnreachableProbesTest, NoInternetProbeResult) {
  NavigateToDnsError();

  EXPECT_EQ(error_page::DNS_PROBE_STARTED, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_STARTED");
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NO_INTERNET, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_FINISHED_NO_INTERNET");
}

// Double-check to make sure sync failures don't explode.
IN_PROC_BROWSER_TEST_F(DnsProbeFailingProbesTest, SyncFailure) {
  NavigateToDnsError();

  EXPECT_EQ(error_page::DNS_PROBE_STARTED, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_STARTED");
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("ERR_NAME_NOT_RESOLVED");
  EXPECT_EQ(0, pending_status_count());
}

// Make sure probes don't run for subframe DNS errors.
IN_PROC_BROWSER_TEST_F(DnsProbeSuccessfulProbesTest, NoProbeInSubframe) {
  ASSERT_TRUE(NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_dns_error.html")));

  // By the time NavigateToURL returns, the browser will have seen the failed
  // provisional load.  If a probe was started (or considered but not run),
  // then the NetErrorTabHelper would have sent a NetErrorInfo message.  Thus,
  // if one hasn't been sent by now, the NetErrorTabHelper has not (and won't)
  // start a probe for this DNS error.
  EXPECT_EQ(0, pending_status_count());
}

// Make sure browser sends NOT_RUN properly when probes are disabled.
IN_PROC_BROWSER_TEST_F(DnsProbeUnreachableProbesTest, ProbesDisabled) {
  // Disable probes (And corrections).
  browser()->profile()->GetPrefs()->SetBoolean(
      embedder_support::kAlternateErrorPagesEnabled, false);

  NavigateToDnsError();

  EXPECT_EQ(error_page::DNS_PROBE_NOT_RUN, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("ERR_NAME_NOT_RESOLVED");
}

// Test incognito mode. DNS probes should still be enabled.
IN_PROC_BROWSER_TEST_F(DnsProbeFailingProbesTest, Incognito) {
  Browser* incognito = CreateIncognitoBrowser();
  SetActiveBrowser(incognito);

  // Just one commit and one sent status, since the corrections are disabled.
  NavigateToDnsError();
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, WaitForSentStatus());

  // Checking the page runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("DNS_PROBE_STARTED");
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_INCONCLUSIVE, WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  ExpectDisplayingErrorPage("ERR_NAME_NOT_RESOLVED");
}

}  // namespace

}  // namespace chrome_browser_net
