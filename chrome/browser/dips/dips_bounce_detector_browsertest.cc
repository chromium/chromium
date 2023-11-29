// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "device/fido/virtual_fido_device_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

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

namespace {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

inline const std::string StorageTypeTestName(const StorageType& type) {
  switch (type) {
    case StorageType::DATABASE:
      return "Database";
    case StorageType::LOCAL_STORAGE:
      return "LocalStorage";
    case StorageType::SESSION_STORAGE:
      return "SessionStorage";
    case StorageType::FILE_SYSTEM:
      return "FileSystem";
    case StorageType::INDEXED_DB:
      return "IndexedDB";
    case StorageType::CACHE:
      return "Cache";
    case StorageType::WEB_LOCKS:
      return "WebLocks";
  }
}

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
      FormatURL(chain.initial_url).c_str(), FormatURL(redirect.url).c_str(),
      std::string(SiteDataAccessTypeToString(redirect.access_type)).c_str(),
      FormatURL(chain.final_url).c_str()));
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
      std::vector<base::StringPiece>(sites.begin(), sites.end()), ", "));
}

}  // namespace

// Keeps a log of DidStartNavigation, OnCookiesAccessed, and DidFinishNavigation
// executions.
class WCOCallbackLogger
    : public content_settings::PageSpecificContentSettings::SiteDataObserver,
      public content::WebContentsObserver,
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

  // Start SiteDataObserver overrides:
  void OnSiteDataAccessed(
      const content_settings::AccessDetails& access_details) override;
  void OnStatefulBounceDetected() override;
  // End SiteDataObserver overrides.

  // Start SharedWorkerService.Observer overrides:
  void OnClientAdded(
      const blink::SharedWorkerToken& token,
      content::GlobalRenderFrameHostId render_frame_host_id) override;
  void OnWorkerCreated(const blink::SharedWorkerToken& token,
                       int worker_process_id,
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
    : content_settings::PageSpecificContentSettings::SiteDataObserver(
          web_contents),
      content::WebContentsObserver(web_contents),
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

inline std::string SiteDataTypeToString(
    const content_settings::SiteDataType& type) {
  switch (type) {
    case content_settings::SiteDataType::kUnknown:
      return "Unknown";
    case content_settings::SiteDataType::kStorage:
      return "Storage";
    case content_settings::SiteDataType::kCookies:
      return "Cookies";
    case content_settings::SiteDataType::kServiceWorker:
      return "ServiceWorker";
    case content_settings::SiteDataType::kSharedWorker:
      return "SharedWorker";
    case content_settings::SiteDataType::kInterestGroup:
      return "InterestGroup";
    case content_settings::SiteDataType::kTopic:
      return "Topics";
    case content_settings::SiteDataType::kTrustToken:
      return "TrustToken";
  }
}

inline std::string AccessTypeToString(content_settings::AccessType type) {
  switch (type) {
    case content_settings::AccessType::kUnknown:
      return "Unknown";
    case content_settings::AccessType::kRead:
      return "Read";
    case content_settings::AccessType::kWrite:
      return "Write";
  }
}

void WCOCallbackLogger::OnSiteDataAccessed(
    const content_settings::AccessDetails& access_details) {
  // Avoids logging notification from the PSCS that are due to cookie accesses,
  // in order not to impact the other cookie access notification logs from the
  // `WebContentsObserver`.
  if (access_details.site_data_type ==
      content_settings::SiteDataType::kCookies) {
    return;
  }

  log_.push_back(base::StringPrintf(
      "OnSiteDataAccessed(AccessDetails, %s: %s: %s)",
      SiteDataTypeToString(access_details.site_data_type).c_str(),
      AccessTypeToString(access_details.access_type).c_str(),
      FormatURL(access_details.url).c_str()));
}

void WCOCallbackLogger::OnStatefulBounceDetected() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

class DIPSBounceDetectorBrowserTest : public PlatformBrowserTest {
 protected:
  DIPSBounceDetectorBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &DIPSBounceDetectorBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    // WebSQL is disabled by default as of M119 (crbug/695592). Enable feature
    // in tests during deprecation trial and enterprise policy support.
    enabled_features_.push_back({blink::features::kWebSQLAccess, {}});
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features_,
        /*disabled_features=*/{
            // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having
            // to disable this feature.
            features::kHttpsUpgrades,
        });
    PlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("sub.b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("sub.c.test", "127.0.0.1");
    host_resolver()->AddRule("d.test", "127.0.0.1");
    host_resolver()->AddRule("sub.d.test", "127.0.0.1");
    host_resolver()->AddRule("e.test", "127.0.0.1");
    host_resolver()->AddRule("sub.e.test", "127.0.0.1");
    host_resolver()->AddRule("f.test", "127.0.0.1");
    host_resolver()->AddRule("g.test", "127.0.0.1");
    SetUpDIPSWebContentsObserver();
  }

  void SetUpDIPSWebContentsObserver() {
    web_contents_observer_ =
        DIPSWebContentsObserver::FromWebContents(GetActiveWebContents());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    web_contents_observer_->SetRedirectChainHandlerForTesting(
        base::BindRepeating(&AppendRedirects, redirects));
  }

  void StartAppendingReportsTo(std::vector<std::string>* reports) {
    web_contents_observer_->SetIssueReportingCallbackForTesting(
        base::BindRepeating(&AppendSitesInReport, reports));
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  void EndRedirectChain() {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("a.test", "/title1.html")));
  }

  [[nodiscard]] bool AccessStorage(content::RenderFrameHost* frame,
                                   const StorageType& type) {
    return content::ExecJs(
        frame,
        base::StringPrintf(kStorageAccessScript,
                           StorageTypeTestName(type).c_str()),
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

  const base::FilePath kChromeTestDataDir =
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));

  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  raw_ptr<DIPSWebContentsObserver, AcrossTasksDanglingUntriaged>
      web_contents_observer_ = nullptr;

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    // TODO(crbug.com/1467570): Re-enable this test
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

// TODO(crbug.com/1466483): Flaky on Mac.
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
                       // TODO(crbug.com/1497430): Re-enable this test
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
    // TODO(crbug.com/1497430): Re-enable this test
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
    // TODO(crbug.com/1497430): Re-enable this test
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

// TODO(crbug.com/1454793): Flaky on Mac.
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
  const int host_id = prerender_test_helper()->AddPrerender(prerendering_url);
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
  const int host_id = prerender_test_helper()->AddPrerender(prerendering_url);
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
  // TODO(crbug.com/1447929): Investigate whether Prerendering pages (same-site)
  // can be use for evasion.
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
                            StorageType::LOCAL_STORAGE));

  // Navigate without a click (considered a client-redirect) to c.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetActiveWebContents(),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  EndRedirectChain();

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
                                  /*is_secure_cookie_set=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "b.test",
                                  /*is_secure_cookie_set=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "c.test",
                                  /*is_secure_cookie_set=*/false));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, embedded_test_server(),
                                  "d.test",
                                  /*is_secure_cookie_set=*/false));

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
  EndRedirectChain();

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
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();

  // Verify interaction was recorded for d.test, before proceeding.
  absl::optional<StateValue> state =
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
  EndRedirectChain();

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
  EndRedirectChain();

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
  EndRedirectChain();

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

  content::WebContentsDestroyedWatcher destruction_watcher(web_contents);
  web_contents->Close();
  destruction_watcher.Wait();

  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> "
                           "b.test/cross-site/c.test/title1.html (None) -> "
                           "c.test/title1.html")));
}

// Verifies server redirects that occur while opening a link in a new tab are
// properly detected.
// TODO(crbug.com/1493269): Flaky on Chrome OS and Linux.
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
  DIPSWebContentsObserver* tab_web_contents_observer =
      DIPSWebContentsObserver::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  EndRedirectChain();

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
  DIPSWebContentsObserver* tab_web_contents_observer =
      DIPSWebContentsObserver::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  // Navigate without a click (i.e. by C-redirecting) to c.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      new_tab, embedded_test_server()->GetURL("c.test", "/title1.html")));
  EndRedirectChain();

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
  DIPSWebContentsObserver* tab_web_contents_observer =
      DIPSWebContentsObserver::FromWebContents(new_tab);
  tab_web_contents_observer->SetRedirectChainHandlerForTesting(
      base::BindRepeating(&AppendRedirects, &redirects));

  EndRedirectChain();

  EXPECT_THAT(redirects,
              ElementsAre((
                  "[1/1] blank -> "
                  "b.test/cross-site-with-cookie/c.test/title1.html (Write) -> "
                  "c.test/title1.html")));
}

// Tests the conditions for recording RedirectHeuristic_CookieAccess and
// RedirectHeuristic_CookieAccessThirdParty UKM events.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
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
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.c.test",
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.d.test",
                                  /*is_secure_cookie_set=*/true));

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
          ukm_recorder.GetEntries("RedirectHeuristic.CookieAccess", {});

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
          "RedirectHeuristic.CookieAccessThirdParty", {});
  ASSERT_EQ(1u, ukm_third_party_entries.size());
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[0].source_id)
          ->url(),
      Eq(tracker_url_pre_target_redirect));
}

// Tests setting different metrics for the RedirectHeuristic_CookieAccess UKM
// event.
// TODO(https://crbug.com/1489241): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
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
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &https_server, "sub.c.test",
                                  /*is_secure_cookie_set=*/true));

  // Start on `tracker_url_with_interaction` and record a current interaction.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents, tracker_url_with_interaction));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();

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
  CreateImageAndWaitForCookieAccess(web_contents, image_url_with_interaction);

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
          "RedirectHeuristic.CookieAccess",
          {"AccessId", "AccessAllowed", "HoursSinceLastInteraction",
           "MillisecondsSinceRedirect", "OpenerHasSameSiteIframe",
           "SitesPassedCount", "DoesFirstPartyPrecedeThirdParty",
           "IsCurrentInteraction"});

  // Expect UKM entries from both of the cookie accesses, as well as the iframe
  // navigation.
  ASSERT_EQ(3u, ukm_entries.size());

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
  EXPECT_EQ(ukm_entries[0].metrics.at("HoursSinceLastInteraction"), 0);
  EXPECT_EQ(ukm_entries[0].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(ukm_entries[0].metrics.at("SitesPassedCount"), 1);
  EXPECT_EQ(ukm_entries[0].metrics.at("DoesFirstPartyPrecedeThirdParty"),
            false);
  EXPECT_EQ(ukm_entries[0].metrics.at("IsCurrentInteraction"), 1);

  // The second cookie access was due to the iframe navigation from
  // target_url_3pc_blocked to tracker_url_in_iframe.
  auto access_id_2 = ukm_entries[1].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[1].source_id)->url(),
      Eq(target_url_3pc_blocked));
  EXPECT_EQ(ukm_entries[1].metrics.at("AccessAllowed"), false);
  EXPECT_EQ(ukm_entries[1].metrics.at("HoursSinceLastInteraction"), -1);
  EXPECT_EQ(ukm_entries[1].metrics.at("IsCurrentInteraction"), 0);

  // The third cookie access was from a tracking site in an iframe of the
  // target, on a site with 3PC access blocked.

  // 3 sites were passed: tracker_url_in_iframe -> tracker_url_with_interaction
  // -> target_url_3pc_allowed -> target_url_3pc_blocked
  auto access_id_3 = ukm_entries[2].metrics.at("AccessId");
  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_entries[2].source_id)->url(),
      Eq(target_url_3pc_blocked));
  EXPECT_EQ(ukm_entries[2].metrics.at("AccessAllowed"), false);
  EXPECT_EQ(ukm_entries[2].metrics.at("OpenerHasSameSiteIframe"),
            static_cast<int32_t>(OptionalBool::kTrue));
  EXPECT_EQ(ukm_entries[2].metrics.at("SitesPassedCount"), 3);
  EXPECT_EQ(ukm_entries[2].metrics.at("DoesFirstPartyPrecedeThirdParty"), true);
  EXPECT_EQ(ukm_entries[2].metrics.at("IsCurrentInteraction"), 0);

  // Verify there are three corresponding CookieAccessThirdParty entries with
  // matching access IDs.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
      ukm_third_party_entries = ukm_recorder.GetEntries(
          "RedirectHeuristic.CookieAccessThirdParty", {"AccessId"});
  ASSERT_EQ(3u, ukm_third_party_entries.size());

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

  EXPECT_THAT(
      ukm_recorder.GetSourceForSourceId(ukm_third_party_entries[2].source_id)
          ->url(),
      Eq(tracker_url_in_iframe));
  EXPECT_EQ(ukm_third_party_entries[2].metrics.at("AccessId"), access_id_3);
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
    : public DIPSBounceDetectorBrowserTest,
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
  }

  void SetUpOnMainThread() override {
    DIPSBounceDetectorBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
  }
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
  UserActivationObserver aba_current_interaction_url_observer(
      web_contents, web_contents->GetPrimaryMainFrame());
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  aba_current_interaction_url_observer.Wait();

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
                aba_current_interaction_url, first_party_url,
                net::CookieSettingOverrides(), nullptr),
            GetParam().write_redirect_grants ? CONTENT_SETTING_ALLOW
                                             : CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(no_interaction_url, first_party_url,
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
  UserActivationObserver aba_past_interaction_url_observer(
      web_contents, web_contents->GetPrimaryMainFrame());
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  aba_past_interaction_url_observer.Wait();

  // Start redirect chain on `no_aba_current_interaction_url` and record a
  // current interaction.
  ASSERT_TRUE(
      content::NavigateToURL(web_contents, no_aba_current_interaction_url));
  UserActivationObserver no_aba_current_interaction_url_observer(
      web_contents, web_contents->GetPrimaryMainFrame());
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  no_aba_current_interaction_url_observer.Wait();

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
                aba_past_interaction_url, first_party_url,
                net::CookieSettingOverrides(), nullptr),
            (GetParam().write_redirect_grants &&
             !GetParam().require_current_interaction)
                ? CONTENT_SETTING_ALLOW
                : CONTENT_SETTING_BLOCK);
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                no_aba_current_interaction_url, first_party_url,
                net::CookieSettingOverrides(), nullptr),
            (GetParam().write_redirect_grants && !GetParam().require_aba_flow)
                ? CONTENT_SETTING_ALLOW
                : CONTENT_SETTING_BLOCK);
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
      public testing::WithParamInterface<StorageType> {
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
      testing::ContainerEq(std::vector<std::string>({
          "DidStartNavigation(a.test/title1.html)",
          "DidFinishNavigation(a.test/title1.html)",
          "OnSiteDataAccessed(AccessDetails, Storage: Unknown: a.test/)",
      })));
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
  // `StorageType::DATABASE` is disallowed in fenced frames.
  if (GetParam() == StorageType::DATABASE) {
    GTEST_SKIP();
  }

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
  // Prerendering pages do not have access to `StorageType::FILE_SYSTEM` until
  // activation (AKA becoming the primary page, whose test case is already
  // covered).
  if (GetParam() == StorageType::FILE_SYSTEM) {
    GTEST_SKIP();
  }

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  const GURL primary_main_frame_url =
      TestServer()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), primary_main_frame_url));

  const GURL prerendering_url = TestServer()->GetURL("a.test", "/title2.html");
  const int host_id = prerender_test_helper()->AddPrerender(prerendering_url);
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
// TODO(crbug.com/1449328): Remove `StorageType::DATABASE` once deprecation is
// complete.
// TODO(crbug.com/1449328): Remove `StorageType::FILE_SYSTEM` once deprecation
// is complete.
INSTANTIATE_TEST_SUITE_P(All,
                         DIPSSiteDataAccessDetectorTest,
                         ::testing::Values(StorageType::DATABASE,
                                           StorageType::LOCAL_STORAGE,
                                           StorageType::SESSION_STORAGE,
                                           StorageType::CACHE,
                                           StorageType::FILE_SYSTEM,
                                           StorageType::INDEXED_DB));

// WebAuthn tests do not work on Android because there is no current way to
// install a virtual authenticator.
// NOTE: Manual testing was performed to ensure this implementation works as
// expected on Android platform.
// TODO(crbug.com/1449328): Implement automated testing once the infrastructure
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

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  void EndRedirectChain() {
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(),
                               TestServer()->GetURL("a.test", "/title1.html")));
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    web_contents_observer_->SetRedirectChainHandlerForTesting(
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
  absl::optional<StateValue> state =
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

// TODO(crbug.com/154571): Shared workers are not available on Android.
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

class AllSitesFollowingFirstPartyTest : public DIPSBounceDetectorBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DIPSBounceDetectorBrowserTest::SetUpOnMainThread();

    first_party_url_ = embedded_test_server()->GetURL("a.test", "/title1.html");
    third_party_url_ = embedded_test_server()->GetURL("b.test", "/title1.html");
    other_url_ = embedded_test_server()->GetURL("c.test", "/title1.html");
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

  EXPECT_THAT(web_contents_observer_->AllSitesFollowingFirstPartyForTesting(
                  first_party_url_),
              testing::ElementsAre(GetSiteForDIPS(third_party_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       SiteNotFollowingFirstPartyNotIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));

  EXPECT_THAT(web_contents_observer_->AllSitesFollowingFirstPartyForTesting(
                  first_party_url_),
              testing::ElementsAre(GetSiteForDIPS(third_party_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest, MultipleSitesIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(web_contents_observer_->AllSitesFollowingFirstPartyForTesting(
                  first_party_url_),
              testing::ElementsAre(GetSiteForDIPS(third_party_url_),
                                   GetSiteForDIPS(other_url_)));
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       NoFirstParty_NothingIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));

  EXPECT_THAT(web_contents_observer_->AllSitesFollowingFirstPartyForTesting(
                  first_party_url_),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(AllSitesFollowingFirstPartyTest,
                       NothingAfterFirstParty_NothingIncluded) {
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), other_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), third_party_url_));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_party_url_));

  EXPECT_THAT(web_contents_observer_->AllSitesFollowingFirstPartyForTesting(
                  first_party_url_),
              testing::IsEmpty());
}
