// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "chrome/browser/tpcd/heuristics/redirect_heuristic_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-shared.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "device/fido/virtual_fido_device_factory.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using base::Bucket;
using content::CookieAccessDetails;
using content::NavigationHandle;
using content::WebContents;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::IsEmpty;
using testing::Pair;
using ukm::builders::DIPS_Redirect;
using AttributionData = std::set<content::AttributionDataModel::DataKey>;

namespace {

using blink::mojom::StorageTypeAccessed;

// Returns a simplified URL representation for ease of comparison in tests.
// Just host+path.
std::string FormatURL(const GURL& url) {
  return base::StrCat({url.host_piece(), url.path_piece()});
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const DIPSRedirectInfo& redirect,
                    const DIPSRedirectChainInfo& chain,
                    size_t redirect_index) {
  redirects->push_back(base::StringPrintf(
      "[%zu/%zu] %s -> %s (%s) -> %s", redirect_index + 1, chain.length,
      FormatURL(chain.initial_url.url).c_str(),
      FormatURL(redirect.url.url).c_str(),
      std::string(SiteDataAccessTypeToString(redirect.access_type)).c_str(),
      FormatURL(chain.final_url.url).c_str()));
}

void AppendRedirects(std::vector<std::string>* vec,
                     std::vector<DIPSRedirectInfoPtr> redirects,
                     DIPSRedirectChainInfoPtr chain) {
  size_t redirect_index = chain->length - redirects.size();
  for (const auto& redirect : redirects) {
    AppendRedirect(vec, *redirect, *chain, redirect_index);
    redirect_index++;
  }
}

void AppendSitesInReport(std::vector<std::string>* reports,
                         const std::set<std::string>& sites) {
  reports->push_back(base::JoinString(
      std::vector<std::string_view>(sites.begin(), sites.end()), ", "));
}

std::vector<url::Origin> GetOrigins(const AttributionData& data) {
  std::vector<url::Origin> origins;
  base::ranges::transform(
      data, std::back_inserter(origins),
      &content::AttributionDataModel::DataKey::reporting_origin);
  return origins;
}

}  // namespace

// Keeps a log of DidStartNavigation, OnCookiesAccessed, and DidFinishNavigation
// executions.
class WCOCallbackLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WCOCallbackLogger>,
      public content::SharedWorkerService::Observer,
      public content::DedicatedWorkerService::Observer {
 public:
  WCOCallbackLogger(const WCOCallbackLogger&) = delete;
  WCOCallbackLogger& operator=(const WCOCallbackLogger&) = delete;

  const std::vector<std::string>& log() const { return log_; }

 private:
  explicit WCOCallbackLogger(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<WCOCallbackLogger>;

  // Start WebContentsObserver overrides:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void NotifyStorageAccessed(content::RenderFrameHost* render_frame_host,
                             StorageTypeAccessed storage_type,
                             bool blocked) override;
  void OnServiceWorkerAccessed(
      content::RenderFrameHost* render_frame_host,
      const GURL& scope,
      content::AllowServiceWorkerResult allowed) override;
  void OnServiceWorkerAccessed(
      content::NavigationHandle* navigation_handle,
      const GURL& scope,
      content::AllowServiceWorkerResult allowed) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void WebAuthnAssertionRequestSucceeded(
      content::RenderFrameHost* render_frame_host) override;
  // End WebContentsObserver overrides.

  // Start SharedWorkerService.Observer overrides:
  void OnClientAdded(
      const blink::SharedWorkerToken& token,
      content::GlobalRenderFrameHostId render_frame_host_id) override;
  void OnWorkerCreated(const blink::SharedWorkerToken& token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       const base::UnguessableToken& dev_tools_token) override {
  }
  void OnBeforeWorkerDestroyed(const blink::SharedWorkerToken& token) override {
  }
  void OnClientRemoved(
      const blink::SharedWorkerToken& token,
      content::GlobalRenderFrameHostId render_frame_host_id) override {}
  using content::SharedWorkerService::Observer::OnFinalResponseURLDetermined;
  // End SharedWorkerService.Observer overrides.

  // Start DedicatedWorkerService.Observer overrides:
  void OnWorkerCreated(const blink::DedicatedWorkerToken& worker_token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       content::DedicatedWorkerCreator creator) override;
  void OnBeforeWorkerDestroyed(
      const blink::DedicatedWorkerToken& worker_token,
      content::DedicatedWorkerCreator creator) override {}
  void OnFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& worker_token,
      const GURL& url) override {}
  // End DedicatedWorkerService.Observer overrides.

  std::vector<std::string> log_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WCOCallbackLogger::WCOCallbackLogger(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WCOCallbackLogger>(*web_contents) {}

void WCOCallbackLogger::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  log_.push_back(
      base::StringPrintf("DidStartNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  // Callbacks for favicons are ignored only in testing logs because their
  // ordering is variable and would cause flakiness
  if (details.url.path() == "/favicon.ico") {
    return;
  }

  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(RenderFrameHost, %s: %s)",
      details.type == CookieOperation::kChange ? "Change" : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(NavigationHandle, %s: %s)",
      details.type == CookieOperation::kChange ? "Change" : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::OnServiceWorkerAccessed(
    content::RenderFrameHost* render_frame_host,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  log_.push_back(
      base::StringPrintf("OnServiceWorkerAccessed(RenderFrameHost: %s)",
                         FormatURL(scope).c_str()));
}

void WCOCallbackLogger::OnServiceWorkerAccessed(
    content::NavigationHandle* navigation_handle,
    const GURL& scope,
    content::AllowServiceWorkerResult allowed) {
  log_.push_back(
      base::StringPrintf("OnServiceWorkerAccessed(NavigationHandle: %s)",
                         FormatURL(scope).c_str()));
}

void WCOCallbackLogger::OnClientAdded(
    const blink::SharedWorkerToken& token,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);
  GURL scope = GetFirstPartyURL(render_frame_host).value_or(GURL());

  log_.push_back(base::StringPrintf("OnSharedWorkerClientAdded(%s)",
                                    FormatURL(scope).c_str()));
}

void WCOCallbackLogger::OnWorkerCreated(
    const blink::DedicatedWorkerToken& worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    content::DedicatedWorkerCreator creator) {
  const content::GlobalRenderFrameHostId& render_frame_host_id =
      absl::get<content::GlobalRenderFrameHostId>(creator);
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);
  GURL scope = GetFirstPartyURL(render_frame_host).value_or(GURL());

  log_.push_back(base::StringPrintf("OnDedicatedWorkerCreated(%s)",
                                    FormatURL(scope).c_str()));
}

void WCOCallbackLogger::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!IsInPrimaryPage(navigation_handle)) {
    return;
  }

  // Android testing produces callbacks for a finished navigation to "blank" at
  // the beginning of a test. These should be ignored here.
  if (FormatURL(navigation_handle->GetURL()) == "blank" ||
      navigation_handle->GetPreviousPrimaryMainFrameURL().is_empty()) {
    return;
  }
  log_.push_back(
      base::StringPrintf("DidFinishNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

void WCOCallbackLogger::WebAuthnAssertionRequestSucceeded(
    content::RenderFrameHost* render_frame_host) {
  log_.push_back(base::StringPrintf(
      "WebAuthnAssertionRequestSucceeded(%s)",
      FormatURL(render_frame_host->GetLastCommittedURL()).c_str()));
}

void WCOCallbackLogger::NotifyStorageAccessed(
    content::RenderFrameHost* render_frame_host,
    StorageTypeAccessed storage_type,
    bool blocked) {
  log_.push_back(base::StringPrintf(
      "NotifyStorageAccessed(%s: %s)", base::ToString(storage_type).c_str(),
      FormatURL(render_frame_host->GetLastCommittedURL()).c_str()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

class DIPSBounceDetectorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 protected:
  DIPSBounceDetectorBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &DIPSBounceDetectorBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    enabled_features_.push_back(
        {network::features::kSkipTpcdMitigationsForAds,
         {{"SkipTpcdMitigationsForAdsHeuristics", "true"}}});
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    disabled_features_.push_back(features::kHttpsUpgrades);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_,
                                                       disabled_features_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    SetUpDIPSWebContentsObserver();

    // These rules apply an ad-tagging param to cookies marked with the `isad=1`
    // param value.
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("isad=1")});
  }

  void SetUpDIPSWebContentsObserver() {
    web_contents_observer_ =
        DIPSWebContentsObserver::FromWebContents(GetActiveWebContents());
    CHECK(web_contents_observer_);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    GetRedirectChainHelper()->SetRedirectChainHandlerForTesting(
        base::BindRepeating(&AppendRedirects, redirects));
  }

  void StartAppendingReportsTo(std::vector<std::string>* reports) {
    web_contents_observer_->SetIssueReportingCallbackForTesting(
        base::BindRepeating(&AppendSitesInReport, reports));
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  //
  // By default (when `wait`=true) this waits for the DIPSService to tell
  // observers that the redirect chain was handled. But some tests override
  // the handling flow so that chains don't reach the service (and so observers
  // are never notified). Such tests should pass `wait`=false.
  void EndRedirectChain(bool wait = true) {
    WebContents* web_contents = GetActiveWebContents();
    DIPSService* dips_service =
        DIPSService::Get(web_contents->GetBrowserContext());
    GURL expected_url = web_contents->GetLastCommittedURL();

    RedirectChainObserver chain_observer(dips_service, expected_url);
    // Performing a browser-based navigation terminates the current redirect
    // chain.
    ASSERT_TRUE(content::NavigateToURL(
        web_contents,
        embedded_test_server()->GetURL("endthechain.test", "/title1.html")));
    if (wait) {
      chain_observer.Wait();
    }
  }

  [[nodiscard]] bool AccessStorage(content::RenderFrameHost* frame,
                                   StorageTypeAccessed type) {
    // We drop the first character of ToString(type) because it's just the
    // constant-indicating 'k'.
    return content::ExecJs(
        frame,
        base::StringPrintf(kStorageAccessScript,
                           base::ToString(type).substr(1).c_str()),
        content::EXECUTE_SCRIPT_NO_USER_GESTURE,
        /*world_id=*/1);
  }

  auto* fenced_frame_test_helper() { return &fenced_frame_test_helper_; }
  auto* prerender_test_helper() { return &prerender_test_helper_; }

  content::RenderFrameHost* GetIFrame() {
    content::WebContents* web_contents = GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedIFrame() {
    return ChildFrameAt(GetIFrame(), 0);
  }

  RedirectChainDetector* GetRedirectChainHelper() {
    return RedirectChainDetector::FromWebContents(GetActiveWebContents());
  }

  void NavigateNestedIFrameTo(content::RenderFrameHost* parent_frame,
                              const std::string& iframe_id,
                              const GURL& url) {
    content::TestNavigationObserver load_observer(GetActiveWebContents());
    std::string script = base::StringPrintf(
        "var iframe = document.getElementById('%s');iframe.src='%s';",
        iframe_id.c_str(), url.spec().c_str());
    ASSERT_TRUE(content::ExecJs(parent_frame, script,
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    load_observer.Wait();
  }

  void AccessCHIPSViaJSIn(content::RenderFrameHost* frame) {
    FrameCookieAccessObserver observer(GetActiveWebContents(), frame,
                                       CookieOperation::kChange);
    ASSERT_TRUE(content::ExecJs(frame,
                                "document.cookie = '__Host-foo=bar;"
                                "SameSite=None;Secure;Path=/;Partitioned';",
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    observer.Wait();
  }

  void SimulateMouseClick() {
    SimulateMouseClickAndWait(GetActiveWebContents());
  }

  void SimulateCookieWrite() {
    WebContents* web_contents = GetActiveWebContents();
    content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
    URLCookieAccessObserver cookie_observer(
        web_contents, frame->GetLastCommittedURL(), CookieOperation::kChange);
    ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    cookie_observer.Wait();
  }

  const base::FilePath kChromeTestDataDir =
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));

  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
  raw_ptr<DIPSWebContentsObserver, AcrossTasksDanglingUntriaged>
      web_contents_observer_ = nullptr;

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    // TODO(crbug.com/40924446): Re-enable this test
    DISABLED_AttributeSameSiteIframesCookieClientAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  AccessCookieViaJSIn(GetActiveWebContents(), GetIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

// TODO(crbug.com/40276415): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AttributeSameSiteIframesCookieServerAccessTo1P \
  DISABLED_AttributeSameSiteIframesCookieServerAccessTo1P
#else
#define MAYBE_AttributeSameSiteIframesCookieServerAccessTo1P \
  AttributeSameSiteIframesCookieServerAccessTo1P
#endif
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       MAYBE_AttributeSameSiteIframesCookieServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()))
      ->SetThirdPartyCookieSetting(
          embedded_test_server()->GetURL("a.test", "/"), CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      https_server.GetURL("a.test", "/set-cookie?foo=bar;SameSite=None;Secure");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Attribute3PIframesCHIPSClientAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url = https_server.GetURL("b.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  AccessCHIPSViaJSIn(GetIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       // TODO(crbug.com/40287072): Re-enable this test
                       DISABLED_Attribute3PIframesCHIPSServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      https_server.GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      https_server.GetURL("b.test",
                          "/set-cookie?__Host-foo=bar;SameSite=None;"
                          "Secure;Path=/;Partitioned");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    // TODO(crbug.com/40287072): Re-enable this test
    DISABLED_AttributeSameSiteNestedIframesCookieClientAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL nested_iframe_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  NavigateNestedIFrameTo(GetIFrame(), "test", nested_iframe_url);

  AccessCookieViaJSIn(GetActiveWebContents(), GetNestedIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    // TODO(crbug.com/40287072): Re-enable this test
    DISABLED_AttributeSameSiteNestedIframesCookieServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL nested_iframe_url =
      https_server.GetURL("a.test", "/set-cookie?foo=bar;SameSite=None;Secure");
  NavigateNestedIFrameTo(GetIFrame(), "test", nested_iframe_url);

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Attribute3PNestedIframesCHIPSClientAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("b.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL nested_iframe_url = https_server.GetURL("c.test", "/title1.html");
  NavigateNestedIFrameTo(GetIFrame(), "test", nested_iframe_url);

  AccessCHIPSViaJSIn(GetNestedIFrame());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Attribute3PNestedIframesCHIPSServerAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url =
      embedded_test_server()->GetURL("b.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL nested_iframe_url = https_server.GetURL(
      "a.test",
      "/set-cookie?__Host-foo=bar;SameSite=None;Secure;Path=/;Partitioned");
  NavigateNestedIFrameTo(GetIFrame(), "test", nested_iframe_url);

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       Attribute3PSubResourceCHIPSClientAccessTo1P) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  ASSERT_TRUE(https_server.Start());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // This block represents a navigation sequence with a CHIP access (write). It
  // might as well be happening in a separate tab from the navigation block
  // below that does the CHIP's read via subresource request.
  {
    const GURL primary_main_frame_url =
        embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

    const GURL iframe_url =
        embedded_test_server()->GetURL("b.test", "/iframe_blank.html");
    ASSERT_TRUE(content::NavigateIframeToURL(GetActiveWebContents(), "test",
                                             iframe_url));

    const GURL nested_iframe_url =
        https_server.GetURL("c.test", "/title1.html");
    NavigateNestedIFrameTo(GetIFrame(), "test", nested_iframe_url);

    AccessCHIPSViaJSIn(GetNestedIFrame());
  }

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  GURL image_url = https_server.GetURL("c.test", "/favicon/icon.png");
  CreateImageAndWaitForCookieAccess(GetActiveWebContents(), image_url);

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());

  EXPECT_THAT(
      redirects,
      ElementsAre(("[1/1] a.test/iframe_blank.html -> a.test/iframe_blank.html "
                   "(Read) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DiscardFencedFrameCookieClientAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("a.test", "/fenced_frames/title2.html");
  content::RenderFrameHostWrapper fenced_frame(
      fenced_frame_test_helper()->CreateFencedFrame(
          GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_FALSE(fenced_frame.IsDestroyed());

  AccessCookieViaJSIn(GetActiveWebContents(), fenced_frame.get());

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DiscardFencedFrameCookieServerAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "a.test", "/fenced_frames/set_cookie_header.html");
  URLCookieAccessObserver observer(GetActiveWebContents(), fenced_frame_url,
                                   CookieOperation::kChange);
  content::RenderFrameHostWrapper fenced_frame(
      fenced_frame_test_helper()->CreateFencedFrame(
          GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_FALSE(fenced_frame.IsDestroyed());
  observer.Wait();

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

// TODO(crbug.com/40917101): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DiscardPrerenderedPageCookieClientAccess \
  DISABLED_DiscardPrerenderedPageCookieClientAccess
#else
#define MAYBE_DiscardPrerenderedPageCookieClientAccess \
  DiscardPrerenderedPageCookieClientAccess
#endif
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       MAYBE_DiscardPrerenderedPageCookieClientAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url =
      embedded_test_server()->GetURL("a.test", "/title2.html");
  const content::FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  content::test::PrerenderHostObserver observer(*GetActiveWebContents(),
                                                host_id);
  EXPECT_FALSE(observer.was_activated());
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  AccessCookieViaJSIn(GetActiveWebContents(), prerender_frame);

  prerender_test_helper()->CancelPrerenderedPage(host_id);
  observer.WaitForDestroyed();

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

// TODO(crrev/1448453): flaky test.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DISABLED_DiscardPrerenderedPageCookieServerAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url =
      embedded_test_server()->GetURL("a.test", "/set_cookie_header.html");
  URLCookieAccessObserver observer(GetActiveWebContents(), prerendering_url,
                                   CookieOperation::kChange);
  const content::FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  observer.Wait();

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);
  EXPECT_FALSE(prerender_observer.was_activated());
  prerender_test_helper()->CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();

  const GURL primary_main_frame_final_url =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  // From the time the cookie was set by the prerendering page and now, the
  // primary main page might have accessed (read) the cookie (when sending a
  // request for a favicon after prerendering page already accessed (Write) the
  // cookie). To prevent flakiness we check for any such access and test for the
  // expected outcome accordingly.
  // TODO(crbug.com/40269100): Investigate whether Prerendering pages
  // (same-site) can be use for evasion.
  const std::string expected_access_type =
      observer.CookieAccessedInPrimaryPage() ? "Read" : "None";

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] blank -> a.test/title1.html (" +
                           expected_access_type + ") -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulBounce_ClientRedirect_SiteDataAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Navigate to the initial page, a.test.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not considered to be redirect) to b.test.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("b.test", "/title1.html")));

  EXPECT_TRUE(AccessStorage(GetActiveWebContents()->GetPrimaryMainFrame(),
                            StorageTypeAccessed::kLocalStorage));

  // Navigate without a click (considered a client-redirect) to c.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> b.test/title1.html "
                           "(Write) -> c.test/title1.html")));
}

// The timing of WCO::OnCookiesAccessed() execution is unpredictable for
// redirects. Sometimes it's called before WCO::DidRedirectNavigation(), and
// sometimes after. Therefore DIPSBounceDetector needs to know when it's safe to
// judge an HTTP redirect as stateful (accessing cookies) or not. This test
// tries to verify that OnCookiesAccessed() is always called before
// DidFinishNavigation(), so that DIPSBounceDetector can safely perform that
// judgement in DidFinishNavigation().
//
// This test also verifies that OnCookiesAccessed() is called for URLs in the
// same order that they're visited (and that for redirects that both read and
// write cookies, OnCookiesAccessed() is called with kRead before it's called
// with kChange, although DIPSBounceDetector doesn't depend on that anymore.)
//
// If either assumption is incorrect, this test will be flaky. On 2022-04-27 I
// (rtarpine) ran this test 1000 times in 40 parallel jobs with no failures, so
// it seems robust.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       AllCookieCallbacksBeforeNavigationFinished) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
      "d.test/set-cookie?name=value");
  GURL final_url =
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on all 4 test domains
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "a.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "b.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "c.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "d.test",
                                  /*is_secure_cookie_set=*/false,
                                  /*is_ad_tagged=*/false));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Visit the redirect.
  URLCookieAccessObserver observer(web_contents, final_url,
                                   CookieOperation::kChange);
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url, final_url));
  observer.Wait();

  // Verify that the 7 OnCookiesAccessed() executions are called in order, and
  // all between DidStartNavigation() and DidFinishNavigation().
  //
  // Note: according to web_contents_observer.h, sometimes cookie reads/writes
  // from navigations may cause the RenderFrameHost* overload of
  // OnCookiesAccessed to be called instead. We haven't seen that yet, and this
  // test will intentionally fail if it happens so that we'll notice.
  EXPECT_THAT(
      logger->log(),
      testing::ContainerEq(std::vector<std::string>(
          {("DidStartNavigation(a.test/cross-site/b.test/"
            "cross-site-with-cookie/"
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "a.test/cross-site/b.test/cross-site-with-cookie/c.test/"
            "cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
            "d.test/"
            "set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Change: "
            "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
            "d.test/"
            "set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Change: "
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           "OnCookiesAccessed(NavigationHandle, Read: d.test/set-cookie)",
           "OnCookiesAccessed(NavigationHandle, Change: d.test/set-cookie)",
           "DidFinishNavigation(d.test/set-cookie)"})));
}

// An EmbeddedTestServer request handler for
// /cross-site-with-samesite-none-cookie URLs. Like /cross-site-with-cookie, but
// the cookie has additional Secure and SameSite=None attributes.
std::unique_ptr<net::test_server::HttpResponse>
HandleCrossSiteSameSiteNoneCookieRedirect(
    net::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  const std::string prefix = "/cross-site-with-samesite-none-cookie";
  if (!net::test_server::ShouldHandle(request, prefix)) {
    return nullptr;
  }

  std::string dest_all = base::UnescapeBinaryURLComponent(
      request.relative_url.substr(prefix.size() + 1));

  std::string dest;
  size_t delimiter = dest_all.find("/");
  if (delimiter != std::string::npos) {
    dest = base::StringPrintf(
        "//%s:%hu/%s", dest_all.substr(0, delimiter).c_str(), server->port(),
        dest_all.substr(delimiter + 1).c_str());
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Set-Cookie",
                                 "server-redirect=true; Secure; SameSite=None");
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      dest.c_str()));
  return http_response;
}

// Ignore iframes because their state will be partitioned under the top-level
// site anyway.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       IgnoreServerRedirectsInIframes) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  const GURL root_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  const GURL redirect_url = https_server.GetURL(
      "b.test", "/cross-site-with-samesite-none-cookie/c.test/title1.html");
  const std::string iframe_id = "test";
  content::WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  ASSERT_TRUE(content::NavigateToURL(web_contents, root_url));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, iframe_id, redirect_url));
  EndRedirectChain(/*wait=*/false);

  // b.test had a stateful redirect, but because it was in an iframe, we ignored
  // it.
  EXPECT_THAT(redirects, IsEmpty());
}

// This test verifies that sites in a redirect chain with previous user
// interaction are not reported in the resulting issue when a navigation
// finishes.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       ReportRedirectorsInChain_OmitSitesWithInteraction) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> reports;
  StartAppendingReportsTo(&reports);

  // Record user activation on d.test.
  GURL url = embedded_test_server()->GetURL("d.test", "/title1.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  SimulateMouseClick();

  // Verify interaction was recorded for d.test, before proceeding.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_TRUE(state.has_value());
  ASSERT_TRUE(state->user_interaction_times.has_value());

  // Visit initial page on a.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which statefully
  // S-redirects to c.test and write a cookie on c.test.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to d.test and write a
  // cookie on d.test:
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // statefully S-redirects to f.test, which statefully S-redirects to g.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL(
          "e.test",
          "/cross-site-with-cookie/f.test/cross-site-with-cookie/g.test/"
          "title1.html"),
      embedded_test_server()->GetURL("g.test", "/title1.html")));
  EndRedirectChain();
  WaitOnStorage(GetDipsService(web_contents));

  EXPECT_THAT(reports, ElementsAre(("b.test"), ("c.test"), ("e.test, f.test")));
}

// This test verifies that a third-party cookie access doesn't cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    DetectStatefulRedirect_Client_IgnoreThirdPartySubresource) {
  // We host the image on an HTTPS server, because for it to read a third-party
  // cookie, it needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(kChromeTestDataDir);
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url = https_server.GetURL("d.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set SameSite=None cookie on d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, https_server.GetURL(
                        "d.test", "/set-cookie?foo=bar;Secure;SameSite=None")));

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Cause a third-party cookie read.
  CreateImageAndWaitForCookieAccess(web_contents, image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EXPECT_THAT(logger->log(),
              ElementsAre(
                  // Set cookie on d.test
                  ("DidStartNavigation(d.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: d.test/set-cookie)"),
                  ("DidFinishNavigation(d.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading third-party d.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: d.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));
  EndRedirectChain(/*wait=*/false);

  // b.test is a bounce, but not stateful.
  EXPECT_THAT(redirects, ElementsAre("[1/1] a.test/title1.html"
                                     " -> b.test/title1.html (None)"
                                     " -> c.test/title1.html"));
}

// This test verifies that a same-site cookie access DOES cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_Client_FirstPartySubresource) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url =
      embedded_test_server()->GetURL("sub.b.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set cookie on sub.b.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("sub.b.test", "/set-cookie?foo=bar")));

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Cause a same-site cookie read.
  CreateImageAndWaitForCookieAccess(web_contents, image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EXPECT_THAT(logger->log(),
              ElementsAre(
                  // Set cookie on sub.b.test
                  ("DidStartNavigation(sub.b.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: sub.b.test/set-cookie)"),
                  ("DidFinishNavigation(sub.b.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading same-site sub.b.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: sub.b.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));
  EndRedirectChain(/*wait=*/false);

  // b.test IS considered a stateful bounce, even though the cookie was read by
  // an image hosted on sub.b.test.
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> b.test/title1.html "
                           "(Read) -> c.test/title1.html")));
}

// This test verifies that consecutive redirect chains are combined into one.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_ServerClientClientServer) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Visit initial page on a.test
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to d.test
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL("e.test",
                                     "/cross-site/f.test/title1.html"),
      embedded_test_server()->GetURL("f.test", "/title1.html")));
  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(
      redirects,
      ElementsAre(("[1/4] a.test/title1.html -> "
                   "b.test/cross-site/c.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[2/4] a.test/title1.html -> c.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[3/4] a.test/title1.html -> d.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[4/4] a.test/title1.html -> "
                   "e.test/cross-site/f.test/title1.html (None) -> "
                   "f.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_ClosingTabEndsChain) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Visit initial page on a.test
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  EXPECT_THAT(redirects, IsEmpty());

  CloseTab(web_contents);

  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> "
                           "b.test/cross-site/c.test/title1.html (None) -> "
                           "c.test/title1.html")));
}

// Verifies server redirects that occur while opening a link in a new tab are
// properly detected.
// TODO(crbug.com/40936579): Flaky on Chrome OS and Linux.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_OpenServerRedirectURLInNewTab \
  DISABLED_OpenServerRedirectURLInNewTab
#else
#define MAYBE_OpenServerRedirectURLInNewTab OpenServerRedirectURLInNewTab
#endif
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       MAYBE_OpenServerRedirectURLInNewTab) {
  WebContents* original_tab = chrome_test_utils::GetActiveWebContents(this);
  GURL original_tab_url(
      embedded_test_server()->GetURL("a.test", "/title1.html"));
  ASSERT_TRUE(content::NavigateToURL(original_tab, original_tab_url));

  // Open a server-redirecting link in a new tab.
  GURL new_tab_url(embedded_test_server()->GetURL(
      "b.test", "/cross-site-with-cookie/c.test/title1.html"));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(original_tab, new_tab_url));

  // Verify the tab is different from the original and at the correct URL.
  EXPECT_NE(new_tab, original_tab);
  ASSERT_EQ(new_tab->GetLastCommittedURL(),
            embedded_test_server()->GetURL("c.test", "/title1.html"));

  std::vector<std::string> redirects;
  RedirectChainDetector* tab_web_contents_observer =
      RedirectChainDetector::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(redirects,
              ElementsAre((
                  "[1/1] a.test/ -> " /* Note: the URL's path is lost here. */
                  "b.test/cross-site-with-cookie/c.test/title1.html (Write) -> "
                  "c.test/title1.html")));
}

// Verifies client redirects that occur while opening a link in a new tab are
// properly detected.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       OpenClientRedirectURLInNewTab) {
  WebContents* original_tab = chrome_test_utils::GetActiveWebContents(this);
  GURL original_tab_url(
      embedded_test_server()->GetURL("a.test", "/title1.html"));
  ASSERT_TRUE(content::NavigateToURL(original_tab, original_tab_url));

  // Open link in a new tab.
  GURL new_tab_url(embedded_test_server()->GetURL("b.test", "/title1.html"));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(original_tab, new_tab_url));

  // Verify the tab is different from the original and at the correct URL.
  EXPECT_NE(original_tab, new_tab);
  ASSERT_EQ(new_tab_url, new_tab->GetLastCommittedURL());

  std::vector<std::string> redirects;
  RedirectChainDetector* tab_web_contents_observer =
      RedirectChainDetector::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  // Navigate without a click (i.e. by C-redirecting) to c.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      new_tab, embedded_test_server()->GetURL("c.test", "/title1.html")));
  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(
      redirects,
      ElementsAre(("[1/1] a.test/ -> " /* Note: the URL's path is lost here. */
                   "b.test/title1.html (None) -> "
                   "c.test/title1.html")));
}

// Verifies the start URL of a redirect chain started by opening a link in a new
// tab is handled correctly, when that start page has an opaque origin.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       OpenRedirectURLInNewTab_OpaqueOriginInitiator) {
  WebContents* original_tab = chrome_test_utils::GetActiveWebContents(this);
  GURL original_tab_url("data:text/html,<html></html>");
  ASSERT_TRUE(content::NavigateToURL(original_tab, original_tab_url));

  // Open a server-redirecting link in a new tab.
  GURL new_tab_url(embedded_test_server()->GetURL(
      "b.test", "/cross-site-with-cookie/c.test/title1.html"));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(original_tab, new_tab_url));

  // Verify the tab is different from the original and at the correct URL.
  EXPECT_NE(new_tab, original_tab);
  ASSERT_EQ(new_tab->GetLastCommittedURL(),
            embedded_test_server()->GetURL("c.test", "/title1.html"));

  std::vector<std::string> redirects;
  RedirectChainDetector* tab_web_contents_observer =
      RedirectChainDetector::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  EndRedirectChain(/*wait=*/false);

  EXPECT_THAT(redirects,
              ElementsAre((
                  "[1/1] blank -> "
                  "b.test/cross-site-with-cookie/c.test/title1.html (Write) -> "
                  "c.test/title1.html")));
}

class RedirectHeuristicBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  void EndRedirectChain() {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("endthechain.test", "/title1.html")));
  }

  void SimulateMouseClick() {
    SimulateMouseClickAndWait(GetActiveWebContents());
  }
};

// Tests the conditions for recording RedirectHeuristic_CookieAccess2 and
// RedirectHeuristic_CookieAccessThirdParty2 UKM events.
IN_PROC_BROWSER_TEST_F(RedirectHeuristicBrowserTest,
                       RecordsRedirectHeuristicCookieAccessEvent) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WebContents* web_contents = GetActiveWebContents();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  GURL tracker_url_pre_target_redirect =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL image_url_pre_target_redirect =
      https_server.GetURL("sub.b.test", "/favicon/icon.png");

  GURL target_url = embedded_test_server()->GetURL("d.test", "/title1.html");
  GURL target_image_url =
      https_server.GetURL("sub.d.test", "/favicon/icon.png");

  GURL tracker_url_post_target_redirect =
      embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url_post_target_redirect =
      https_server.GetURL("sub.c.test", "/favicon/icon.png");

  GURL final_url = embedded_test_server()->GetURL("f.test", "/title1.html");

  // Initialize 3PC settings for the target site.
  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      web_contents->GetBrowserContext());
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]" + target_url.host()),
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW);

  // Set cookies on image URLs.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.c.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.d.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));

  // Visit initial page.
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Redirect to tracking URL.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_pre_target_redirect));

  // Redirect to target URL.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   target_url));
  // Read a cookie from the tracking URL.
  CreateImageAndWaitForCookieAccess(web_contents,
                                    image_url_pre_target_redirect);
  // Read a cookie from the second tracking URL.
  CreateImageAndWaitForCookieAccess(web_contents,
                                    image_url_post_target_redirect);
  // Read a cookie from an image with the same domain as the target URL.
  CreateImageAndWaitForCookieAccess(web_contents, target_image_url);

  // Redirect to second tracking URL. (This has no effect since the cookie
  // accesses already happened.)
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_post_target_redirect));
  // Redirect to final URL.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EndRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_first_party_entries =
          ukm_recorder.GetEntries("RedirectHeuristic.CookieAccess2", {});

  // Expect one UKM entry.

  // Include the cookies read where a tracking site read cookies while embedded
  // on a site later in the redirect chain.

  // Exclude the cookies reads where:
  // - The tracking site did not appear in the prior redirect chain.
  // - The tracking and target sites had the same domain.
  ASSERT_EQ(1u, ukm_first_party_entries.size());
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_first_party_entries[0].source_id)
          ->url(),
      Eq(target_url));

  // Expect one corresponding UKM entry for CookieAccessThirdParty.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_third_party_entries = ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccessThirdParty2", {});
  ASSERT_EQ(1u, ukm_third_party_entries.size());
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[0].source_id)
          ->url(),
      Eq(tracker_url_pre_target_redirect));
}

// Tests setting different metrics for the RedirectHeuristic_CookieAccess2 UKM
// event.
// TODO(crbug.com/40934961): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(RedirectHeuristicBrowserTest,
                       DISABLED_RedirectHeuristicCookieAccessEvent_AllMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WebContents* web_contents = GetActiveWebContents();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL final_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  GURL tracker_url_with_interaction =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL image_url_with_interaction =
      https_server.GetURL("sub.b.test", "/favicon/icon.png");

  GURL tracker_url_in_iframe =
      embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url_in_iframe =
      https_server.GetURL("sub.c.test", "/favicon/icon.png");

  GURL target_url_3pc_allowed =
      embedded_test_server()->GetURL("d.test", "/title1.html");
  GURL target_url_3pc_blocked =
      embedded_test_server()->GetURL("e.test", "/iframe_blank.html");

  // Initialize 3PC settings for the target sites.
  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      web_contents->GetBrowserContext());
  map->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                    ContentSettingsPattern::FromString(
                                        "[*.]" + target_url_3pc_allowed.host()),
                                    ContentSettingsType::COOKIES,
                                    ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                    ContentSettingsPattern::FromString(
                                        "[*.]" + target_url_3pc_blocked.host()),
                                    ContentSettingsType::COOKIES,
                                    ContentSetting::CONTENT_SETTING_BLOCK);

  // Set cookies on image URLs.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/true));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.c.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));

  // Start on `tracker_url_with_interaction` and record a current interaction.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents, tracker_url_with_interaction));
  SimulateMouseClick();

  // Redirect to one of the target URLs, to set DoesFirstPartyPrecedeThirdParty.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, target_url_3pc_blocked));
  // Redirect to all tracking URLs.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_in_iframe));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, tracker_url_with_interaction));

  // Redirect to target URL with cookies allowed.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, target_url_3pc_allowed));
  // Read a cookie from the tracking URL with interaction.
  CreateImageAndWaitForCookieAccess(
      web_contents,
      https_server.GetURL("sub.b.test", "/favicon/icon.png?isad=1"));

  // Redirect to target URL with cookies blocked.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, target_url_3pc_blocked));
  // Open an iframe of the tracking URL on the target URL.
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents,
                                           /*iframe_id=*/"test",
                                           image_url_in_iframe));
  // Read a cookie from the tracking URL in an iframe on the target page.
  CreateImageAndWaitForCookieAccess(web_contents, image_url_in_iframe);

  // Redirect to final URL.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EndRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccess2",
          {"AccessId", "AccessAllowed", "IsAdTagged",
           "HoursSinceLastInteraction", "MillisecondsSinceRedirect",
           "OpenerHasSameSiteIframe", "SitesPassedCount",
           "DoesFirstPartyPrecedeThirdParty", "IsCurrentInteraction"});

  // Expect UKM entries from both of the cookie accesses.
  ASSERT_EQ(2u, ukm_entries.size());

  // Expect reasonable delays between the redirect and cookie access.
  for (const auto& entry : ukm_entries) {
    EXPECT_GT(entry.metrics.at("MillisecondsSinceRedirect"), 0);
    EXPECT_LT(entry.metrics.at("MillisecondsSinceRedirect"), 1000);
  }

  // The first cookie access was from a tracking site with a user interaction
  // within the last hour, on a site with 3PC access allowed.

  // 1 site was passed: tracker_url_with_interaction -> target_url_3pc_allowed
  auto access_id_1 = ukm_entries[0].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[0].source_id)->url(),
      Eq(target_url_3pc_allowed));
  EXPECT_EQ(ukm_entries[0].metrics.at("AccessAllowed"), true);
  EXPECT_EQ(ukm_entries[0].metrics.at("IsAdTagged"),
            static_cast<int32_t>(OptionalBool::kTrue));
  EXPECT_EQ(ukm_entries[0].metrics.at("HoursSinceLastInteraction"), 0);
  EXPECT_EQ(ukm_entries[0].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[0].metrics.at("SitesPassedCount"), 1);
  EXPECT_EQ(ukm_entries[0].metrics.at("DoesFirstPartyPrecedeThirdParty"),
            false);
  EXPECT_EQ(ukm_entries[0].metrics.at("IsCurrentInteraction"), 1);

  // The third cookie access was from a tracking site in an iframe of the
  // target, on a site with 3PC access blocked.

  // 3 sites were passed: tracker_url_in_iframe -> tracker_url_with_interaction
  // -> target_url_3pc_allowed -> target_url_3pc_blocked
  auto access_id_2 = ukm_entries[1].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[1].source_id)->url(),
      Eq(target_url_3pc_blocked));
  EXPECT_EQ(ukm_entries[1].metrics.at("AccessAllowed"), false);
  EXPECT_EQ(ukm_entries[1].metrics.at("IsAdTagged"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[1].metrics.at("HoursSinceLastInteraction"), -1);
  EXPECT_EQ(ukm_entries[1].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kTrue));
  EXPECT_EQ(ukm_entries[1].metrics.at("SitesPassedCount"), 3);
  EXPECT_EQ(ukm_entries[1].metrics.at("DoesFirstPartyPrecedeThirdParty"), true);
  EXPECT_EQ(ukm_entries[1].metrics.at("IsCurrentInteraction"), 0);

  // Verify there are 2 corresponding CookieAccessThirdParty entries with
  // matching access IDs.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_third_party_entries = ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccessThirdParty2", {"AccessId"});
  ASSERT_EQ(2u, ukm_third_party_entries.size());

  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[0].source_id)
          ->url(),
      Eq(tracker_url_with_interaction));
  EXPECT_EQ(ukm_third_party_entries[0].metrics.at("AccessId"), access_id_1);

  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[1].source_id)
          ->url(),
      Eq(tracker_url_in_iframe));
  EXPECT_EQ(ukm_third_party_entries[1].metrics.at("AccessId"), access_id_2);
}

struct RedirectHeuristicFlags {
  bool write_redirect_grants = false;
  bool require_aba_flow = true;
  bool require_current_interaction = true;
};

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class RedirectHeuristicGrantTest
    : public RedirectHeuristicBrowserTest,
      public testing::WithParamInterface<RedirectHeuristicFlags> {
 public:
  RedirectHeuristicGrantTest() {
    std::string grant_time_string =
        GetParam().write_redirect_grants ? "60s" : "0s";
    std::string require_aba_flow_string =
        GetParam().require_aba_flow ? "true" : "false";
    std::string require_current_interaction_string =
        GetParam().require_current_interaction ? "true" : "false";

    enabled_features_.push_back(
        {content_settings::features::kTpcdHeuristicsGrants,
         {{"TpcdReadHeuristicsGrants", "true"},
          {"TpcdWriteRedirectHeuristicGrants", grant_time_string},
          {"TpcdRedirectHeuristicRequireABAFlow", require_aba_flow_string},
          {"TpcdRedirectHeuristicRequireCurrentInteraction",
           require_current_interaction_string}}});

    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    disabled_features_.push_back(features::kHttpsUpgrades);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features_,
                                                       disabled_features_);
    RedirectHeuristicBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    RedirectHeuristicBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
};

IN_PROC_BROWSER_TEST_P(RedirectHeuristicGrantTest,
                       CreatesRedirectHeuristicGrantsWithSatisfyingURL) {
  WebContents* web_contents = GetActiveWebContents();
  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  // Initialize first party URL and two trackers.
  GURL first_party_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL aba_current_interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL no_interaction_url =
      embedded_test_server()->GetURL("c.test", "/title1.html");

  // Start on `first_party_url`.
  ASSERT_TRUE(content::NavigateToURL(web_contents, first_party_url));

  // Navigate to `aba_current_interaction_url` and record a current interaction.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents, aba_current_interaction_url));
  SimulateMouseClick();

  // Redirect through `first_party_url`, `aba_current_interaction_url`, and
  // `no_interaction_url` before committing and ending on `first_party_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_party_url));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, aba_current_interaction_url));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, no_interaction_url));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_party_url));
  EndRedirectChain();

  // Wait on async tasks for the grants to be created.
  WaitOnStorage(GetDipsService(web_contents));

  // Expect some cookie grants on `first_party_url` based on flags and criteria.
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                aba_current_interaction_url, net::SiteForCookies(),
                first_party_url, net::CookieSettingOverrides(), nullptr),
            GetParam().write_redirect_grants ? CONTENT_SETTING_ALLOW
                                             : CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                no_interaction_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_P(
    RedirectHeuristicGrantTest,
    CreatesRedirectHeuristicGrantsWithPartiallySatisfyingURL) {
  WebContents* web_contents = GetActiveWebContents();
  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  // Initialize first party URL and two trackers.
  GURL first_party_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL aba_past_interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL no_aba_current_interaction_url =
      embedded_test_server()->GetURL("c.test", "/title1.html");

  // Record a past interaction on `aba_past_interaction_url`.
  ASSERT_TRUE(content::NavigateToURL(web_contents, aba_past_interaction_url));
  SimulateMouseClick();

  // Start redirect chain on `no_aba_current_interaction_url` and record a
  // current interaction.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents, no_aba_current_interaction_url));
  SimulateMouseClick();

  // Redirect through `no_aba_current_interaction_url`, `first_party_url`, and
  // `aba_past_interaction_url` before committing and ending on
  // `first_party_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, no_aba_current_interaction_url));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_party_url));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, aba_past_interaction_url));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_party_url));
  EndRedirectChain();

  // Wait on async tasks for the grants to be created.
  WaitOnStorage(GetDipsService(web_contents));

  // Expect some cookie grants on `first_party_url` based on flags and criteria.
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                aba_past_interaction_url, net::SiteForCookies(),
                first_party_url, net::CookieSettingOverrides(), nullptr),
            (GetParam().write_redirect_grants &&
             !GetParam().require_current_interaction)
                ? CONTENT_SETTING_ALLOW
                : CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                no_aba_current_interaction_url, net::SiteForCookies(),
                first_party_url, net::CookieSettingOverrides(), nullptr),
            (GetParam().write_redirect_grants && !GetParam().require_aba_flow)
                ? CONTENT_SETTING_ALLOW
                : CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       RedirectInfoHttpStatusPersistence) {
  WebContents* const web_contents = GetActiveWebContents();

  // The "final" URL will not have any server redirects.
  GURL final_url = embedded_test_server()->GetURL("/echo");
  // The "302" and "303" URLs will have a server redirect to the final URL,
  // giving a 302 and 303 HTTP response code status, respectively.
  GURL redirect_303 = embedded_test_server()->GetURL("/server-redirect-303?" +
                                                     final_url.spec());
  GURL redirect_302 = embedded_test_server()->GetURL("/server-redirect-302?" +
                                                     final_url.spec());
  // The "301" URL will give a 301 response code and redirect to the "302" URL.
  GURL redirect_301 = embedded_test_server()->GetURL("/server-redirect-301?" +
                                                     redirect_302.spec());

  // Navigate to a URL that will give a 301 redirect to another URL that will
  // give a 302 redirect, before settling on a third URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_301, final_url));

  // Do client redirect to a URL that gives a 303 redirect.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, redirect_303, final_url));

  RedirectChainDetector* wco =
      RedirectChainDetector::FromWebContents(web_contents);
  const DIPSRedirectContext& context = wco->CommittedRedirectContext();

  ASSERT_EQ(context.size(), 4u);

  EXPECT_EQ(context.AtForTesting(0).response_code, 301);
  EXPECT_EQ(context.AtForTesting(1).response_code, 302);
  // The client redirect does not have an explicit HTTP response status.
  EXPECT_EQ(context.AtForTesting(2).response_code, 0);
  EXPECT_EQ(context.AtForTesting(3).response_code, 303);
}

const RedirectHeuristicFlags kRedirectHeuristicTestCases[] = {
    {
        .write_redirect_grants = false,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = true,
        .require_current_interaction = true,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = false,
        .require_current_interaction = true,
    },
    {
        .write_redirect_grants = true,
        .require_aba_flow = true,
        .require_current_interaction = false,
    },
};

INSTANTIATE_TEST_SUITE_P(All,
                         RedirectHeuristicGrantTest,
                         ::testing::ValuesIn(kRedirectHeuristicTestCases));
#endif  // !BUILDFLAG(IS_ANDROID)

class DIPSBounceTrackingDevToolsIssueTest
    : public content::TestDevToolsProtocolClient,
      public DIPSBounceDetectorBrowserTest {
 protected:
  void WaitForIssueAndCheckTrackingSites(
      const std::vector<std::string>& sites) {
    auto is_dips_issue = [](const base::Value::Dict& params) {
      return *(params.FindStringByDottedPath("issue.code")) ==
             "BounceTrackingIssue";
    };

    // Wait for notification of a Bounce Tracking Issue.
    base::Value::Dict params = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(is_dips_issue));
    ASSERT_EQ(*params.FindStringByDottedPath("issue.code"),
              "BounceTrackingIssue");

    base::Value::Dict* bounce_tracking_issue_details =
        params.FindDictByDottedPath("issue.details.bounceTrackingIssueDetails");
    ASSERT_TRUE(bounce_tracking_issue_details);

    std::vector<std::string> tracking_sites;
    base::Value::List* tracking_sites_list =
        bounce_tracking_issue_details->FindList("trackingSites");
    if (tracking_sites_list) {
      for (const auto& val : *tracking_sites_list) {
        tracking_sites.push_back(val.GetString());
      }
    }

    // Verify the reported tracking sites match the expected sites.
    EXPECT_THAT(tracking_sites, testing::ElementsAreArray(sites));

    // Clear existing notifications so subsequent calls don't fail by checking
    // `sites` against old notifications.
    ClearNotifications();
  }

  void TearDownOnMainThread() override {
    DetachProtocolClient();
    DIPSBounceDetectorBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(DIPSBounceTrackingDevToolsIssueTest,
                       BounceTrackingDevToolsIssue) {
  WebContents* web_contents = GetActiveWebContents();

  // Visit initial page on a.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Open DevTools and enable Audit domain.
  AttachToWebContents(web_contents);
  SendCommandSync("Audits.enable");
  ClearNotifications();

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));
  WaitForIssueAndCheckTrackingSites({"b.test"});

  // Write a cookie via JS on c.test.
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver cookie_observer(web_contents, frame,
                                            CookieOperation::kChange);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));
  WaitForIssueAndCheckTrackingSites({"c.test"});

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test, which S-redirects to g.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL(
          "e.test",
          "/cross-site-with-cookie/f.test/cross-site-with-cookie/g.test/"
          "title1.html"),
      embedded_test_server()->GetURL("g.test", "/title1.html")));
  // Note d.test is not listed as a potentially tracking site since it did not
  // write cookies before bouncing the user.
  WaitForIssueAndCheckTrackingSites({"e.test", "f.test"});
}

class DIPSSiteDataAccessDetectorTest
    : public DIPSBounceDetectorBrowserTest,
      public testing::WithParamInterface<StorageTypeAccessed> {
 public:
  DIPSSiteDataAccessDetectorTest(const DIPSSiteDataAccessDetectorTest&) =
      delete;
  DIPSSiteDataAccessDetectorTest& operator=(
      const DIPSSiteDataAccessDetectorTest&) = delete;

  DIPSSiteDataAccessDetectorTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(kChromeTestDataDir);
    ASSERT_TRUE(https_server_.Start());
    SetUpDIPSWebContentsObserver();
  }

  auto* TestServer() { return &https_server_; }

 private:
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(DIPSSiteDataAccessDetectorTest,
                       DetectSiteDataAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      GetActiveWebContents()->GetPrimaryMainFrame(),
      TestServer()->GetURL("a.test", "/title1.html")));

  EXPECT_TRUE(
      AccessStorage(GetActiveWebContents()->GetPrimaryMainFrame(), GetParam()));

  EXPECT_THAT(
      logger->log(),
      testing::ContainerEq(std::vector<std::string>(
          {"DidStartNavigation(a.test/title1.html)",
           "DidFinishNavigation(a.test/title1.html)",
           base::StringPrintf("NotifyStorageAccessed(%s: a.test/title1.html)",
                              base::ToString(GetParam()).c_str())})));
}

IN_PROC_BROWSER_TEST_P(DIPSSiteDataAccessDetectorTest,
                       AttributeSameSiteIframesSiteDataAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      TestServer()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url = TestServer()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  EXPECT_TRUE(AccessStorage(GetIFrame(), GetParam()));

  const GURL primary_main_frame_final_url =
      TestServer()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_P(DIPSSiteDataAccessDetectorTest,
                       AttributeSameSiteNestedIframesSiteDataAccessTo1P) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      TestServer()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL iframe_url = TestServer()->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));

  const GURL nested_iframe_url = TestServer()->GetURL("a.test", "/title1.html");
  NavigateNestedIFrameTo(GetIFrame(), "test", nested_iframe_url);

  EXPECT_TRUE(AccessStorage(GetNestedIFrame(), GetParam()));

  const GURL primary_main_frame_final_url =
      TestServer()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(redirects, ElementsAre(("[1/1] blank -> a.test/iframe_blank.html "
                                      "(Write) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_P(DIPSSiteDataAccessDetectorTest,
                       DiscardFencedFrameCookieClientAccess) {
  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      TestServer()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL fenced_frame_url =
      TestServer()->GetURL("a.test", "/fenced_frames/title2.html");
  std::unique_ptr<content::RenderFrameHostWrapper> fenced_frame =
      std::make_unique<content::RenderFrameHostWrapper>(
          fenced_frame_test_helper()->CreateFencedFrame(
              GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url));
  EXPECT_NE(fenced_frame, nullptr);

  EXPECT_TRUE(AccessStorage(fenced_frame->get(), GetParam()));

  const GURL primary_main_frame_final_url =
      TestServer()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

IN_PROC_BROWSER_TEST_P(DIPSSiteDataAccessDetectorTest,
                       DiscardPrerenderedPageCookieClientAccess) {
  // Prerendering pages do not have access to `StorageTypeAccessed::kFileSystem`
  // until activation (AKA becoming the primary page, whose test case is already
  // covered).
  if (GetParam() == StorageTypeAccessed::kFileSystem) {
    GTEST_SKIP();
  }

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      TestServer()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url = TestServer()->GetURL("a.test", "/title2.html");
  const content::FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  content::test::PrerenderHostObserver observer(*GetActiveWebContents(),
                                                host_id);
  EXPECT_FALSE(observer.was_activated());
  content::RenderFrameHost* prerender_frame =
      prerender_test_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);

  EXPECT_TRUE(AccessStorage(prerender_frame, GetParam()));

  prerender_test_helper()->CancelPrerenderedPage(host_id);
  observer.WaitForDestroyed();

  const GURL primary_main_frame_final_url =
      TestServer()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `primary_main_frame_final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), primary_main_frame_final_url));

  CloseTab(GetActiveWebContents());
  EXPECT_THAT(
      redirects,
      ElementsAre(
          ("[1/1] blank -> a.test/title1.html (None) -> d.test/title1.html")));
}

// WeLocks accesses aren't monitored by the `PageSpecificContentSettings` as
// they are not persistent.
// TODO(crbug.com/40269763): Remove `StorageTypeAccessed::kFileSystem` once
// deprecation is complete.
INSTANTIATE_TEST_SUITE_P(All,
                         DIPSSiteDataAccessDetectorTest,
                         ::testing::Values(StorageTypeAccessed::kLocalStorage,
                                           StorageTypeAccessed::kSessionStorage,
                                           StorageTypeAccessed::kCacheStorage,
                                           StorageTypeAccessed::kFileSystem,
                                           StorageTypeAccessed::kIndexedDB));

// WebAuthn tests do not work on Android because there is no current way to
// install a virtual authenticator.
// NOTE: Manual testing was performed to ensure this implementation works as
// expected on Android platform.
// TODO(crbug.com/40269763): Implement automated testing once the infrastructure
// permits it (Requires mocking the Android Platform Authenticator i.e. GMS
// Core).
#if !BUILDFLAG(IS_ANDROID)
// Some refs for this test fixture:
// clang-format off
// - https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/webauthn/chrome_webauthn_browsertest.cc;drc=c4061a03f240338b42a5b84c98b1a11b62a97a9a
// - https://source.chromium.org/chromium/chromium/src/+/main:content/browser/webauth/webauth_browsertest.cc;drc=e8e4ad9096841fae7c55cea1b7d278c58f6160ff
// - https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/payments/secure_payment_confirmation_authenticator_browsertest.cc;drc=edea5c45c08d151afe67276f08a2ee13814563e1
// clang-format on
class DIPSWebAuthnBrowserTest : public CertVerifierBrowserTest {
 public:
  DIPSWebAuthnBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  DIPSWebAuthnBrowserTest(const DIPSWebAuthnBrowserTest&) = delete;
  DIPSWebAuthnBrowserTest& operator=(const DIPSWebAuthnBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();

    // Allowlist all certs for the HTTPS server.
    mock_cert_verifier()->set_default_result(net::OK);

    CertVerifierBrowserTest::host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    https_server_.RegisterDefaultHandler(base::BindRepeating(
        &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server_));
    ASSERT_TRUE(https_server_.Start());

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();

    virtual_device_factory->mutable_state()->InjectResidentKey(
        std::vector<uint8_t>{1, 2, 3, 4}, authn_hostname,
        std::vector<uint8_t>{5, 6, 7, 8}, "Foo", "Foo Bar");

    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));

    auth_env_ =
        std::make_unique<content::ScopedAuthenticatorEnvironmentForTesting>(
            std::move(virtual_device_factory));

    web_contents_observer_ =
        DIPSWebContentsObserver::FromWebContents(GetActiveWebContents());
    CHECK(web_contents_observer_);
  }

  void TearDownOnMainThread() override {
    CertVerifierBrowserTest::TearDownOnMainThread();
    web_contents_observer_ = nullptr;
  }

  void PostRunTestOnMainThread() override {
    auth_env_.reset();
    // web_contents_observer_.ClearAndDelete();
    CertVerifierBrowserTest::PostRunTestOnMainThread();
  }

  auto* TestServer() { return &https_server_; }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  RedirectChainDetector* GetRedirectChainHelper() {
    return RedirectChainDetector::FromWebContents(GetActiveWebContents());
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  void EndRedirectChain() {
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(),
                               TestServer()->GetURL("a.test", "/title1.html")));
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    GetRedirectChainHelper()->SetRedirectChainHandlerForTesting(
        base::BindRepeating(&AppendRedirects, redirects));
  }

  void StartAppendingReportsTo(std::vector<std::string>* reports) {
    web_contents_observer_->SetIssueReportingCallbackForTesting(
        base::BindRepeating(&AppendSitesInReport, reports));
  }

  void GetWebAuthnAssertion() {
    ASSERT_EQ("OK", content::EvalJs(GetActiveWebContents(), R"(
    let cred_id = new Uint8Array([1,2,3,4]);
    navigator.credentials.get({
      publicKey: {
        challenge: cred_id,
        userVerification: 'preferred',
        allowCredentials: [{
          type: 'public-key',
          id: cred_id,
          transports: ['usb', 'nfc', 'ble'],
        }],
        timeout: 10000
      }
    }).then(c => 'OK',
      e => e.toString());
  )",
                                    content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

 protected:
  const std::string authn_hostname = "b.test";

 private:
  net::EmbeddedTestServer https_server_;
  raw_ptr<DIPSWebContentsObserver> web_contents_observer_ = nullptr;
  std::unique_ptr<content::ScopedAuthenticatorEnvironmentForTesting> auth_env_;
};

IN_PROC_BROWSER_TEST_F(DIPSWebAuthnBrowserTest,
                       WebAuthnAssertion_ConfirmWCOCallback) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL initial_url = TestServer()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), initial_url));

  const GURL bounce_url = TestServer()->GetURL(authn_hostname, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), bounce_url));

  AccessCookieViaJSIn(GetActiveWebContents(),
                      GetActiveWebContents()->GetPrimaryMainFrame());

  GetWebAuthnAssertion();

  const GURL final_url = TestServer()->GetURL("d.test", "/title1.html");
  // Performs a Client-redirect to `final_url`.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(), final_url));

  EXPECT_THAT(
      logger->log(),
      testing::ElementsAre(
          "DidStartNavigation(a.test/title1.html)",
          "DidFinishNavigation(a.test/title1.html)",
          "DidStartNavigation(b.test/title1.html)",
          "DidFinishNavigation(b.test/title1.html)",
          "OnCookiesAccessed(RenderFrameHost, Change: b.test/title1.html)",
          "WebAuthnAssertionRequestSucceeded(b.test/title1.html)",
          "DidStartNavigation(d.test/title1.html)",
          "DidFinishNavigation(d.test/title1.html)"));

  EndRedirectChain();

  std::vector<std::string> expected_redirects;
  // NOTE: The bounce detection isn't impacted (is exonerated) at this point by
  // the web authn assertion.
  expected_redirects.push_back(
      "[1/1] a.test/title1.html -> b.test/title1.html (Write) -> "
      "d.test/title1.html");
  // NOTE: Due the favicon.ico temporally iffy callbacks we could expect the
  // following outcome to help avoid flakiness.
  expected_redirects.push_back(
      "[1/1] a.test/title1.html -> b.test/title1.html (ReadWrite) -> "
      "d.test/title1.html");

  EXPECT_THAT(expected_redirects, Contains(redirects.front()));
}

// This test verifies that sites in a redirect chain with previous web authn
// assertions are not reported in the resulting issue when a navigation
// finishes.
IN_PROC_BROWSER_TEST_F(
    DIPSWebAuthnBrowserTest,
    ReportRedirectorsInChain_OmitSitesWithWebAuthnAssertions) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> reports;
  StartAppendingReportsTo(&reports);

  // Visit initial page on a.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, TestServer()->GetURL("a.test", "/title1.html")));

  GURL url = TestServer()->GetURL(authn_hostname, "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURLFromRendererWithoutUserGesture(web_contents, url));

  GetWebAuthnAssertion();

  // Verify web authn assertion was recorded for `authn_hostname`, before
  // proceeding.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_TRUE(state.has_value());
  ASSERT_FALSE(state->user_interaction_times.has_value());
  ASSERT_TRUE(state->web_authn_assertion_times.has_value());

  // Navigate with a click (not a redirect) to d.test, which statefully
  // S-redirects to c.test and write a cookie on c.test.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      TestServer()->GetURL(
          "d.test", "/cross-site-with-samesite-none-cookie/c.test/title1.html"),
      TestServer()->GetURL("c.test", "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to `authn_hostname` and
  // write a cookie on `authn_hostname`:
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, TestServer()->GetURL(authn_hostname, "/title1.html")));
  AccessCookieViaJSIn(web_contents, web_contents->GetPrimaryMainFrame());

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // statefully S-redirects to f.test, which statefully S-redirects to g.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      TestServer()->GetURL("e.test",
                           "/cross-site-with-samesite-none-cookie/f.test/"
                           "cross-site-with-samesite-none-cookie/g.test/"
                           "title1.html"),
      TestServer()->GetURL("g.test", "/title1.html")));

  EndRedirectChain();
  WaitOnStorage(GetDipsService(web_contents));

  EXPECT_THAT(reports, ElementsAre(("d.test"), ("c.test"), ("e.test, f.test")));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verifies that a successfully registered service worker is tracked as a
// storage access.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       ServiceWorkerAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  // Navigate to URL to set service workers. This will result in a service
  // worker access from the RenderFrameHost.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL(
          "/service_worker/create_service_worker.html")));

  // Register a service worker on the current page, and await its completion.
  ASSERT_EQ(true, content::EvalJs(GetActiveWebContents(), R"(
    (async () => {
      await navigator.serviceWorker.register('/service_worker/empty.js');
      await navigator.serviceWorker.ready;
      return true;
    })();
  )"));

  // Navigate away from and back to the URL in scope of the registered service
  // worker. This will result in a service worker access from the
  // NavigationHandle.
  ASSERT_TRUE(NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("/service_worker/blank.html")));
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL(
          "/service_worker/create_service_worker.html")));

  // Validate that the expected callbacks to WebContentsObserver were made.
  EXPECT_THAT(logger->log(),
              testing::IsSupersetOf({"OnServiceWorkerAccessed(RenderFrameHost: "
                                     "127.0.0.1/service_worker/)",
                                     "OnServiceWorkerAccessed(NavigationHandle:"
                                     " 127.0.0.1/service_worker/)"}));
}

// TODO(crbug.com/40290702): Shared workers are not available on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SharedWorkerAccess_Storages DISABLED_SharedWorkerAccess_Storages
#else
#define MAYBE_SharedWorkerAccess_Storages SharedWorkerAccess_Storages
#endif
// Verifies that adding a shared worker to a frame is tracked as a storage
// access.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       MAYBE_SharedWorkerAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  // Add the WCOCallbackLogger as an observer of SharedWorkerService events.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetSharedWorkerService()
      ->AddObserver(logger);

  // Navigate to URL for shared worker.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL(
          "a.test", "/private_network_access/no-favicon.html")));

  // Create and start a shared worker on the current page.
  ASSERT_EQ(true, content::EvalJs(GetActiveWebContents(),
                                  content::JsReplace(
                                      R"(
    (async () => {
      const worker = await new Promise((resolve, reject) => {
        const worker =
            new SharedWorker("/workers/shared_fetcher_treat_as_public.js");
        worker.port.addEventListener("message", () => resolve(worker));
        worker.addEventListener("error", reject);
        worker.port.start();
      });

      const messagePromise = new Promise((resolve) => {
        const listener = (event) => resolve(event.data);
        worker.port.addEventListener("message", listener, { once: true });
      });

      worker.port.postMessage($1);

      const { error, ok } = await messagePromise;
      if (error !== undefined) {
        throw(error);
      }

      return ok;
    })();
  )",
                                      embedded_test_server()->GetURL(
                                          "b.test", "/cors-ok.txt"))));

  // Validate that the expected callback to SharedWorkerService.Observer was
  // made.
  EXPECT_THAT(logger->log(),
              testing::Contains("OnSharedWorkerClientAdded(a.test/"
                                "private_network_access/no-favicon.html)"));

  // Clean up the observer to avoid a dangling ptr.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetSharedWorkerService()
      ->RemoveObserver(logger);
}

// Verifies that adding a dedicated worker to a frame is tracked as a storage
// access.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DedicatedWorkerAccess_Storages) {
  // Start logging `WebContentsObserver` callbacks.
  WCOCallbackLogger::CreateForWebContents(GetActiveWebContents());
  auto* logger = WCOCallbackLogger::FromWebContents(GetActiveWebContents());

  // Add the WCOCallbackLogger as an observer of DedicatedWorkerService events.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetDedicatedWorkerService()
      ->AddObserver(logger);

  // Navigate to URL for dedicated worker.
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(),
      embedded_test_server()->GetURL(
          "a.test", "/private_network_access/no-favicon.html")));

  // Create and start a dedicated worker on the current page.
  ASSERT_EQ(true, content::EvalJs(GetActiveWebContents(),
                                  content::JsReplace(
                                      R"(
    (async () => {
      const worker = new Worker("/workers/fetcher_treat_as_public.js");

      const messagePromise = new Promise((resolve) => {
        const listener = (event) => resolve(event.data);
        worker.addEventListener("message", listener, { once: true });
      });

      worker.postMessage($1);

      const { error, ok } = await messagePromise;
      if (error !== undefined) {
        throw(error);
      }

      return ok;
    })();
  )",
                                      embedded_test_server()->GetURL(
                                          "b.test", "/cors-ok.txt"))));

  // Validate that the expected callback to DedicatedWorkerService.Observer was
  // made.
  EXPECT_THAT(logger->log(),
              testing::Contains("OnDedicatedWorkerCreated(a.test/"
                                "private_network_access/no-favicon.html)"));

  // Clean up the observer to avoid a dangling ptr.
  GetActiveWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetDedicatedWorkerService()
      ->RemoveObserver(logger);
}

// Tests that currently only work consistently when the trigger is (any) bounce.
// TODO(crbug.com/336161248) Make these tests use stateful bounces.
class DIPSBounceTriggerBrowserTest : public DIPSBounceDetectorBrowserTest {
 protected:
  DIPSBounceTriggerBrowserTest() {
    enabled_features_.push_back(
        {features::kDIPS, {{"triggering_action", "bounce"}}});
  }

  void SetUpOnMainThread() override {
    DIPSBounceDetectorBrowserTest::SetUpOnMainThread();
    // DIPS will only record bounces if 3PCs are blocked.
    chrome_test_utils::GetProfile(this)->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }
};

// Verifies that a HTTP 204 (No Content) response is treated like a bounce.
IN_PROC_BROWSER_TEST_F(DIPSBounceTriggerBrowserTest, NoContent) {
  content::WebContents* web_contents = GetActiveWebContents();

  GURL committed_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, committed_url));

  RedirectChainObserver observer(
      DIPSService::Get(web_contents->GetBrowserContext()), committed_url);
  GURL nocontent_url = embedded_test_server()->GetURL("b.test", "/nocontent");
  ASSERT_TRUE(
      content::NavigateToURL(web_contents, nocontent_url, committed_url));
  observer.Wait();

  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));
}

class DIPSThrottlingBrowserTest : public DIPSBounceDetectorBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DIPSBounceDetectorBrowserTest::SetUpOnMainThread();
    DIPSWebContentsObserver::FromWebContents(GetActiveWebContents())
        ->SetClockForTesting(&test_clock_);
  }

  base::SimpleTestClock test_clock_;
};

IN_PROC_BROWSER_TEST_F(DIPSThrottlingBrowserTest,
                       InteractionRecording_Throttled) {
  WebContents* web_contents = GetActiveWebContents();
  const base::Time start_time = test_clock_.Now();

  // Record user activation on a.test.
  const GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  SimulateMouseClick();
  // Verify the interaction was recorded in the DIPS DB.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->user_interaction_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Click again, just before kDIPSTimestampUpdateInterval elapses.
  test_clock_.Advance(kDIPSTimestampUpdateInterval - base::Seconds(1));
  SimulateMouseClick();
  // Verify the second interaction was NOT recorded, due to throttling.
  state = GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->user_interaction_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Click a third time, after kDIPSTimestampUpdateInterval has passed since the
  // first click.
  test_clock_.Advance(base::Seconds(1));
  SimulateMouseClick();
  // Verify the third interaction WAS recorded.
  state = GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->user_interaction_times,
              testing::Optional(testing::Pair(
                  start_time, start_time + kDIPSTimestampUpdateInterval)));
}

IN_PROC_BROWSER_TEST_F(DIPSThrottlingBrowserTest,
                       InteractionRecording_NotThrottled_AfterRefresh) {
  WebContents* web_contents = GetActiveWebContents();
  const base::Time start_time = test_clock_.Now();

  // Record user activation on a.test.
  const GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  SimulateMouseClick();
  // Verify the interaction was recorded in the DIPS DB.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->user_interaction_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Navigate to a new page and click, only a second after the previous click.
  test_clock_.Advance(base::Seconds(1));
  const GURL url2 = embedded_test_server()->GetURL("b.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url2));
  SimulateMouseClick();
  // Verify the second interaction was also recorded (not throttled).
  state = GetDIPSState(GetDipsService(web_contents), url2);
  ASSERT_THAT(state->user_interaction_times,
              testing::Optional(testing::Pair(start_time + base::Seconds(1),
                                              start_time + base::Seconds(1))));
}

// TODO(b/325196134): Re-enable the test.
IN_PROC_BROWSER_TEST_F(DIPSThrottlingBrowserTest,
                       DISABLED_StorageRecording_Throttled) {
  WebContents* web_contents = GetActiveWebContents();
  const base::Time start_time = test_clock_.Now();

  // Record client-side storage access on a.test.
  const GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  SimulateCookieWrite();
  // Verify the write was recorded in the DIPS DB.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->site_storage_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Write a cookie again, just before kDIPSTimestampUpdateInterval elapses.
  test_clock_.Advance(kDIPSTimestampUpdateInterval - base::Seconds(1));
  SimulateCookieWrite();
  // Verify the second write was NOT recorded, due to throttling.
  state = GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->site_storage_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Write a third time, after kDIPSTimestampUpdateInterval has passed since the
  // first write.
  test_clock_.Advance(base::Seconds(1));
  SimulateCookieWrite();
  // Verify the third write WAS recorded.
  state = GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->site_storage_times,
              testing::Optional(testing::Pair(
                  start_time, start_time + kDIPSTimestampUpdateInterval)));
}

// TODO(b/325196134): Re-enable the test.
IN_PROC_BROWSER_TEST_F(DIPSThrottlingBrowserTest,
                       DISABLED_StorageRecording_NotThrottled_AfterRefresh) {
  WebContents* web_contents = GetActiveWebContents();
  const base::Time start_time = test_clock_.Now();

  // Record client-side storage access on a.test.
  const GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  SimulateCookieWrite();
  // Verify the write was recorded in the DIPS DB.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), url);
  ASSERT_THAT(state->site_storage_times,
              testing::Optional(testing::Pair(start_time, start_time)));

  // Navigate to a new page and write cookies again, only a second after the
  // previous write.
  test_clock_.Advance(base::Seconds(1));
  const GURL url2 = embedded_test_server()->GetURL("b.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url2));
  SimulateCookieWrite();
  // Verify the second write was also recorded (not throttled).
  state = GetDIPSState(GetDipsService(web_contents), url2);
  ASSERT_THAT(state->site_storage_times,
              testing::Optional(testing::Pair(start_time + base::Seconds(1),
                                              start_time + base::Seconds(1))));
}

class AllSitesFollowingFirstPartyTest : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    first_party_url_ = embedded_test_server()->GetURL("a.test", "/title1.html");
    third_party_url_ = embedded_test_server()->GetURL("b.test", "/title1.html");
    other_url_ = embedded_test_server()->GetURL("c.test", "/title1.html");
  }

  WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 protected:
  GURL first_party_url_;
  GURL third_party_url_;
  GURL other_url_;
};

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       SiteFollowingFirstPartyIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::ElementsAre(GetSiteForDIPS(third_party_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       SiteNotFollowingFirstPartyNotIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::ElementsAre(GetSiteForDIPS(third_party_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest, MultipleSitesIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::ElementsAre(GetSiteForDIPS(third_party_url_),
                                   GetSiteForDIPS(other_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       NoFirstParty_NothingIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       NothingAfterFirstParty_NothingIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));

  EXPECT_THAT(RedirectHeuristicTabHelper::AllSitesFollowingFirstParty(
                  GetActiveWebContents(), first_party_url_),
              testing::IsEmpty());
}

class DIPSPrivacySandboxApiInteractionTest : public PlatformBrowserTest {
 public:
  DIPSPrivacySandboxApiInteractionTest()
      : embedded_https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back({features::kPrivacySandboxAdsAPIsOverride, {}});
    enabled_features.push_back(
        {features::kDIPS, {{"triggering_action", "stateful_bounce"}}});
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUpOnMainThread() override {
    // Enable Privacy Sandbox APIs on all sites.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    RegisterTrustTokenTestHandler(&trust_token_request_handler_);
    embedded_https_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server_.Start());
    chrome_test_utils::GetProfile(this)->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void EndRedirectChain() {
    WebContents* web_contents = GetActiveWebContents();
    DIPSService* dips_service = GetDipsService(web_contents);
    GURL expected_url = web_contents->GetLastCommittedURL();

    RedirectChainObserver chain_observer(dips_service, expected_url);
    // Performing a browser-based navigation terminates the current redirect
    // chain.
    ASSERT_TRUE(content::NavigateToURL(
        web_contents, embedded_https_test_server_.GetURL("end-the-chain.d.test",
                                                         "/title1.html")));
    chain_observer.Wait();
  }

  base::expected<std::vector<url::Origin>, std::string>
  WaitForInterestGroupData() {
    WebContents* web_contents = GetActiveWebContents();
    content::InterestGroupManager* interest_group_manager =
        web_contents->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetInterestGroupManager();
    if (!interest_group_manager) {
      return base::unexpected("null interest group manager");
    }
    // Poll until data appears, failing if action_timeout() passes
    base::Time deadline = base::Time::Now() + TestTimeouts::action_timeout();
    while (base::Time::Now() < deadline) {
      base::test::TestFuture<std::vector<url::Origin>> future;
      interest_group_manager->GetAllInterestGroupJoiningOrigins(
          future.GetCallback());
      std::vector<url::Origin> data = future.Get();
      if (!data.empty()) {
        return data;
      }
      Sleep(TestTimeouts::tiny_timeout());
    }
    return base::unexpected("timed out waiting for interest group data");
  }

  base::expected<AttributionData, std::string> WaitForAttributionData() {
    WebContents* web_contents = GetActiveWebContents();
    content::AttributionDataModel* model = web_contents->GetBrowserContext()
                                               ->GetDefaultStoragePartition()
                                               ->GetAttributionDataModel();
    if (!model) {
      return base::unexpected("null attribution data model");
    }
    // Poll until data appears, failing if action_timeout() passes
    base::Time deadline = base::Time::Now() + TestTimeouts::action_timeout();
    while (base::Time::Now() < deadline) {
      base::test::TestFuture<AttributionData> future;
      model->GetAllDataKeys(future.GetCallback());
      AttributionData data = future.Get();
      if (!data.empty()) {
        return data;
      }
      Sleep(TestTimeouts::tiny_timeout());
    }
    return base::unexpected("timed out waiting for attribution data");
  }

  void ProvideRequestHandlerKeyCommitmentsToNetworkService(
      std::vector<std::string_view> hosts) {
    base::flat_map<url::Origin, std::string_view> origins_and_commitments;
    std::string key_commitments =
        trust_token_request_handler_.GetKeyCommitmentRecord();

    for (std::string_view host : hosts) {
      origins_and_commitments.insert_or_assign(
          embedded_https_test_server_.GetOrigin(std::string(host)),
          key_commitments);
    }

    if (origins_and_commitments.empty()) {
      origins_and_commitments = {
          {embedded_https_test_server_.GetOrigin(), key_commitments}};
    }

    base::RunLoop run_loop;
    content::GetNetworkService()->SetTrustTokenKeyCommitments(
        network::WrapKeyCommitmentsForIssuers(
            std::move(origins_and_commitments)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // TODO: crbug.com/1509946 - When embedded_https_test_server() is added to
  // AndroidBrowserTest, switch to using
  // PlatformBrowserTest::embedded_https_test_server() and delete this.
  net::EmbeddedTestServer embedded_https_test_server_;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  static void Sleep(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

  void RegisterTrustTokenTestHandler(
      network::test::TrustTokenRequestHandler* handler) {
    embedded_https_test_server_.RegisterRequestHandler(
        base::BindLambdaForTesting(
            [handler, this](const net::test_server::HttpRequest& request)
                -> std::unique_ptr<net::test_server::HttpResponse> {
              if (request.relative_url != "/issue") {
                return nullptr;
              }
              if (!base::Contains(request.headers, "Sec-Private-State-Token") ||
                  !base::Contains(request.headers,
                                  "Sec-Private-State-Token-Crypto-Version")) {
                return MakeTrustTokenFailureResponse();
              }

              std::optional<std::string> operation_result =
                  handler->Issue(request.headers.at("Sec-Private-State-Token"));

              if (!operation_result) {
                return MakeTrustTokenFailureResponse();
              }

              return MakeTrustTokenResponse(*operation_result);
            }));
  }

  std::unique_ptr<net::test_server::HttpResponse>
  MakeTrustTokenFailureResponse() {
    // No need to report a failure HTTP code here: returning a vanilla OK should
    // fail the Trust Tokens operation client-side.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  // Constructs and returns an HTTP response bearing the given base64-encoded
  // Trust Tokens issuance or redemption protocol response message.
  std::unique_ptr<net::test_server::HttpResponse> MakeTrustTokenResponse(
      std::string_view contents) {
    CHECK([&]() {
      std::string temp;
      return base::Base64Decode(contents, &temp);
    }());

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Sec-Private-State-Token", std::string(contents));
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  network::test::TrustTokenRequestHandler trust_token_request_handler_;
};

// Verify that accessing storage via the PAT Protected Audience API doesn't
// trigger DIPS deletion for the accessing site.
IN_PROC_BROWSER_TEST_F(DIPSPrivacySandboxApiInteractionTest,
                       DontTriggerDeletionOnProtectedAudienceApiStorageAccess) {
  WebContents* web_contents = GetActiveWebContents();
  // Enable Privacy Sandbox APIs in the current profile.
  PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->SetAllPrivacySandboxAllowedForTesting();

  const char* source_host = "source.a.test";
  const char* pat_using_host = "pat.b.test";

  // Write a secure cookie for PAT-using site, to represent site data written
  // through non-DIPS-triggering means.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &embedded_https_test_server_,
                                  pat_using_host, true, false));

  // Visit source site.
  GURL source_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, source_url));

  // Navigate from source site to PAT-using site.
  GURL bounce_url =
      embedded_https_test_server_.GetURL(pat_using_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Have PAT-using site perform an interest groups API action that accesses
  // storage, without accessing storage in any other way.
  ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(),
                              content::JsReplace(R"(
                                (async () => {
                                  const pageOrigin = new URL($1).origin;
                                  const interestGroup = {
                                    name: "exampleInterestGroup",
                                    owner: pageOrigin,
                                  };

                                  await navigator.joinAdInterestGroup(
                                      interestGroup,
                                      // Pick an arbitrarily high duration to
                                      // guarantee that we never leave the ad
                                      // interest group while the test runs.
                                      /*durationSeconds=*/3000000);
                                })();
                              )",
                                                 bounce_url),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for interest group data to be written to storage.
  ASSERT_OK_AND_ASSIGN(std::vector<url::Origin> interest_group_joining_origins,
                       WaitForInterestGroupData());
  ASSERT_THAT(interest_group_joining_origins,
              ElementsAre(url::Origin::Create(bounce_url)));

  // Have the PAT-using site client-side-redirect back to the source site and
  // end the redirect chain.
  GURL bounce_back_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html?unique");
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, bounce_back_url));
  EndRedirectChain();

  // Expect DIPS to not have recorded user interaction.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), bounce_url);
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times, std::nullopt);

  // Expect DIPS to have classified the bounce to the PAT-using site as
  // stateless (i.e., to have recorded a bounce, but no stateful bounce).
  EXPECT_EQ(state->stateful_bounce_times, std::nullopt);
  EXPECT_TRUE(state->bounce_times.has_value());

  // Trigger DIPS deletion, and expect DIPS to not have deleted data for the
  // PAT-using site.
  DIPSService* dips = GetDipsService(web_contents);
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  dips->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  EXPECT_THAT(deleted_sites.Get(), IsEmpty());

  // Make sure that the cookie we wrote for the PAT-using site is still there.
  EXPECT_EQ(content::GetCookies(web_contents->GetBrowserContext(), bounce_url),
            "name=value");
}

// Verify that accessing storage via the PAT Attribution Reporting API doesn't
// trigger DIPS deletion for the accessing site.
IN_PROC_BROWSER_TEST_F(
    DIPSPrivacySandboxApiInteractionTest,
    DontTriggerDeletionOnAttributionReportingApiStorageAccess) {
  WebContents* web_contents = GetActiveWebContents();
  // Enable Privacy Sandbox APIs in the current profile.
  PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->SetAllPrivacySandboxAllowedForTesting();

  const char* source_host = "source.a.test";
  const char* pat_using_host = "pat.b.test";
  const char* attribution_host = "attribution.c.test";

  // Write a secure cookie for PAT-using site, to represent site data written
  // through non-DIPS-triggering means.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &embedded_https_test_server_,
                                  pat_using_host, true, false));

  // Visit source site.
  GURL source_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, source_url));

  // Navigate from source site to PAT-using site.
  GURL bounce_url =
      embedded_https_test_server_.GetURL(pat_using_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Have PAT-using site perform an attribution reporting action that accesses
  // storage, without accessing storage in any other way.
  GURL attribution_url = embedded_https_test_server_.GetURL(
      attribution_host, "/attribution_reporting/register_source_headers.html");
  ASSERT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(
                                  R"(
                                  let img = document.createElement('img');
                                  img.attributionSrc = $1;
                                  document.body.appendChild(img);)",
                                  attribution_url),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Wait for attribution data to be written to storage.
  ASSERT_OK_AND_ASSIGN(AttributionData data, WaitForAttributionData());
  ASSERT_THAT(GetOrigins(data),
              ElementsAre(url::Origin::Create(attribution_url)));

  // Have the PAT-using site client-side-redirect back to the source site and
  // end the redirect chain.
  GURL bounce_back_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html?unique");
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, bounce_back_url));
  EndRedirectChain();

  // Expect DIPS to not have recorded user interaction.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), bounce_url);
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times, std::nullopt);

  // Expect DIPS to have classified the bounce to the PAT-using site as
  // stateless (= to have recorded a bounce but no stateful bounce).
  EXPECT_EQ(state->stateful_bounce_times, std::nullopt);
  EXPECT_TRUE(state->bounce_times.has_value());

  // Trigger DIPS deletion, and expect DIPS to not have deleted data for the
  // PAT-using site.
  DIPSService* dips = GetDipsService(web_contents);
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  dips->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  EXPECT_THAT(deleted_sites.Get(), IsEmpty());

  // Make sure that the cookie we wrote for the PAT-using site is still there.
  EXPECT_EQ(content::GetCookies(web_contents->GetBrowserContext(), bounce_url),
            "name=value");
}

// Verify that accessing storage via the PAT Private State Tokens API doesn't
// trigger DIPS deletion for the accessing site.
IN_PROC_BROWSER_TEST_F(
    DIPSPrivacySandboxApiInteractionTest,
    DontTriggerDeletionOnPrivateStateTokensApiStorageAccess) {
  WebContents* web_contents = GetActiveWebContents();
  // Enable Privacy Sandbox APIs in the current profile.
  PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->SetAllPrivacySandboxAllowedForTesting();

  const char* source_host = "source.a.test";
  const char* pat_using_host = "pat.b.test";
  ProvideRequestHandlerKeyCommitmentsToNetworkService({pat_using_host});

  // Write a secure cookie for PAT-using site, to represent site data written
  // through non-DIPS-triggering means.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &embedded_https_test_server_,
                                  pat_using_host, true, false));

  // Visit source site.
  GURL source_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, source_url));

  // Navigate from source site to PAT-using site.
  GURL bounce_url =
      embedded_https_test_server_.GetURL(pat_using_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Have PAT-using site perform a Private State Tokens API action that accesses
  // storage, without accessing storage in any other way, and wait for the
  // private state token to be written to storage.
  const std::string pat_using_site_origin =
      embedded_https_test_server_.GetOrigin(pat_using_host).Serialize();
  ASSERT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(
                                  R"(
                                    (async () => {
                                      await fetch("/issue", {
                                        privateToken: {
                                          operation: "token-request",
                                          version: 1
                                        }
                                      });
                                      return await document.hasPrivateToken($1);
                                    })();
                                  )",
                                  pat_using_site_origin),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Have the PAT-using site client-side-redirect back to the source site and
  // end the redirect chain.
  GURL bounce_back_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html?unique");
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, bounce_back_url));
  EndRedirectChain();

  // Expect DIPS to not have recorded user interaction.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), bounce_url);
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times, std::nullopt);

  // Expect DIPS to have classified the bounce to the PAT-using site as
  // stateless (= to have recorded a bounce but no stateful bounce).
  EXPECT_EQ(state->stateful_bounce_times, std::nullopt);
  EXPECT_TRUE(state->bounce_times.has_value());

  // Trigger DIPS deletion, and expect DIPS to not have deleted data for the
  // PAT-using site.
  DIPSService* dips = GetDipsService(web_contents);
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  dips->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  EXPECT_THAT(deleted_sites.Get(), IsEmpty());

  // Make sure that the cookie we wrote for the PAT-using site is still there.
  EXPECT_EQ(content::GetCookies(web_contents->GetBrowserContext(), bounce_url),
            "name=value");
}

// Verify that accessing storage via the PAT Topics API doesn't trigger DIPS
// deletion for the accessing site.
IN_PROC_BROWSER_TEST_F(DIPSPrivacySandboxApiInteractionTest,
                       DontTriggerDeletionOnTopicsApiStorageAccess) {
  WebContents* web_contents = GetActiveWebContents();
  // Enable Privacy Sandbox APIs in the current profile.
  PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->SetAllPrivacySandboxAllowedForTesting();

  const char* source_host = "source.a.test";
  const char* pat_using_host = "pat.b.test";

  // Write a secure cookie for PAT-using site, to represent site data written
  // through non-DIPS-triggering means.
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &embedded_https_test_server_,
                                  pat_using_host, true, false));

  // Visit source site.
  GURL source_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, source_url));

  // Navigate from source site to PAT-using site.
  GURL bounce_url =
      embedded_https_test_server_.GetURL(pat_using_host, "/title1.html");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Have PAT-using site perform a Topics API action that accesses storage,
  // without accessing storage in any other way.
  ASSERT_TRUE(content::ExecJs(web_contents,
                              R"(
                                (async () => {
                                  await document.browsingTopics();
                                })();
                              )",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Have the PAT-using site client-side-redirect back to the source site and
  // end the redirect chain.
  GURL bounce_back_url =
      embedded_https_test_server_.GetURL(source_host, "/title1.html?unique");
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, bounce_back_url));
  EndRedirectChain();

  // Expect DIPS to not have recorded user interaction.
  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), bounce_url);
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->user_interaction_times, std::nullopt);

  // Expect DIPS to have classified the bounce to the PAT-using site as
  // stateless (= to have recorded a bounce but no stateful bounce).
  EXPECT_EQ(state->stateful_bounce_times, std::nullopt);
  EXPECT_TRUE(state->bounce_times.has_value());

  // Trigger DIPS deletion, and expect DIPS to not have deleted data for the
  // PAT-using site.
  DIPSService* dips = GetDipsService(web_contents);
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  dips->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  EXPECT_THAT(deleted_sites.Get(), IsEmpty());

  // Make sure that the cookie we wrote for the PAT-using site is still there.
  EXPECT_EQ(content::GetCookies(web_contents->GetBrowserContext(), bounce_url),
            "name=value");
}

class DIPSPrivacySandboxDataPreservationTest
    : public DIPSPrivacySandboxApiInteractionTest,
      public testing::WithParamInterface<bool> {
 public:
  DIPSPrivacySandboxDataPreservationTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.emplace_back(features::kPrivacySandboxAdsAPIsOverride);
    (ShouldPreservePSData() ? enabled_features : disabled_features)
        .emplace_back(features::kDIPSPreservePSData);
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool ShouldPreservePSData() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(DIPSPrivacySandboxDataPreservationTest,
                       DontClearAttributionReportingApiData) {
  WebContents* web_contents = GetActiveWebContents();
  // Enable Privacy Sandbox APIs in the current profile.
  PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()))
      ->SetAllPrivacySandboxAllowedForTesting();

  GURL toplevel_url =
      embedded_https_test_server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  // Create image that registers an attribution source.
  GURL attribution_url = embedded_https_test_server_.GetURL(
      "b.test", "/attribution_reporting/register_source_headers.html");
  ASSERT_TRUE(content::ExecJs(web_contents, content::JsReplace(
                                                R"(
    let img = document.createElement('img');
    img.attributionSrc = $1;
    document.body.appendChild(img);)",
                                                attribution_url)));

  // Wait for the AttributionDataModel to show that source.
  ASSERT_OK_AND_ASSIGN(AttributionData data, WaitForAttributionData());
  ASSERT_THAT(GetOrigins(data),
              ElementsAre(url::Origin::Create(attribution_url)));

  // Make the attribution site eligible for DIPS deletion.
  DIPSServiceImpl* dips =
      DIPSServiceImpl::Get(web_contents->GetBrowserContext());
  ASSERT_TRUE(dips != nullptr);
  base::test::TestFuture<void> record_bounce;
  dips->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(attribution_url, base::Time::Now(), /*stateful=*/true)
      .Then(record_bounce.GetCallback());
  ASSERT_TRUE(record_bounce.Wait());

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  dips->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  EXPECT_THAT(deleted_sites.Get(),
              ElementsAre(GetSiteForDIPS(attribution_url)));

  base::test::TestFuture<AttributionData> post_deletion_data;
  web_contents->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetAttributionDataModel()
      ->GetAllDataKeys(post_deletion_data.GetCallback());
  if (ShouldPreservePSData()) {
    // Confirm the attribution data was not deleted.
    EXPECT_THAT(GetOrigins(post_deletion_data.Get()),
                ElementsAre(url::Origin::Create(attribution_url)));
  } else {
    // Confirm the attribution data was deleted.
    EXPECT_THAT(post_deletion_data.Get(), IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DIPSPrivacySandboxDataPreservationTest,
                         ::testing::Bool());

namespace {

class SiteStorage {
 public:
  constexpr SiteStorage() = default;

  virtual base::expected<std::string, std::string> ReadValue(
      content::RenderFrameHost* frame) const = 0;
  virtual testing::AssertionResult WriteValue(content::RenderFrameHost* frame,
                                              std::string_view value,
                                              bool partitioned) const = 0;

  virtual std::string_view name() const = 0;
};

class CookieStorage : public SiteStorage {
  base::expected<std::string, std::string> ReadValue(
      content::RenderFrameHost* frame) const override {
    content::EvalJsResult result = content::EvalJs(
        frame, "document.cookie", content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    if (!result.error.empty()) {
      return base::unexpected(result.error);
    }
    return base::ok(result.ExtractString());
  }

  testing::AssertionResult WriteValue(content::RenderFrameHost* frame,
                                      std::string_view cookie,
                                      bool partitioned) const override {
    std::string value(cookie);
    if (partitioned) {
      value += ";Secure;Partitioned;SameSite=None";
    }

    FrameCookieAccessObserver obs(WebContents::FromRenderFrameHost(frame),
                                  frame, CookieOperation::kChange);
    testing::AssertionResult result = content::ExecJs(
        frame, content::JsReplace("document.cookie = $1;", value),
        content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    if (result) {
      obs.Wait();
    }
    return result;
  }

  std::string_view name() const override { return "CookieStorage"; }
};

class LocalStorage : public SiteStorage {
  base::expected<std::string, std::string> ReadValue(
      content::RenderFrameHost* frame) const override {
    content::EvalJsResult result =
        content::EvalJs(frame, "localStorage.getItem('value')",
                        content::EXECUTE_SCRIPT_NO_USER_GESTURE);
    if (!result.error.empty()) {
      return base::unexpected(result.error);
    }
    if (result.value.is_none()) {
      return base::ok("");
    }
    return base::ok(result.ExtractString());
  }

  testing::AssertionResult WriteValue(content::RenderFrameHost* frame,
                                      std::string_view value,
                                      bool partitioned) const override {
    return content::ExecJs(
        frame, content::JsReplace("localStorage.setItem('value', $1);", value),
        content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  }

  std::string_view name() const override { return "LocalStorage"; }
};

void PrintTo(const SiteStorage* storage, std::ostream* os) {
  *os << storage->name();
}

static constexpr CookieStorage kCookieStorage;
static constexpr LocalStorage kLocalStorage;
}  // namespace

class DIPSDataDeletionBrowserTest
    : public DIPSBounceDetectorBrowserTest,
      public testing::WithParamInterface<const SiteStorage*> {
 public:
  void SetUpOnMainThread() override {
    DIPSBounceDetectorBrowserTest::SetUpOnMainThread();
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(kChromeTestDataDir);
    ASSERT_TRUE(https_server_.Start());

    chrome_test_utils::GetProfile(this)->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  const net::EmbeddedTestServer& https_server() const { return https_server_; }

  [[nodiscard]] testing::AssertionResult WriteToPartitionedStorage(
      std::string_view first_party_hostname,
      std::string_view third_party_hostname,
      std::string_view value) {
    content::WebContents* web_contents = GetActiveWebContents();

    if (!content::NavigateToURL(web_contents,
                                https_server().GetURL(first_party_hostname,
                                                      "/iframe_blank.html"))) {
      return testing::AssertionFailure() << "Failed to navigate top-level";
    }

    const std::string_view kIframeId = "test";
    if (!content::NavigateIframeToURL(
            web_contents, kIframeId,
            https_server().GetURL(third_party_hostname, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate iframe";
    }

    content::RenderFrameHost* iframe = content::ChildFrameAt(web_contents, 0);
    if (!iframe) {
      return testing::AssertionFailure() << "Child frame not found";
    }
    return WriteValue(iframe, value, /*partitioned=*/true);
  }

  [[nodiscard]] base::expected<std::string, std::string>
  ReadFromPartitionedStorage(std::string_view first_party_hostname,
                             std::string_view third_party_hostname) {
    content::WebContents* web_contents = GetActiveWebContents();

    if (!content::NavigateToURL(web_contents,
                                https_server().GetURL(first_party_hostname,
                                                      "/iframe_blank.html"))) {
      return base::unexpected("Failed to navigate top-level");
    }

    const std::string_view kIframeId = "test";
    if (!content::NavigateIframeToURL(
            web_contents, kIframeId,
            https_server().GetURL(third_party_hostname, "/title1.html"))) {
      return base::unexpected("Failed to navigate iframe");
    }

    content::RenderFrameHost* iframe = content::ChildFrameAt(web_contents, 0);
    if (!iframe) {
      return base::unexpected("iframe not found");
    }
    return ReadValue(iframe);
  }

  [[nodiscard]] base::expected<std::string, std::string> ReadFromStorage(
      std::string_view hostname) {
    content::WebContents* web_contents = GetActiveWebContents();

    if (!content::NavigateToURL(
            web_contents, https_server().GetURL(hostname, "/title1.html"))) {
      return base::unexpected("Failed to navigate");
    }

    return ReadValue(web_contents);
  }

  [[nodiscard]] testing::AssertionResult WriteToStorage(
      std::string_view hostname,
      std::string_view value) {
    content::WebContents* web_contents = GetActiveWebContents();

    if (!content::NavigateToURL(
            web_contents, https_server().GetURL(hostname, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate";
    }

    return WriteValue(web_contents, value);
  }

  // Navigates to host1, then performs a stateful bounce on host2 to host3.
  [[nodiscard]] testing::AssertionResult DoStatefulBounce(
      std::string_view host1,
      std::string_view host2,
      std::string_view host3) {
    content::WebContents* web_contents = GetActiveWebContents();

    if (!content::NavigateToURL(web_contents,
                                https_server().GetURL(host1, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate to " << host1;
    }

    if (!content::NavigateToURLFromRenderer(
            web_contents, https_server().GetURL(host2, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate to " << host2;
    }

    testing::AssertionResult result = WriteValue(web_contents, "bounce=yes");
    if (!result) {
      return result;
    }

    if (!content::NavigateToURLFromRendererWithoutUserGesture(
            web_contents, https_server().GetURL(host3, "/title1.html"))) {
      return testing::AssertionFailure() << "Failed to navigate to " << host3;
    }

    EndRedirectChain();

    return testing::AssertionSuccess();
  }

 private:
  const SiteStorage* storage() { return GetParam(); }

  [[nodiscard]] base::expected<std::string, std::string> ReadValue(
      const content::ToRenderFrameHost& frame) {
    return storage()->ReadValue(frame.render_frame_host());
  }

  [[nodiscard]] testing::AssertionResult WriteValue(
      const content::ToRenderFrameHost& frame,
      std::string_view value,
      bool partitioned = false) {
    return storage()->WriteValue(frame.render_frame_host(), value, partitioned);
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_P(DIPSDataDeletionBrowserTest, DeleteDomain) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Confirm unpartitioned storage was written on b.test.
  EXPECT_THAT(ReadFromStorage("b.test"), base::test::ValueIs("bounce=yes"));
  // Navigate away from b.test since DIPS won't delete its state while loaded.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, https_server().GetURL("a.test", "/title1.html")));

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm b.test storage was deleted.
  EXPECT_THAT(ReadFromStorage("b.test"), base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(DIPSDataDeletionBrowserTest, DontDeleteOtherDomains) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test
  ASSERT_TRUE(WriteToStorage("a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromStorage("a.test"), base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm a.test storage was NOT deleted.
  EXPECT_THAT(ReadFromStorage("a.test"), base::test::ValueIs("foo=bar"));
}

IN_PROC_BROWSER_TEST_P(DIPSDataDeletionBrowserTest,
                       DontDeleteDomainWhenPartitioned) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Set storage on b.test embedded in a.test.
  ASSERT_TRUE(WriteToPartitionedStorage("a.test", "b.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("a.test", "b.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm partitioned storage was NOT deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("a.test", "b.test"),
              base::test::ValueIs("foo=bar"));
}

IN_PROC_BROWSER_TEST_P(DIPSDataDeletionBrowserTest, DeleteSubdomains) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Set storage on sub.b.test
  ASSERT_TRUE(WriteToStorage("sub.b.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromStorage("sub.b.test"), base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm sub.b.test storage was deleted.
  EXPECT_THAT(ReadFromStorage("sub.b.test"), base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(DIPSDataDeletionBrowserTest, DeleteEmbedded3Ps) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test embedded in b.test.
  ASSERT_TRUE(WriteToPartitionedStorage("b.test", "a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("b.test", "a.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm partitioned a.test storage was deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("b.test", "a.test"),
              base::test::ValueIs(""));
}

IN_PROC_BROWSER_TEST_P(DIPSDataDeletionBrowserTest,
                       DeleteEmbedded3Ps_Subdomain) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Set storage on a.test embedded in sub.b.test.
  ASSERT_TRUE(WriteToPartitionedStorage("sub.b.test", "a.test", "foo=bar"));
  // Confirm written.
  EXPECT_THAT(ReadFromPartitionedStorage("sub.b.test", "a.test"),
              base::test::ValueIs("foo=bar"));

  // Perform a stateful bounce on b.test to make it eligible for deletion.
  ASSERT_TRUE(DoStatefulBounce("a.test", "b.test", "c.test"));

  // Trigger DIPS deletion.
  base::test::TestFuture<const std::vector<std::string>&> deleted_sites;
  DIPSService::Get(web_contents->GetBrowserContext())
      ->DeleteEligibleSitesImmediately(deleted_sites.GetCallback());
  ASSERT_THAT(deleted_sites.Get(), ElementsAre("b.test"));

  // Confirm partitioned a.test storage was deleted.
  EXPECT_THAT(ReadFromPartitionedStorage("sub.b.test", "a.test"),
              base::test::ValueIs(""));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DIPSDataDeletionBrowserTest,
                         ::testing::Values(&kCookieStorage, &kLocalStorage));

class DIPSBounceDetectorBFCacheTest : public DIPSBounceDetectorBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (IsBFCacheEnabled() &&
        !base::FeatureList::IsEnabled(features::kBackForwardCache)) {
      GTEST_SKIP() << "BFCache disabled";
    }
    DIPSBounceDetectorBrowserTest::SetUp();
  }
  bool IsBFCacheEnabled() const { return GetParam(); }
  void SetUpOnMainThread() override {
    if (!IsBFCacheEnabled()) {
      content::DisableBackForwardCacheForTesting(
          GetActiveWebContents(),
          content::BackForwardCache::DisableForTestingReason::
              TEST_REQUIRES_NO_CACHING);
    }

    DIPSBounceDetectorBrowserTest::SetUpOnMainThread();
  }
};

// Confirm that DIPS records a bounce that writes a cookie as stateful, even if
// the user immediately navigates away.
IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest, LateCookieAccessTest) {
  const GURL bounce_url =
      embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL final_url =
      embedded_test_server()->GetURL("c.test", "/empty.html");

  WebContents* const web_contents = GetActiveWebContents();
  RedirectChainDetector* wco =
      RedirectChainDetector::FromWebContents(web_contents);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/empty.html")));

  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));
  ASSERT_TRUE(content::ExecJs(web_contents, "document.cookie = 'bounce=true';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));
  URLCookieAccessObserver cookie_observer(web_contents, final_url,
                                          CookieOperation::kChange);

  ASSERT_TRUE(content::ExecJs(web_contents, "document.cookie = 'final=yes';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();
  // Since cookies are reported serially, both cookie writes should have been
  // reported by now.

  const DIPSRedirectContext& context = wco->CommittedRedirectContext();
  ASSERT_EQ(context.size(), 1u);
  const DIPSRedirectInfo& redirect = context.AtForTesting(0);
  EXPECT_EQ(redirect.url.url, bounce_url);
  // A request to /favicon.ico may cause a cookie read in addition to the write
  // we explicitly performed.
  EXPECT_THAT(redirect.access_type,
              testing::AnyOf(SiteDataAccessType::kWrite,
                             SiteDataAccessType::kReadWrite));
}

// Confirm that DIPS records a bounce that writes a cookie as stateful, even if
// the chain ends immediately afterwards.
IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest, QuickEndChainTest) {
  // Block 3PCs so DIPS will record bounces.
  chrome_test_utils::GetProfile(this)->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  const GURL initial_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL bounce_url =
      embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL final_url =
      embedded_test_server()->GetURL("c.test", "/empty.html");
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));
  ASSERT_TRUE(content::ExecJs(web_contents, "document.cookie = 'bounce=true';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));
  // End the redirect chain without waiting for the cookie access notification.
  EndRedirectChain();

  std::optional<StateValue> state =
      GetDIPSState(GetDipsService(web_contents), bounce_url);
  ASSERT_TRUE(state.has_value());
  ASSERT_TRUE(state->stateful_bounce_times.has_value());
}

// Confirm that WCO::OnCookiesAccessed() is always called even if the user
// immediately navigates away.
IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest, CookieAccessReported) {
  const GURL url1 = embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL url2 = embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL url3 = embedded_test_server()->GetURL("c.test", "/empty.html");

  WebContents* const web_contents = GetActiveWebContents();
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  ASSERT_TRUE(content::NavigateToURL(web_contents, url1));
  ASSERT_TRUE(content::ExecJs(web_contents, "document.cookie = 'initial=true';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(content::NavigateToURL(web_contents, url2));
  ASSERT_TRUE(content::NavigateToURL(web_contents, url3));
  URLCookieAccessObserver cookie_observer(web_contents, url3,
                                          CookieOperation::kChange);
  ASSERT_TRUE(content::ExecJs(web_contents, "document.cookie = 'final=yes';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  cookie_observer.Wait();

  EXPECT_THAT(
      logger->log(),
      testing::Contains(
          "OnCookiesAccessed(RenderFrameHost, Change: a.test/empty.html)"));
}

// Confirm that DIPS records an interaction, even if the user immediately
// navigates away.
IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest, LateInteractionTest) {
  const GURL bounce_url =
      embedded_test_server()->GetURL("b.test", "/empty.html");
  const GURL final_url =
      embedded_test_server()->GetURL("c.test", "/empty.html");
  WebContents* const web_contents = GetActiveWebContents();
  RedirectChainDetector* wco =
      RedirectChainDetector::FromWebContents(web_contents);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/empty.html")));

  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  // Consume the transient user activation so the next navigation is not
  // considered to be user-initiated and will be judged a bounce.
  if (content::EvalJs(web_contents, "!open('about:blank')",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool()) {
    // Due to a race condition, the open() call might be executed before the
    // click is processed, causing open() to fail and leaving the window with
    // transient user activation. In such a case, just skip the test. (If we
    // used UserActivationObserver::Wait() here, it would defeat the purpose of
    // this test, which is to verify that DIPS sees the interaction even if the
    // test doesn't wait for it.)
    GTEST_SKIP();
  }
  ASSERT_FALSE(
      web_contents->GetPrimaryMainFrame()->HasTransientUserActivation());

  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));
  UserActivationObserver interaction_observer(
      web_contents, web_contents->GetPrimaryMainFrame());
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  interaction_observer.Wait();

  const DIPSRedirectContext& context = wco->CommittedRedirectContext();
  ASSERT_EQ(context.size(), 1u);
  const DIPSRedirectInfo& redirect = context.AtForTesting(0);
  EXPECT_EQ(redirect.url.url, bounce_url);
  EXPECT_THAT(redirect.has_sticky_activation, true);
}

IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest, IsOrWasInPrimaryPage) {
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/empty.html")));
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(IsInPrimaryPage(rfh));
  EXPECT_TRUE(dips::IsOrWasInPrimaryPage(rfh));
  const content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("b.test", "/empty.html")));
  // Attempt to get a pointer to the RFH of the a.test page, although
  rfh = content::RenderFrameHost::FromID(rfh_id);
  if (IsBFCacheEnabled()) {
    // If the bfcache is enabled, the RFH should be in the cache.
    ASSERT_TRUE(rfh);
    EXPECT_TRUE(rfh->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kInBackForwardCache));
    // The page is no longer primary, but it used to be:
    EXPECT_FALSE(IsInPrimaryPage(rfh));
    EXPECT_TRUE(dips::IsOrWasInPrimaryPage(rfh));
  } else {
    // If the bfcache is disabled, the RFH may or may not be in memory. If it
    // still is, it's only because it's pending deletion.
    if (rfh) {
      EXPECT_TRUE(rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPendingDeletion));
      // The page is no longer primary, but it used to be:
      EXPECT_FALSE(IsInPrimaryPage(rfh));
      EXPECT_TRUE(dips::IsOrWasInPrimaryPage(rfh));
    }
  }
}

// For waiting until prerendering starts.
class PrerenderingObserver : public content::WebContentsObserver {
 public:
  explicit PrerenderingObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void Wait() { run_loop_.Run(); }

  content::GlobalRenderFrameHostId rfh_id() const {
    CHECK(rfh_id_.has_value());
    return rfh_id_.value();
  }

 private:
  base::RunLoop run_loop_;
  std::optional<content::GlobalRenderFrameHostId> rfh_id_;

  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
};

void PrerenderingObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    rfh_id_ = render_frame_host->GetGlobalId();
    run_loop_.Quit();
  }
}

// Confirm that IsOrWasInPrimaryPage() returns false for prerendered pages that
// are never activated.
IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest,
                       PrerenderedPagesAreNotPrimary) {
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/empty.html?primary")));

  PrerenderingObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    const elt = document.createElement('script');
    elt.setAttribute('type', 'speculationrules');
    elt.textContent = JSON.stringify({
      prerender: [{'urls': ['empty.html?prerendered']}]
    });    document.body.appendChild(elt);
  )"));
  observer.Wait();
  ASSERT_FALSE(testing::Test::HasFailure())
      << "Failed waiting for prerendering";

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(observer.rfh_id());
  ASSERT_TRUE(rfh);
  EXPECT_FALSE(dips::IsOrWasInPrimaryPage(rfh));

  // Navigating to another site may trigger destruction of the frame.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("b.test", "/empty.html")));

  rfh = content::RenderFrameHost::FromID(observer.rfh_id());
  if (rfh) {
    // Even if it's still in memory, it was never primary.
    EXPECT_FALSE(dips::IsOrWasInPrimaryPage(rfh));
  }
}

// Confirm that IsOrWasInPrimaryPage() returns true for prerendered pages that
// get activated.
IN_PROC_BROWSER_TEST_P(DIPSBounceDetectorBFCacheTest,
                       PrerenderedPagesCanBecomePrimary) {
  WebContents* const web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/empty.html?primary")));

  PrerenderingObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    const elt = document.createElement('script');
    elt.setAttribute('type', 'speculationrules');
    elt.textContent = JSON.stringify({
      prerender: [{'urls': ['empty.html?prerendered']}]
    });
    document.body.appendChild(elt);
  )"));
  observer.Wait();
  ASSERT_FALSE(testing::Test::HasFailure())
      << "Failed waiting for prerendering";

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(observer.rfh_id());
  ASSERT_TRUE(rfh);
  EXPECT_FALSE(dips::IsOrWasInPrimaryPage(rfh));

  // Navigate to the prerendered page.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("a.test", "/empty.html?prerendered")));
  // Navigate to another page, so the prerendered page is no longer active.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("b.test", "/empty.html")));

  rfh = content::RenderFrameHost::FromID(observer.rfh_id());
  if (rfh) {
    EXPECT_FALSE(IsInPrimaryPage(rfh));
    EXPECT_TRUE(dips::IsOrWasInPrimaryPage(rfh));
  }
}

INSTANTIATE_TEST_SUITE_P(All, DIPSBounceDetectorBFCacheTest, ::testing::Bool());
