// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a fake safebrowsing service, where we can inject known-
// threat urls.  It then uses a real browser to go to these urls, and sends
// "goback" or "proceed" commands and verifies they work.

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_idn_test.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/permissions/permission_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/renderer/threat_dom_details.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/web_ui/constants.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/security_interstitials/core/urls.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Delayed warnings feature checks if the Suspicious Site Reporter extension
// is installed. These includes are to fake-install this extension.
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#endif

using chrome_browser_interstitials::SecurityInterstitialIDNTest;
using content::BrowserThread;
using content::NavigationController;
using content::RenderFrameHost;
using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;

namespace safe_browsing {

namespace {
const char kEmptyPage[] = "/empty.html";
const char kHTTPSPage[] = "/ssl/google.html";
const char kMaliciousPage[] = "/safe_browsing/malware.html";
const char kCrossSiteMaliciousPage[] = "/safe_browsing/malware2.html";
const char kPageWithCrossOriginMaliciousIframe[] =
    "/safe_browsing/malware3.html";
const char kCrossOriginMaliciousIframeHost[] = "malware.test";
const char kMaliciousIframe[] = "/safe_browsing/malware_iframe.html";
const char kUnrelatedUrl[] = "https://www.google.com";
const char kEnhancedProtectionUrl[] = "chrome://settings/security?q=enhanced";

}  // namespace

enum Visibility { VISIBILITY_ERROR = -1, HIDDEN = 0, VISIBLE = 1 };

bool IsShowingInterstitial(WebContents* contents) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  return helper &&
         (helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
          nullptr);
}

content::RenderFrameHost* GetRenderFrameHost(Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
}

bool WaitForReady(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!content::WaitForRenderFrameReady(contents->GetMainFrame())) {
    return false;
  }
  return IsShowingInterstitial(contents);
}

Visibility GetVisibility(Browser* browser, const std::string& node_id) {
  content::RenderFrameHost* rfh = GetRenderFrameHost(browser);
  if (!rfh)
    return VISIBILITY_ERROR;

  // clang-format off
  std::string jsFindVisibility = R"(
    (function isNodeVisible(node) {
      if (!node) return 'node not found';
      if (node.offsetWidth === 0 || node.offsetHeight === 0) return false;
      // Do not check opacity, since the css transition may actually leave
      // opacity at 0 after it's been unhidden
      if (node.classList.contains('hidden')) return false;
      // Otherwise, we must check all parent nodes
      var parentVisibility = isNodeVisible(node.parentElement);
      if (parentVisibility === 'node not found') {
        return true; // none of the parents are set invisible
      }
      return parentVisibility;
    }(document.getElementById(')" + node_id + R"(')));)";
  // clang-format on

  base::Value value = content::ExecuteScriptAndGetValue(rfh, jsFindVisibility);

  if (!value.is_bool())
    return VISIBILITY_ERROR;

  return value.GetBool() ? VISIBLE : HIDDEN;
}

bool Click(Browser* browser, const std::string& node_id) {
  DCHECK(node_id == "primary-button" || node_id == "proceed-link" ||
         node_id == "whitepaper-link" || node_id == "details-button" ||
         node_id == "opt-in-checkbox" || node_id == "enhanced-protection-link")
      << "Unexpected node_id: " << node_id;
  content::RenderFrameHost* rfh = GetRenderFrameHost(browser);
  if (!rfh)
    return false;
  // We don't use ExecuteScriptAndGetValue for this one, since clicking
  // the button/link may navigate away before the injected javascript can
  // reply, hanging the test.
  rfh->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.getElementById('" + node_id +
                         "').click();\n"),
      base::NullCallback());
  return true;
}

bool ClickAndWaitForDetach(Browser* browser, const std::string& node_id) {
  // We wait for interstitial_detached rather than nav_entry_committed, as
  // going back from a main-frame safe browsing interstitial page will not
  // cause a nav entry committed event.
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  if (!Click(browser, node_id))
    return false;
  observer.WaitForNavigationFinished();
  return true;
}

// A SafeBrowsingDatabaseManager class that allows us to inject the malicious
// URLs.
class FakeSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager() {}

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Overrides SafeBrowsingDatabaseManager::CheckBrowseUrl.
  bool CheckBrowseUrl(const GURL& gurl,
                      const SBThreatTypeSet& threat_types,
                      Client* client) override {
    if (badurls_.find(gurl.spec()) == badurls_.end() ||
        badurls_[gurl.spec()] == SB_THREAT_TYPE_SAFE)
      return true;

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                       this, gurl, client));
    return false;
  }

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    if (badurls_.find(gurl.spec()) != badurls_.end())
      client->OnCheckBrowseUrlResult(gurl, badurls_[gurl.spec()],
                                     ThreatMetadata());
    else
      NOTREACHED();
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    badurls_[url.spec()] = threat_type;
  }

  void ClearBadURL(const GURL& url) { badurls_.erase(url.spec()); }

  // These are called when checking URLs, so we implement them.
  bool IsSupported() const override { return true; }
  bool ChecksAreAlwaysAsync() const override { return false; }
  bool CanCheckResourceType(
      blink::mojom::ResourceType /* resource_type */) const override {
    return true;
  }

  // Called during startup, so must not check-fail.
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override {
    return true;
  }

  safe_browsing::ThreatSource GetThreatSource() const override {
    return safe_browsing::ThreatSource::LOCAL_PVER3;
  }

 private:
  ~FakeSafeBrowsingDatabaseManager() override {}

  std::map<std::string, SBThreatType> badurls_;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// A SafeBrowingUIManager class that allows intercepting malware details.
class FakeSafeBrowsingUIManager : public TestSafeBrowsingUIManager {
 public:
  FakeSafeBrowsingUIManager()
      : TestSafeBrowsingUIManager(),
        threat_details_done_(false),
        hit_report_sent_(false) {}
  explicit FakeSafeBrowsingUIManager(SafeBrowsingService* service)
      : TestSafeBrowsingUIManager(service),
        threat_details_done_(false),
        hit_report_sent_(false) {}

  // Overrides SafeBrowsingUIManager
  void SendSerializedThreatDetails(content::BrowserContext* browser_context,
                                   const std::string& serialized) override {
    // Notify the UI thread that we got a report.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSafeBrowsingUIManager::OnThreatDetailsDone, this,
                       serialized));
  }

  void OnThreatDetailsDone(const std::string& serialized) {
    if (threat_details_done_) {
      return;
    }
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    report_ = serialized;

    ASSERT_TRUE(threat_details_done_callback_);
    std::move(threat_details_done_callback_).Run();
    threat_details_done_ = true;
  }

  void MaybeReportSafeBrowsingHit(const HitReport& hit_report,
                                  WebContents* web_contents) override {
    if (SafeBrowsingUIManager::ShouldSendHitReport(hit_report, web_contents)) {
      hit_report_sent_ = true;
    }
  }

  bool hit_report_sent() { return hit_report_sent_; }

  void set_threat_details_done_callback(base::OnceClosure callback) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_FALSE(threat_details_done_callback_);
    threat_details_done_callback_ = std::move(callback);
  }

  std::string GetReport() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return report_;
  }

 protected:
  ~FakeSafeBrowsingUIManager() override {}

 private:
  std::string report_;
  base::OnceClosure threat_details_done_callback_;
  bool threat_details_done_;
  bool hit_report_sent_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingUIManager);
};

class TestThreatDetailsFactory : public ThreatDetailsFactory {
 public:
  TestThreatDetailsFactory() : details_() {}
  ~TestThreatDetailsFactory() override {}

  std::unique_ptr<ThreatDetails> CreateThreatDetails(
      BaseUIManager* delegate,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      bool trim_to_ad_tags,
      ThreatDetailsDoneCallback done_callback) override {
    auto details = base::WrapUnique(new ThreatDetails(
        delegate, web_contents, unsafe_resource, url_loader_factory,
        history_service, referrer_chain_provider, trim_to_ad_tags,
        std::move(done_callback)));
    details_ = details.get();
    details->StartCollection();
    return details;
  }

  ThreatDetails* get_details() { return details_; }

 private:
  ThreatDetails* details_;
};

// A SafeBrowingBlockingPage class that lets us wait until it's hidden.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(
      BaseUIManager* manager,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      bool should_trigger_reporting)
      : SafeBrowsingBlockingPage(manager,
                                 web_contents,
                                 main_frame_url,
                                 unsafe_resources,
                                 display_options,
                                 should_trigger_reporting),
        wait_for_delete_(false) {
    // Don't wait the whole 3 seconds for the browser test.
    SetThreatDetailsProceedDelayForTesting(100);
  }

  ~TestSafeBrowsingBlockingPage() override {
    if (!wait_for_delete_)
      return;

    // Notify that we are gone
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    wait_for_delete_ = false;
  }

  void WaitForDelete() {
    wait_for_delete_ = true;
    content::RunMessageLoop();
  }

  // SecurityInterstitialPage methods:
  void CommandReceived(const std::string& command) override {
    SafeBrowsingBlockingPage::CommandReceived(command);
  }

 private:
  bool wait_for_delete_;
};

void AssertNoInterstitial(Browser* browser, bool wait_for_delete) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_FALSE(IsShowingInterstitial(contents));
  return;
}

class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() : always_show_back_to_safety_(true) {}
  ~TestSafeBrowsingBlockingPageFactory() override {}

  void SetAlwaysShowBackToSafety(bool value) {
    always_show_back_to_safety_ = value;
  }

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* delegate,
      WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting) override {
    PrefService* prefs =
        Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->GetPrefs();
    bool is_extended_reporting_opt_in_allowed =
        prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
    bool is_proceed_anyway_disabled =
        prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled);

    BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options(
        BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
        is_extended_reporting_opt_in_allowed,
        web_contents->GetBrowserContext()->IsOffTheRecord(),
        IsExtendedReportingEnabled(*prefs),
        IsExtendedReportingPolicyManaged(*prefs),
        IsEnhancedProtectionEnabled(*prefs), is_proceed_anyway_disabled,
        true,  // should_open_links_in_new_tab
        always_show_back_to_safety_,
        IsEnhancedProtectionMessageInInterstitialsEnabled(),
        IsSafeBrowsingPolicyManaged(*prefs),
        "cpn_safe_browsing" /* help_center_article_link */);
    return new TestSafeBrowsingBlockingPage(
        delegate, web_contents, main_frame_url, unsafe_resources,
        display_options, should_trigger_reporting);
  }

 private:
  bool always_show_back_to_safety_;
};

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingBlockingPageBrowserTest
    : public CertVerifierBrowserTest,
      public testing::WithParamInterface<testing::tuple<SBThreatType, bool>> {
 public:
  SafeBrowsingBlockingPageBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::map<std::string, std::string> parameters = {
        {safe_browsing::kTagAndAttributeParamName, "div,foo,div,baz"}};
    base::test::ScopedFeatureList::FeatureAndParams tag_and_attribute(
        safe_browsing::kThreatDomDetailsTagAndAttributeFeature, parameters);
    scoped_feature_list_.InitWithFeaturesAndParameters({tag_and_attribute}, {});
  }

  ~SafeBrowsingBlockingPageBrowserTest() override {}

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager());
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager());
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    SafeBrowsingBlockingPage::RegisterFactory(nullptr);
    SafeBrowsingService::RegisterFactory(nullptr);
    ThreatDetails::RegisterFactory(nullptr);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    if (testing::get<1>(GetParam()))
      content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->SetURLThreatType(url, threat_type);
  }

  void ClearBadURL(const GURL& url) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->ClearBadURL(url);
  }

  // The basic version of this method, which uses an HTTP test URL.
  GURL SetupWarningAndNavigate(Browser* browser) {
    return SetupWarningAndNavigateToURL(
        embedded_test_server()->GetURL(kEmptyPage), browser);
  }

  // The basic version of this method, which uses an HTTP test URL.
  GURL SetupWarningAndNavigateInNewTab(Browser* browser) {
    return SetupWarningAndNavigateToURLInNewTab(
        embedded_test_server()->GetURL(kEmptyPage), browser);
  }

  // Navigates to a warning on a valid HTTPS website.
  GURL SetupWarningAndNavigateToValidHTTPS() {
    EXPECT_TRUE(https_server_.Start());
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert;
    verify_result.cert_status = 0;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);
    GURL url = https_server_.GetURL(kHTTPSPage);
    return SetupWarningAndNavigateToURL(url, browser());
  }

  // Navigates through an HTTPS interstitial, then opens up a SB warning on that
  // same URL.
  GURL SetupWarningAndNavigateToInvalidHTTPS() {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    EXPECT_TRUE(https_server_.Start());
    GURL url = https_server_.GetURL(kHTTPSPage);

    // Proceed through the HTTPS interstitial.
    ui_test_utils::NavigateToURL(browser(), url);

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    security_interstitials::SecurityInterstitialPage* ssl_blocking_page;

    EXPECT_TRUE(WaitForRenderFrameReady(contents->GetMainFrame()));
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    EXPECT_TRUE(helper);
    ssl_blocking_page =
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();

    EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
              ssl_blocking_page->GetTypeForTesting());
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ssl_blocking_page->CommandReceived(base::NumberToString(
        security_interstitials::SecurityInterstitialCommand::CMD_PROCEED));
    // When both SB and SSL interstitials are committed navigations, we need
    // to wait for two navigations here, one is from the SSL interstitial to
    // the blocked site (which does not complete since SB blocks it) and the
    // second one is to the actual SB interstitial.
    observer.WaitForNavigationFinished();

    return SetupWarningAndNavigateToURL(url, browser());
  }

  // Adds a safebrowsing threat results to the fake safebrowsing service,
  // navigates to a page with an iframe containing the threat site, and returns
  // the url of the parent page.
  GURL SetupThreatIframeWarningAndNavigate() {
    GURL url = embedded_test_server()->GetURL(kCrossSiteMaliciousPage);
    GURL iframe_url = embedded_test_server()->GetURL(kMaliciousIframe);
    SetURLThreatType(iframe_url, testing::get<0>(GetParam()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady(browser()));
    return url;
  }

  // Adds a safebrowsing threat results to the fake safebrowsing service, and
  // navigates to a page with a cross-origin iframe containing the threat site.
  // Returns the url of the parent page and sets |iframe_url| to the malicious
  // cross-origin iframe.
  GURL SetupCrossOriginThreatIframeWarningAndNavigate(GURL* iframe_url) {
    GURL url =
        embedded_test_server()->GetURL(kPageWithCrossOriginMaliciousIframe);
    *iframe_url = embedded_test_server()->GetURL(kMaliciousIframe);
    GURL::Replacements replace_host;
    replace_host.SetHostStr(kCrossOriginMaliciousIframeHost);
    *iframe_url = iframe_url->ReplaceComponents(replace_host);
    SetURLThreatType(*iframe_url, testing::get<0>(GetParam()));

    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady(browser()));
    return url;
  }

  GURL GetWhitePaperUrl() {
    return google_util::AppendGoogleLocaleParam(
        GURL(security_interstitials::kSafeBrowsingWhitePaperUrl),
        factory_.test_safe_browsing_service()
            ->ui_manager()
            .get()
            ->app_locale());
  }

  void SendCommand(
      security_interstitials::SecurityInterstitialCommand command) {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SafeBrowsingBlockingPage* interstitial_page;
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    ASSERT_TRUE(helper);
    interstitial_page = static_cast<SafeBrowsingBlockingPage*>(
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
              interstitial_page->GetTypeForTesting());
    interstitial_page->CommandReceived(base::NumberToString(command));
  }

  void SetReportSentCallback(base::OnceClosure callback) {
    static_cast<FakeSafeBrowsingUIManager*>(
        factory_.test_safe_browsing_service()->ui_manager().get())
        ->set_threat_details_done_callback(std::move(callback));
  }

  std::string GetReportSent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->GetReport();
  }

  content::RenderFrameHost* GetRenderFrameHost() {
    return ::safe_browsing::GetRenderFrameHost(browser());
  }

  Visibility GetVisibility(const std::string& node_id) {
    return ::safe_browsing::GetVisibility(browser(), node_id);
  }

  bool Click(const std::string& node_id) {
    return ::safe_browsing::Click(browser(), node_id);
  }

  bool ClickAndWaitForDetach(const std::string& node_id) {
    return ::safe_browsing::ClickAndWaitForDetach(browser(), node_id);
  }

  void AssertNoInterstitial(bool wait_for_delete) {
    return ::safe_browsing::AssertNoInterstitial(browser(), wait_for_delete);
  }

  void TestReportingDisabledAndDontProceed(const GURL& url) {
    SetURLThreatType(url, testing::get<0>(GetParam()));
    ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_TRUE(WaitForReady(browser()));

    EXPECT_EQ(HIDDEN, GetVisibility("extended-reporting-opt-in"));
    EXPECT_EQ(HIDDEN, GetVisibility("opt-in-checkbox"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(VISIBLE, GetVisibility("learn-more-link"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));

    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
    AssertNoInterstitial(false);          // Assert the interstitial is gone
    EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
              browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  }

  void VerifyResource(
      const ClientSafeBrowsingReportRequest& report,
      const ClientSafeBrowsingReportRequest::Resource& actual_resource,
      const std::string& expected_url,
      const std::string& expected_parent,
      int expected_child_size,
      const std::string& expected_tag_name) {
    EXPECT_EQ(expected_url, actual_resource.url());
    // Finds the parent url by comparing resource ids.
    for (auto resource : report.resources()) {
      if (actual_resource.parent_id() == resource.id()) {
        EXPECT_EQ(expected_parent, resource.url());
        break;
      }
    }
    EXPECT_EQ(expected_child_size, actual_resource.child_ids_size());
    EXPECT_EQ(expected_tag_name, actual_resource.tag_name());
  }

  void VerifyElement(
      const ClientSafeBrowsingReportRequest& report,
      const HTMLElement& actual_element,
      const std::string& expected_tag_name,
      const int expected_child_ids_size,
      const std::vector<mojom::AttributeNameValuePtr>& expected_attributes) {
    EXPECT_EQ(expected_tag_name, actual_element.tag());
    EXPECT_EQ(expected_child_ids_size, actual_element.child_ids_size());
    ASSERT_EQ(static_cast<int>(expected_attributes.size()),
              actual_element.attribute_size());
    for (size_t i = 0; i < expected_attributes.size(); ++i) {
      const mojom::AttributeNameValue& expected_attribute =
          *expected_attributes[i];
      const HTMLElement::Attribute& actual_attribute_pb =
          actual_element.attribute(i);
      EXPECT_EQ(expected_attribute.name, actual_attribute_pb.name());
      EXPECT_EQ(expected_attribute.value, actual_attribute_pb.value());
    }
  }

  void ExpectSecurityIndicatorDowngrade(content::WebContents* tab,
                                        net::CertStatus cert_status) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(tab);
    ASSERT_TRUE(helper);
    EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
    EXPECT_NE(security_state::MALICIOUS_CONTENT_STATUS_NONE,
              helper->GetVisibleSecurityState()->malicious_content_status);
    // TODO(felt): Restore this check when https://crbug.com/641187 is fixed.
    // EXPECT_EQ(cert_status, helper->GetSecurityInfo().cert_status);
  }

  void ExpectNoSecurityIndicatorDowngrade(content::WebContents* tab) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(tab);
    ASSERT_TRUE(helper);
    EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
    EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
              helper->GetVisibleSecurityState()->malicious_content_status);
  }

  bool hit_report_sent() {
    return static_cast<FakeSafeBrowsingUIManager*>(
               factory_.test_safe_browsing_service()->ui_manager().get())
        ->hit_report_sent();
  }

  // Helper method for LearnMore test below. Implemented as a test fixture
  // method instead of in the test below because the whole test fixture class
  // is friended by SafeBrowsingBlockingPage.
  void MockHelpCenterUrl(SafeBrowsingBlockingPage* sb_interstitial) {
    ASSERT_TRUE(https_server_.Start());
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = cert;
    verify_result.cert_status = 0;
    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);

    security_interstitials::SecurityInterstitialControllerClient* client =
        sb_interstitial->controller();

    client->SetBaseHelpCenterUrlForTesting(
        https_server_.GetURL("/title1.html"));
  }

  void SetAlwaysShowBackToSafety(bool val) {
    blocking_page_factory_.SetAlwaysShowBackToSafety(val);
  }

 protected:
  TestThreatDetailsFactory details_factory_;

 private:
  // Adds a safebrowsing result of the current test threat to the fake
  // safebrowsing service, navigates to that page, and returns the url.
  // The various wrappers supply different URLs.
  GURL SetupWarningAndNavigateToURL(GURL url, Browser* browser) {
    SetURLThreatType(url, testing::get<0>(GetParam()));
    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_TRUE(WaitForReady(browser));
    return url;
  }
  // Adds a safebrowsing result of the current test threat to the fake
  // safebrowsing service, navigates to that page, and returns the url.
  // The various wrappers supply different URLs.
  GURL SetupWarningAndNavigateToURLInNewTab(GURL url, Browser* browser) {
    SetURLThreatType(url, testing::get<0>(GetParam()));
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    observer.WaitForNavigationFinished();
    EXPECT_TRUE(WaitForReady(browser));
    return url;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageBrowserTest);
};

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, HardcodedUrls) {
  const GURL urls[] = {GURL(kChromeUISafeBrowsingMatchMalwareUrl),
                       GURL(kChromeUISafeBrowsingMatchPhishingUrl),
                       GURL(kChromeUISafeBrowsingMatchUnwantedUrl)};

  for (const GURL& url : urls) {
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(WaitForReady(browser()));

    EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
    EXPECT_EQ(HIDDEN, GetVisibility("details"));
    EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(Click("details-button"));
    EXPECT_EQ(VISIBLE, GetVisibility("details"));
    EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
    EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
    EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

    AssertNoInterstitial(false);          // Assert the interstitial is gone
    EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
              browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  }
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, DontProceed) {
  SetupWarningAndNavigate(browser());

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial(false);          // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, VisitWhitePaper) {
  SetupWarningAndNavigate(browser());

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(interstitial_tab);

  EXPECT_EQ(VISIBLE, GetVisibility("whitepaper-link"));
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(Click("whitepaper-link"));

  nav_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Assert the interstitial is not present in the foreground tab.
  AssertNoInterstitial(false);

  // Foreground tab displays the help center.
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(GetWhitePaperUrl(), new_tab->GetURL());

  // Interstitial should still display in the background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(interstitial_tab,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, Proceed) {
  GURL url = SetupWarningAndNavigate(browser());

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeDontProceed) {
  SetupThreatIframeWarningAndNavigate();

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  AssertNoInterstitial(false);  // Assert the interstitial is gone

  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, IframeProceed) {
  GURL url = SetupThreatIframeWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

#if defined(OS_MAC)
#define MAYBE_IframeOptInAndReportThreatDetails \
  DISABLED_IframeOptInAndReportThreatDetails
#else
#define MAYBE_IframeOptInAndReportThreatDetails \
  IframeOptInAndReportThreatDetails
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_IframeOptInAndReportThreatDetails) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Set up testing url containing iframe and cross site iframe.
  GURL url = SetupThreatIframeWarningAndNavigate();

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    // Do some basic verification of report contents.
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(embedded_test_server()->GetURL(kMaliciousIframe).spec(),
              report.url());
    std::vector<ClientSafeBrowsingReportRequest::Resource> resources;
    for (auto resource : report.resources()) {
      resources.push_back(resource);
    }
    // Sort resources based on their urls.
    std::sort(resources.begin(), resources.end(),
              [](const ClientSafeBrowsingReportRequest::Resource& a,
                 const ClientSafeBrowsingReportRequest::Resource& b) -> bool {
                return a.url() < b.url();
              });
    ASSERT_EQ(2U, resources.size());
    VerifyResource(
        report, resources[0],
        embedded_test_server()->GetURL(kCrossSiteMaliciousPage).spec(),
        embedded_test_server()->GetURL(kCrossSiteMaliciousPage).spec(), 1, "");
    VerifyResource(report, resources[1],
                   embedded_test_server()->GetURL(kMaliciousIframe).spec(),
                   url.spec(),  // kCrossSiteMaliciousPage
                   0, "IFRAME");

    ASSERT_EQ(2, report.dom_size());
    // Because the order of elements is not deterministic, we basically need to
    // verify the relationship. Namely that there is an IFRAME element and that
    // it has a DIV as its parent.
    int iframe_node_id = -1;
    for (const HTMLElement& elem : report.dom()) {
      if (elem.tag() == "IFRAME") {
        iframe_node_id = elem.id();
        VerifyElement(report, elem, "IFRAME", /*child_size=*/0,
                      std::vector<mojom::AttributeNameValuePtr>());
        break;
      }
    }
    EXPECT_GT(iframe_node_id, -1);

    // Find the parent DIV that is the parent of the iframe.
    for (const HTMLElement& elem : report.dom()) {
      if (elem.id() != iframe_node_id) {
        std::vector<mojom::AttributeNameValuePtr> attributes;
        attributes.push_back(mojom::AttributeNameValue::New("foo", "1"));
        // Not the IIFRAME, so this is the parent DIV
        VerifyElement(report, elem, "DIV", /*child_size=*/1, attributes);
        // Make sure this DIV has the IFRAME as a child.
        EXPECT_EQ(iframe_node_id, elem.child_ids(0));
      }
    }
  }
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MainFrameBlockedShouldHaveNoDOMDetailsWhenDontProceed) {
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Navigate to a safe page which contains multiple potential DOM details.
  // (Despite the name, kMaliciousPage is not the page flagged as bad in this
  // test.)
  GURL safe_url(embedded_test_server()->GetURL(kMaliciousPage));
  ui_test_utils::NavigateToURL(browser(), safe_url);

  EXPECT_EQ(nullptr, details_factory_.get_details());

  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL url = SetupWarningAndNavigate(browser());
  observer.WaitForNavigationFinished();
  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);

  // Go back.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  EXPECT_EQ(safe_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(url.spec(), report.url());
    ASSERT_EQ(1, report.resources_size());
    EXPECT_EQ(url.spec(), report.resources(0).url());
  }
}

IN_PROC_BROWSER_TEST_P(
    SafeBrowsingBlockingPageBrowserTest,
    MainFrameBlockedShouldHaveNoDOMDetailsWhenProceeding) {
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Navigate to a safe page which contains multiple potential DOM details.
  // (Despite the name, kMaliciousPage is not the page flagged as bad in this
  // test.)
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kMaliciousPage));

  EXPECT_EQ(nullptr, details_factory_.get_details());

  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  GURL url = SetupWarningAndNavigate(browser());

  ThreatDetails* threat_details = details_factory_.get_details();
  EXPECT_EQ(expect_threat_details, threat_details != nullptr);

  // Proceed through the warning.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    // Verify the report is complete.
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
    EXPECT_EQ(url.spec(), report.url());
    ASSERT_EQ(1, report.resources_size());
    EXPECT_EQ(url.spec(), report.resources(0).url());
  }
}

// Verifies that the "proceed anyway" link isn't available when it is disabled
// by the corresponding policy. Also verifies that sending the "proceed"
// command anyway doesn't advance to the unsafe site.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ProceedDisabled) {
  // Simulate a policy disabling the "proceed anyway" link.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingProceedAnywayDisabled, true);

  SetupWarningAndNavigate(browser());

  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("final-paragraph"));
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  SendCommand(security_interstitials::CMD_PROCEED);
  observer.WaitForNavigationFinished();

  // The "proceed" command should go back instead, if proceeding is disabled.
  AssertNoInterstitial(true);
  EXPECT_EQ(GURL(url::kAboutBlankURL),  // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, NoBackToSafety) {
  SetAlwaysShowBackToSafety(false);
  SetupWarningAndNavigateInNewTab(browser());

  EXPECT_EQ(HIDDEN, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
}

// Verifies that the reporting checkbox is hidden when opt-in is
// disabled by policy. However, reports can still be sent if extended
// reporting is enabled (eg: by its own policy).
// Note: this combination will be deprecated along with the OptInAllowed
// policy, to be replaced by a policy on the SBER setting itself.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       ReportingDisabledByPolicy) {
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  TestReportingDisabledAndDontProceed(
      embedded_test_server()->GetURL(kEmptyPage));
}

// Verifies that the reporting checkbox is still shown if the page is reloaded
// while the interstitial is showing.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       ReloadWhileInterstitialShowing) {
  // Start navigation to bad page (kEmptyPage), which will be blocked before it
  // is committed.
  const GURL url = SetupWarningAndNavigate(browser());

  // Checkbox should be showing.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  // Security indicator should be showing.
  ExpectSecurityIndicatorDowngrade(tab, 0u);

  // Check navigation entry state.
  NavigationController& controller = tab->GetController();
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // "Reload" the tab.
  SetupWarningAndNavigate(browser());

  // Checkbox should be showing.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));

  // Security indicator should be showing.
  ExpectSecurityIndicatorDowngrade(tab, 0u);
  // Check navigation entry state.
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
}

#if (defined(OS_MAC) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
// TODO(crbug.com/1132307): Address flaky timeout.
#define MAYBE_LearnMore DISABLED_LearnMore
#else
#define MAYBE_LearnMore LearnMore
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, MAYBE_LearnMore) {
  SetupWarningAndNavigate(browser());

  SafeBrowsingBlockingPage* sb_interstitial;
  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(interstitial_tab);

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          interstitial_tab);
  ASSERT_TRUE(helper);
  sb_interstitial = static_cast<SafeBrowsingBlockingPage*>(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());

  MockHelpCenterUrl(sb_interstitial);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  SendCommand(security_interstitials::CMD_OPEN_HELP_CENTER);
  nav_observer.Wait();

  // A new tab has been opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Interstitial does not display in the foreground tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  WebContents* new_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_tab);
  EXPECT_FALSE(IsShowingInterstitial(new_tab));

  // Interstitial still displays in the background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(interstitial_tab,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_DontProceed) {
  base::HistogramTester histograms;
  std::string prefix;
  SBThreatType threat_type = testing::get<0>(GetParam());
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE)
    prefix = "malware";
  else if (threat_type == SB_THREAT_TYPE_URL_PHISHING)
    prefix = "phishing";
  else if (threat_type == SB_THREAT_TYPE_URL_UNWANTED)
    prefix = "harmful";
  else
    NOTREACHED();
  const std::string decision_histogram = "interstitial." + prefix + ".decision";
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";

  // TODO(nparker): Check for *.from_device as well.

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  SetupWarningAndNavigate(browser());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial(false);  // Assert the interstitial is gone
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::DONT_PROCEED,
      1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       Histograms_Proceed) {
  base::HistogramTester histograms;
  std::string prefix;
  SBThreatType threat_type = testing::get<0>(GetParam());
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE)
    prefix = "malware";
  else if (threat_type == SB_THREAT_TYPE_URL_PHISHING)
    prefix = "phishing";
  else if (threat_type == SB_THREAT_TYPE_URL_UNWANTED)
    prefix = "harmful";
  else
    NOTREACHED();
  const std::string decision_histogram = "interstitial." + prefix + ".decision";
  const std::string interaction_histogram =
      "interstitial." + prefix + ".interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  GURL url = SetupWarningAndNavigate(browser());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, WhitelistRevisit) {
  GURL url = SetupWarningAndNavigate(browser());

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Unrelated pages should not be whitelisted now.
  ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl));
  AssertNoInterstitial(false);

  // The whitelisted page should remain whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);
  AssertNoInterstitial(false);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       WhitelistIframeRevisit) {
  GURL url = SetupThreatIframeWarningAndNavigate();

  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone.
  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Unrelated pages should not be whitelisted now.
  ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl));
  AssertNoInterstitial(false);

  // The whitelisted page should remain whitelisted.
  ui_test_utils::NavigateToURL(browser(), url);
  AssertNoInterstitial(false);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, WhitelistUnsaved) {
  GURL url = SetupWarningAndNavigate(browser());

  // Navigate without making a decision.
  ui_test_utils::NavigateToURL(browser(), GURL(kUnrelatedUrl));
  AssertNoInterstitial(false);

  // The non-whitelisted page should now show an interstitial.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WaitForReady(browser()));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
}

#if (defined(OS_MAC) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
// TODO(crbug.com/1132307): Address flay failure.
#define MAYBE_VerifyHitReportSentOnSBERAndNotIncognito \
  DISABLED_VerifyHitReportSentOnSBERAndNotIncognito
#else
#define MAYBE_VerifyHitReportSentOnSBERAndNotIncognito \
  VerifyHitReportSentOnSBERAndNotIncognito
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_VerifyHitReportSentOnSBERAndNotIncognito) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, true);
  GURL url = SetupWarningAndNavigate(browser());            // not incognito
  EXPECT_TRUE(hit_report_sent());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       VerifyHitReportNotSentOnIncognito) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  Browser* incognito_browser = CreateIncognitoBrowser();
  incognito_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, true);     // set up SBER
  GURL url = SetupWarningAndNavigate(incognito_browser);    // incognito
  // Check SBER opt in is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        incognito_browser, "extended-reporting-opt-in"));
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(incognito_browser,
                                                   "opt-in-checkbox"));

  EXPECT_FALSE(hit_report_sent());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       VerifyHitReportNotSentWithoutSBER) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));

  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, false);     // set up SBER
  GURL url = SetupWarningAndNavigate(browser());             // not incognito
  EXPECT_FALSE(hit_report_sent());
}

namespace {

class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        latest_security_style_(blink::SecurityStyle::kUnknown),
        latest_security_style_explanations_() {}

  blink::SecurityStyle latest_security_style() const {
    return latest_security_style_;
  }

  content::SecurityStyleExplanations latest_security_style_explanations()
      const {
    return latest_security_style_explanations_;
  }

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override {
    latest_security_style_ = web_contents()->GetDelegate()->GetSecurityStyle(
        web_contents(), &latest_security_style_explanations_);
  }

 private:
  blink::SecurityStyle latest_security_style_;
  content::SecurityStyleExplanations latest_security_style_explanations_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStyleTestObserver);
};

}  // namespace

// Test that the security indicator gets updated on a Safe Browsing
// interstitial triggered by a subresource. Regression test for
// https://crbug.com/659713.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityStateDowngradedForSubresourceInterstitial) {
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  SecurityStyleTestObserver observer(error_tab);
  // The security indicator should be downgraded while the interstitial shows.
  SetupThreatIframeWarningAndNavigate();
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);
  EXPECT_EQ(blink::SecurityStyle::kInsecureBroken,
            observer.latest_security_style());
  // Security style summary for Developer Tools should contain a warning.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            observer.latest_security_style_explanations().summary);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  ExpectSecurityIndicatorDowngrade(post_tab, 0u);
}

// Test that the security indicator does not stay downgraded after
// clicking back from a Safe Browsing interstitial. Regression test for
// https://crbug.com/659709.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityStateGoBack) {
  // Navigate to a page so that there is somewhere to go back to.
  GURL start_url = GURL(kUnrelatedUrl);
  ui_test_utils::NavigateToURL(browser(), start_url);

  // The security indicator should be downgraded while the interstitial shows.
  GURL bad_url = embedded_test_server()->GetURL(kEmptyPage);
  SetupWarningAndNavigate(browser());
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);
  content::NavigationEntry* entry =
      error_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  ASSERT_EQ(bad_url, entry->GetURL());

  // Go back.
  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  // The security indicator should *not* still be downgraded after going back.
  AssertNoInterstitial(true);
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  entry = post_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(start_url, entry->GetURL());
  ExpectNoSecurityIndicatorDowngrade(post_tab);

  ClearBadURL(bad_url);
  // Navigate to the URL that the interstitial was on, and check that it
  // is no longer marked as dangerous.
  ui_test_utils::NavigateToURL(browser(), bad_url);
  ExpectNoSecurityIndicatorDowngrade(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Test that the security indicator does not stay downgraded after
// clicking back from a Safe Browsing interstitial triggered by a
// subresource. Regression test for https://crbug.com/659709.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityStateGoBackOnSubresourceInterstitial) {
  // Navigate to a page so that there is somewhere to go back to.
  GURL start_url = embedded_test_server()->GetURL(kEmptyPage);
  ui_test_utils::NavigateToURL(browser(), start_url);

  // The security indicator should be downgraded while the interstitial
  // shows. Load a cross-origin iframe to be sure that the main frame origin
  // (rather than the subresource origin) is being added and removed from the
  // whitelist; this is a regression test for https://crbug.com/710955.
  GURL bad_iframe_url;
  GURL main_url =
      SetupCrossOriginThreatIframeWarningAndNavigate(&bad_iframe_url);
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // Go back.
  EXPECT_EQ(VISIBLE, GetVisibility("primary-button"));
  EXPECT_EQ(HIDDEN, GetVisibility("details"));
  EXPECT_EQ(HIDDEN, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(Click("details-button"));
  EXPECT_EQ(VISIBLE, GetVisibility("details"));
  EXPECT_EQ(VISIBLE, GetVisibility("proceed-link"));
  EXPECT_EQ(HIDDEN, GetVisibility("error-code"));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));

  // The security indicator should *not* still be downgraded after going back.
  AssertNoInterstitial(true);
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  content::NavigationEntry* entry = post_tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(start_url, entry->GetURL());
  ExpectNoSecurityIndicatorDowngrade(post_tab);

  // Clear the malicious subresource URL, and check that the hostname of the
  // interstitial is no longer marked as Dangerous.
  ClearBadURL(bad_iframe_url);
  ui_test_utils::NavigateToURL(browser(), main_url);
  ExpectNoSecurityIndicatorDowngrade(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Test that the security indicator is downgraded after clicking through a
// Safe Browsing interstitial.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityState_HTTP) {
  // The security indicator should be downgraded while the interstitial shows.
  SetupWarningAndNavigate(browser());
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  ExpectSecurityIndicatorDowngrade(post_tab, 0u);
}

// Test that the security indicator is downgraded even if the website has valid
// HTTPS (meaning that the SB state overrides the HTTPS state).
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityState_ValidHTTPS) {
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  SecurityStyleTestObserver observer(error_tab);

  // The security indicator should be downgraded while the interstitial shows.
  SetupWarningAndNavigateToValidHTTPS();
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // Security style summary for Developer Tools should contain a warning.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            observer.latest_security_style_explanations().summary);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  ExpectSecurityIndicatorDowngrade(post_tab, 0u);

  // Security style summary for Developer Tools should still contain a warning.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            observer.latest_security_style_explanations().summary);
}

// Test that the security indicator is still downgraded after two interstitials
// are shown in a row (one for Safe Browsing, one for invalid HTTPS).
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       SecurityState_InvalidHTTPS) {
  // The security indicator should be downgraded while the interstitial shows.
  SetupWarningAndNavigateToInvalidHTTPS();
  WebContents* error_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(error_tab);
  ExpectSecurityIndicatorDowngrade(error_tab, 0u);

  // The security indicator should still be downgraded post-interstitial.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  WebContents* post_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(post_tab);
  // TODO(felt): Sometimes the cert status here is 0u, which is wrong.
  // Filed https://crbug.com/641187 to investigate.
  ExpectSecurityIndicatorDowngrade(post_tab, net::CERT_STATUS_INVALID);
}

// Test that no safe browsing interstitial will be shown, if URL matches
// enterprise safe browsing whitelist domains.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       VerifyEnterpriseWhitelist) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  // Add test server domain into the enterprise whitelist.
  base::ListValue whitelist;
  whitelist.AppendString(url.host());
  browser()->profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains,
                                        whitelist);

  SetURLThreatType(url, testing::get<0>(GetParam()));
  ui_test_utils::NavigateToURL(browser(), url);
  base::RunLoop().RunUntilIdle();
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForRenderFrameReady(contents->GetMainFrame()));
  EXPECT_FALSE(IsShowingInterstitial(contents));
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageBrowserTestWithThreatTypeAndIsolationSetting,
    SafeBrowsingBlockingPageBrowserTest,
    testing::Combine(
        testing::Values(SB_THREAT_TYPE_URL_MALWARE,  // Threat types
                        SB_THREAT_TYPE_URL_PHISHING,
                        SB_THREAT_TYPE_URL_UNWANTED),
        testing::Bool()));  // If isolate all sites for testing.

// Tests that commands work in a subframe triggered interstitial if a different
// interstitial has been shown previously on the same webcontents. Regression
// test for crbug.com/1021334
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       IframeProceedAfterMainFrameInterstitial) {
  // Navigate to a site that triggers an interstitial due to a bad main frame
  // URL.
  ui_test_utils::NavigateToURL(browser(),
                               GURL(kChromeUISafeBrowsingMatchMalwareUrl));
  EXPECT_TRUE(WaitForReady(browser()));
  EXPECT_TRUE(ClickAndWaitForDetach("primary-button"));
  AssertNoInterstitial(false);

  // Navigate to a site that triggers an interstitial due to a bad iframe.
  GURL url = SetupThreatIframeWarningAndNavigate();

  // Commands should work.
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);  // Assert the interstitial is gone

  EXPECT_EQ(url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Check back and forward work correctly after clicking through an interstitial.
#if (defined(OS_MAC) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
// TODO(crbug.com/1132307): Address flay failure.
#define MAYBE_NavigatingBackAndForth DISABLED_NavigatingBackAndForth
#else
#define MAYBE_NavigatingBackAndForth NavigatingBackAndForth
#endif
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest,
                       MAYBE_NavigatingBackAndForth) {
  // Load a safe page. (Despite the name, kMaliciousPage is not the page flagged
  // as bad in this test.)
  GURL safe_url(embedded_test_server()->GetURL(kMaliciousPage));
  ui_test_utils::NavigateToURL(browser(), safe_url);
  // Navigate to a site that triggers a warning and click through it.
  const GURL bad_url = SetupWarningAndNavigate(browser());
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  // Go back and check we are back on the safe site.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver back_observer(contents);
  contents->GetController().GoBack();
  back_observer.Wait();
  EXPECT_EQ(safe_url, contents->GetURL());
  // Check forward takes us back to the flagged site with no interstitial.
  content::TestNavigationObserver forward_observer(contents);
  contents->GetController().GoForward();
  forward_observer.Wait();
  WaitForReady(browser());
  AssertNoInterstitial(true);
  EXPECT_EQ(bad_url, contents->GetURL());
}

// Toggle the SBER opt in checkbox and check it enables reporting.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ToggleSBEROn) {
  // The extended reporting opt-in is presented in the interstitial for malware,
  // phishing, and UwS threats.
  const bool expect_threat_details =
      SafeBrowsingBlockingPage::ShouldReportThreatDetails(
          testing::get<0>(GetParam()));
  scoped_refptr<content::MessageLoopRunner> threat_report_sent_runner(
      new content::MessageLoopRunner);
  if (expect_threat_details)
    SetReportSentCallback(threat_report_sent_runner->QuitClosure());

  // Initially disable SBER.
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), false);
  ASSERT_FALSE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  // Navigate to a site that triggers a warning.
  const GURL url = SetupWarningAndNavigate(browser());
  // Click the checkbox and click through the warning.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  // Check preference is now enabled.
  EXPECT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  // If a report should be sent for this type of page, check we got one.
  if (expect_threat_details) {
    threat_report_sent_runner->Run();
    std::string serialized = GetReportSent();
    ClientSafeBrowsingReportRequest report;
    ASSERT_TRUE(report.ParseFromString(serialized));
    EXPECT_TRUE(report.complete());
    EXPECT_EQ(url.spec(), report.page_url());
  }
}

// Toggle the SBER opt in checkbox and check it disables reporting.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageBrowserTest, ToggleSBEROff) {
  // Initially enable SBER.
  SetExtendedReportingPrefForTests(browser()->profile()->GetPrefs(), true);
  ASSERT_TRUE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
  // Navigate to a site that triggers a warning.
  const GURL url = SetupWarningAndNavigate(browser());
  // Click the checkbox and click through the warning.
  EXPECT_EQ(VISIBLE, GetVisibility("extended-reporting-opt-in"));
  EXPECT_TRUE(Click("opt-in-checkbox"));
  EXPECT_TRUE(ClickAndWaitForDetach("proceed-link"));
  AssertNoInterstitial(true);
  // Check preference is now disabled.
  EXPECT_FALSE(IsExtendedReportingEnabled(*browser()->profile()->GetPrefs()));
}

class SafeBrowsingBlockingPageDelayedWarningBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          testing::tuple<bool /* IsolateAllSitesForTesting */,
                         bool /* Show warning on mouse click */>> {
 public:
  SafeBrowsingBlockingPageDelayedWarningBrowserTest() = default;

  void SetUp() override {
    if (warning_on_mouse_click_enabled()) {
      const std::map<std::string, std::string> parameters{{"mouse", "true"}};
      std::vector<base::test::ScopedFeatureList::FeatureAndParams>
          enabled_features{base::test::ScopedFeatureList::FeatureAndParams(
                               kDelayedWarnings, parameters),
                           base::test::ScopedFeatureList::FeatureAndParams(
                               blink::features::kPortals, {}),
                           base::test::ScopedFeatureList::FeatureAndParams(
                               blink::features::kPortalsCrossOrigin, {})};
      scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{kDelayedWarnings, blink::features::kPortals,
                                blink::features::kPortalsCrossOrigin},
          /*disabled_features=*/{});
    }
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (testing::get<0>(GetParam()))
      content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    SafeBrowsingUserInteractionObserver::
        ResetSuspiciousSiteReporterExtensionIdForTesting();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager());
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager());
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

  static bool TypeAndWaitForInterstitial(Browser* browser) {
    // Type something. An interstitial should be shown.
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.text[0] = 'a';
    content::RenderWidgetHost* rwh = contents->GetRenderViewHost()->GetWidget();
    rwh->ForwardKeyboardEvent(event);
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static void MouseClick(Browser* browser) {
    blink::WebMouseEvent event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.button = blink::WebMouseEvent::Button::kLeft;
    event.SetPositionInWidget(100, 100);
    event.click_count = 1;
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::RenderWidgetHost* rwh = contents->GetRenderViewHost()->GetWidget();
    rwh->ForwardMouseEvent(event);
  }

  static bool MouseClickAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    MouseClick(browser);
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static bool FullscreenAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    const char* const kScript = "document.body.webkitRequestFullscreen()";
    EXPECT_TRUE(content::ExecuteScript(contents, kScript));
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static bool RequestPermissionAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    const char* const kScript = "Notification.requestPermission(function(){})";
    EXPECT_TRUE(content::ExecuteScript(contents, kScript));
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

  static bool RequestDesktopCaptureAndWaitForInterstitial(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    const char* const kScript = "navigator.mediaDevices.getDisplayMedia()";
    EXPECT_TRUE(content::ExecuteScript(contents, kScript));
    observer.WaitForNavigationFinished();
    return WaitForReady(browser);
  }

 protected:
  // Initiates a download and waits for it to be completed or cancelled.
  static void DownloadAndWaitForNavigation(Browser* browser) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(contents);
    content::WebContentsConsoleObserver console_observer(contents);
    console_observer.SetPattern(
        "A SafeBrowsing warning is pending on this page*");

    ui_test_utils::NavigateToURL(
        browser, GURL("data:application/octet-stream;base64,SGVsbG8="));
    observer.WaitForNavigationFinished();
    console_observer.Wait();

    ASSERT_EQ(1u, console_observer.messages().size());
  }

  void NavigateAndAssertNoInterstitial() {
    // Use a page that contains an iframe so that we can test both top frame
    // and subresource warnings.
    const GURL top_frame = embedded_test_server()->GetURL("/iframe.html");
    SetURLThreatType(top_frame, SB_THREAT_TYPE_URL_PHISHING);
    const GURL iframe = embedded_test_server()->GetURL("/title1.html");
    SetURLThreatType(iframe, SB_THREAT_TYPE_URL_PHISHING);

    ui_test_utils::NavigateToURL(browser(), top_frame);
    AssertNoInterstitial(browser(), true);
  }

  bool warning_on_mouse_click_enabled() const {
    return testing::get<1>(GetParam());
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->SetURLThreatType(url, threat_type);
  }

 protected:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Installs an extension and returns its ID.
  std::string InstallTestExtension() {
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII("theme.crx"));
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(service);

    extensions::ChromeExtensionTestNotificationObserver observer(browser());
    observer.Watch(extensions::NOTIFICATION_CRX_INSTALLER_DONE,
                   content::Source<extensions::CrxInstaller>(installer.get()));

    installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
    installer->set_install_immediately(true);
    installer->set_allow_silent_install(true);
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowedInTest);
    installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);
    installer->InstallCrx(path);
    observer.Wait();
    return observer.last_loaded_extension_id();
  }
#endif

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;
  TestThreatDetailsFactory details_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageDelayedWarningBrowserTest);
};

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       NoInteraction_WarningNotShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Navigate away without interacting with the page.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       NotPhishing_WarningNotDelayed) {
  base::HistogramTester histograms;

  // Navigate to a non-phishing page. The warning should not be delayed.
  const GURL url = embedded_test_server()->GetURL("/empty.html");
  SetURLThreatType(url, SB_THREAT_TYPE_URL_MALWARE);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WaitForReady(browser()));

  // Navigate to about:blank to "flush" metrics, if any.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 0);
}

// Close the tab while a user interaction observer is attached to the tab. It
// shouldn't crash.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       CloseTab_ShouldNotCrash) {
  base::HistogramTester histograms;
  chrome::NewTab(browser());
  NavigateAndAssertNoInterstitial();
  chrome::CloseTab(browser());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_WarningShown) {
  constexpr int kTimeOnPage = 10;
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Inject a test clock to test the histogram that records the time on the
  // delayed warning page before the warning shows or the user leaves the page.
  base::SimpleTestClock clock;
  SafeBrowsingUserInteractionObserver* observer =
      SafeBrowsingUserInteractionObserver::FromWebContents(web_contents);
  ASSERT_TRUE(observer);
  clock.SetNow(observer->GetCreationTimeForTesting());
  observer->SetClockForTesting(&clock);
  clock.Advance(base::TimeDelta::FromSeconds(kTimeOnPage));

  // Type something. An interstitial should be shown.
  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningShownOnKeypress, 1);
  histograms.ExpectUniqueTimeSample(kDelayedWarningsTimeOnPageHistogram,
                                    base::TimeDelta::FromSeconds(kTimeOnPage),
                                    1);
}

// Same as KeyPress_WarningShown, but user disabled URL elision by enabling
// "Always Show Full URLs" option. A separate histogram must be recorded.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_WarningShown_UrlElisionDisabled) {
  constexpr int kTimeOnPage = 10;
  browser()->profile()->GetPrefs()->SetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox, true);

  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Inject a test clock to test the histogram that records the time on the
  // delayed warning page before the warning shows or the user leaves the page.
  base::SimpleTestClock clock;
  SafeBrowsingUserInteractionObserver* observer =
      SafeBrowsingUserInteractionObserver::FromWebContents(web_contents);
  ASSERT_TRUE(observer);
  clock.SetNow(observer->GetCreationTimeForTesting());
  observer->SetClockForTesting(&clock);
  clock.Advance(base::TimeDelta::FromSeconds(kTimeOnPage));

  // Type something. An interstitial should be shown.
  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            web_contents->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsWithElisionDisabledHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsWithElisionDisabledHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsWithElisionDisabledHistogram,
                               DelayedWarningEvent::kWarningShownOnKeypress, 1);
  histograms.ExpectUniqueTimeSample(
      kDelayedWarningsTimeOnPageWithElisionDisabledHistogram,
      base::TimeDelta::FromSeconds(kTimeOnPage), 1);
}

// Same as KeyPress_WarningShown_UrlElisionDisabled, but user disabled URL
// elision by installing Suspicious Site Reporter extension.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_WarningShown_UrlElisionDisabled_Extension) {
  const std::string extension_id = InstallTestExtension();
  SafeBrowsingUserInteractionObserver::
      SetSuspiciousSiteReporterExtensionIdForTesting(extension_id.c_str());

  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Type something. An interstitial should be shown.
  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsWithElisionDisabledHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsWithElisionDisabledHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsWithElisionDisabledHistogram,
                               DelayedWarningEvent::kWarningShownOnKeypress, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_ESC_WarningNotShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press ESC key. The interstitial should not be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  // Browser expects a non-synthesized event to have an os_event. Make the
  // browser ignore this event instead.
  event.skip_in_browser = true;
  contents->GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
  AssertNoInterstitial(browser(), false);

  // Navigate to about:blank twice to "flush" metrics, if any. The delayed
  // warning user interaction observer may not have been deleted after the first
  // navigation.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_ModifierKey_WarningNotShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press CTRL+A key. The interstitial should not be shown because we ignore
  // the CTRL modifier unless it's CTRL+C or CTRL+V.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_A;
  // Browser expects a non-synthesized event to have an os_event. Make the
  // browser ignore this event instead.
  event.skip_in_browser = true;
  contents->GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
  AssertNoInterstitial(browser(), false);

  // Navigate to about:blank twice to "flush" metrics, if any. The delayed
  // warning user interaction observer may not have been deleted after the first
  // navigation.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_CtrlC_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press CTRL+C. The interstitial should be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents);

  content::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kControlKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_C;
  event.native_key_code = ui::VKEY_C;
  // We don't set event.skip_in_browser = true here because the event will be
  // consumed by UserInteractionObserver and not passed to the browser.
  contents->GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);

  observer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForReady(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningShownOnKeypress, 1);
}

// Similar to KeyPress_ESC_WarningNotShown, but a character key is pressed after
// ESC. The warning should be shown.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       KeyPress_ESCAndCharacterKey_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Press ESC key. The interstitial should not be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  // Browser expects a non-synthesized event to have an os_event. Make the
  // browser ignore this event instead.
  event.skip_in_browser = true;
  contents->GetRenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
  base::RunLoop().RunUntilIdle();
  AssertNoInterstitial(browser(), false);
  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);

  // Now type something. The interstitial should be shown.
  EXPECT_TRUE(TypeAndWaitForInterstitial(browser()));
  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningShownOnKeypress, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       Fullscreen_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to enter fullscreen. An interstitial should be shown.
  EXPECT_TRUE(FullscreenAndWaitForInterstitial(browser()));
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsFullscreen());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(
      kDelayedWarningsHistogram,
      DelayedWarningEvent::kWarningShownOnFullscreenAttempt, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       PermissionRequest_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to request a notification permission. The prompt should be
  // cancelled and an interstitial should be shown.
  EXPECT_TRUE(RequestPermissionAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(
      kDelayedWarningsHistogram,
      DelayedWarningEvent::kWarningShownOnPermissionRequest, 1);
  histograms.ExpectTotalCount("Permissions.Action.Notifications", 1);
  histograms.ExpectBucketCount(
      "Permissions.Action.Notifications",
      static_cast<int>(permissions::PermissionAction::DENIED), 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       JavaScriptDialog_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to show a JavaScript dialog. The dialog should be
  // cancelled and an interstitial should be shown.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents);
  EXPECT_TRUE(content::ExecuteScript(contents, "alert('test')"));
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForReady(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(
      kDelayedWarningsHistogram,
      DelayedWarningEvent::kWarningShownOnJavaScriptDialog, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       DesktopCaptureRequest_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Page tries to request a desktop capture permission. The request should be
  // cancelled and an interstitial should be shown.
  EXPECT_TRUE(RequestDesktopCaptureAndWaitForInterstitial(browser()));
  EXPECT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsFullscreen());

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(
      kDelayedWarningsHistogram,
      DelayedWarningEvent::kWarningShownOnDesktopCaptureRequest, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       Paste_WarningShown) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Create a test context menu and send a paste command through it. This
  // should show the delayed interstitial.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents);
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(contents, contents->GetURL(), GURL(),
                                        GURL()));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_PASTE, 0);
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForReady(browser()));
  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningShownOnPaste, 1);
}

// The user clicks on the page. Feature isn't configured to show a warning on
// mouse clicks. We should record that the user interacted with the page, but
// shouldn't shown an interstitial.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       MouseClick_WarningNotShown) {
  if (warning_on_mouse_click_enabled()) {
    return;
  }
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Click on the page. An interstitial shouldn't be shown because the feature
  // parameter is off.
  MouseClick(browser());
  AssertNoInterstitial(browser(), false);

  // Navigate away to "flush" the metrics.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 3);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
  histograms.ExpectBucketCount(
      kDelayedWarningsHistogram,
      DelayedWarningEvent::kWarningNotTriggeredOnMouseClick, 1);
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       MouseClick_WarningShown) {
  if (!warning_on_mouse_click_enabled()) {
    return;
  }
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  // Click on the page. An interstitial should be shown because the feature
  // parameter is on.
  EXPECT_TRUE(MouseClickAndWaitForInterstitial(browser()));

  EXPECT_TRUE(ClickAndWaitForDetach(browser(), "primary-button"));
  AssertNoInterstitial(browser(), false);  // Assert the interstitial is gone
  EXPECT_EQ(GURL(url::kAboutBlankURL),     // Back to "about:blank"
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 2);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningShownOnMouseClick,
                               1);
}

// This test initiates a download when a warning is delayed. The download should
// be cancelled and the interstitial should not be shown.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       Download_CancelledWithNoInterstitial) {
  base::HistogramTester histograms;
  NavigateAndAssertNoInterstitial();

  DownloadAndWaitForNavigation(browser());
  AssertNoInterstitial(browser(), false);

  // Navigate away to "flush" the metrics.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 3);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kDownloadCancelled, 1);
}

// This test navigates to a page with password form and submits a password. The
// warning should be delayed, the "Save Password" bubble should not be shown,
// and a histogram entry for the password save should be recorded.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageDelayedWarningBrowserTest,
                       PasswordSaveDisabled) {
  base::HistogramTester histograms;

  // Navigate to the page.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer1(contents);
  const GURL url =
      embedded_test_server()->GetURL("/password/password_form.html");
  SetURLThreatType(url, SB_THREAT_TYPE_URL_PHISHING);
  ui_test_utils::NavigateToURL(browser(), url);
  observer1.Wait();

  // Submit a password.
  NavigationObserver observer2(contents);
  std::unique_ptr<BubbleObserver> prompt_observer(new BubbleObserver(contents));
  std::string fill_and_submit =
      "document.getElementById('retry_password_field').value = 'pw';"
      "document.getElementById('retry_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(contents, fill_and_submit));
  observer2.Wait();
  EXPECT_FALSE(prompt_observer->IsSavePromptShownAutomatically());
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());
  AssertNoInterstitial(browser(), false);

  // Navigate away to "flush" the metrics.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histograms.ExpectTotalCount(kDelayedWarningsHistogram, 3);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kPageLoaded, 1);
  histograms.ExpectBucketCount(kDelayedWarningsHistogram,
                               DelayedWarningEvent::kWarningNotShown, 1);
  histograms.ExpectBucketCount(
      kDelayedWarningsHistogram,
      DelayedWarningEvent::kPasswordSaveOrAutofillDenied, 1);
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageWithDelayedWarningsBrowserTest,
    SafeBrowsingBlockingPageDelayedWarningBrowserTest,
    testing::Combine(
        testing::Values(false, true), /* IsolateAllSitesForTesting */
        testing::Values(false, true) /* Show warning on mouse click */));

// Test that SafeBrowsingBlockingPage properly decodes IDN URLs that are
// displayed.
class SafeBrowsingBlockingPageIDNTest
    : public SecurityInterstitialIDNTest,
      public testing::WithParamInterface<testing::tuple<bool, SBThreatType>> {
 protected:
  // SecurityInterstitialIDNTest implementation
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    SafeBrowsingUIManager::CreateWhitelistForTesting(contents);
    const bool is_subresource = testing::get<0>(GetParam());

    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    SafeBrowsingBlockingPage::UnsafeResource resource;

    resource.url = request_url;
    resource.is_subresource = is_subresource;
    resource.threat_type = testing::get<1>(GetParam());
    resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
        contents->GetMainFrame()->GetProcess()->GetID(),
        contents->GetMainFrame()->GetRoutingID());
    resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER3;

    return SafeBrowsingBlockingPage::CreateBlockingPage(
        sb_service->ui_manager().get(), contents,
        is_subresource ? GURL("http://mainframe.example.com/") : request_url,
        resource, true);
  }
};

// TODO(crbug.com/1039367): VerifyIDNDecoded does not work with committed
// interstitials, this test should be re-enabled once it is adapted.
IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageIDNTest,
                       DISABLED_SafeBrowsingBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageIDNTestWithThreatType,
    SafeBrowsingBlockingPageIDNTest,
    testing::Combine(testing::Values(false, true),
                     testing::Values(SB_THREAT_TYPE_URL_MALWARE,
                                     SB_THREAT_TYPE_URL_PHISHING,
                                     SB_THREAT_TYPE_URL_UNWANTED)));

// Tests with the <portal> tag.
class SafeBrowsingBlockingPageDelayedWarningWithPortalBrowserTest
    : public SafeBrowsingBlockingPageDelayedWarningBrowserTest {
 public:
  SafeBrowsingBlockingPageDelayedWarningWithPortalBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kDelayedWarnings, blink::features::kPortals,
                              blink::features::kPortalsCrossOrigin},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SafeBrowsingBlockingPageDelayedWarningWithPortalBrowserTest,
    testing::Combine(
        testing::Values(false, true), /* IsolateAllSitesForTesting */
        testing::Values(false, true) /* Show warning on mouse click */));

// Tests that if a page embeds a portal whose contents are considered dangerous
// by Safe Browsing, the embedder is also treated as dangerous, and the
// interstitial isn't delayed. This is similar to
// PortalBrowserTest.EmbedderOfDangerousPortalConsideredDangerous.
IN_PROC_BROWSER_TEST_P(
    SafeBrowsingBlockingPageDelayedWarningWithPortalBrowserTest,
    Portal_WarningNotDelayed) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL dangerous_url(
      embedded_test_server()->GetURL("evil.com", "/title2.html"));
  SetURLThreatType(dangerous_url, SB_THREAT_TYPE_URL_PHISHING);

  ui_test_utils::NavigateToURL(browser(), main_url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(contents);
  ASSERT_TRUE(content::ExecJs(
      contents,
      content::JsReplace("let portal = document.createElement('portal');"
                         "portal.src = $1;"
                         "document.body.appendChild(portal);",
                         dangerous_url)));
  observer.WaitForNavigationFinished();
  // The interstitial should be shown immediately.
  EXPECT_TRUE(WaitForReady(browser()));
  EXPECT_TRUE(IsShowingInterstitial(contents));
}

class SafeBrowsingBlockingPageEnhancedProtectionMessageTest
    : public policy::PolicyTest {
 public:
  SafeBrowsingBlockingPageEnhancedProtectionMessageTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kEnhancedProtectionMessageInInterstitials);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestUIManager(new FakeSafeBrowsingUIManager());
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager());
    SafeBrowsingService::RegisterFactory(&factory_);
    SafeBrowsingBlockingPage::RegisterFactory(&blocking_page_factory_);
    ThreatDetails::RegisterFactory(&details_factory_);
  }

 protected:
  void SetupWarningAndNavigateToURL(GURL url, Browser* browser) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->SetURLThreatType(url, SB_THREAT_TYPE_URL_MALWARE);

    ui_test_utils::NavigateToURL(browser, url);
    EXPECT_TRUE(WaitForReady(browser));
  }

 private:
  TestSafeBrowsingServiceFactory factory_;
  TestSafeBrowsingBlockingPageFactory blocking_page_factory_;
  TestThreatDetailsFactory details_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(
      SafeBrowsingBlockingPageEnhancedProtectionMessageTest);
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageEnhancedProtectionMessageTest,
                       VerifyEnhancedProtectionMessageShownAndClicked) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  SetupWarningAndNavigateToURL(embedded_test_server()->GetURL("/empty.html"),
                               browser());

  // Check SBER opt in is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        browser(), "extended-reporting-opt-in"));
  // Check enhanced protection message is shown.
  EXPECT_EQ(VISIBLE, ::safe_browsing::GetVisibility(
                         browser(), "enhanced-protection-message"));
  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(interstitial_tab);
  ASSERT_TRUE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();
  // Click the enhanced protection link.
  EXPECT_TRUE(Click(browser(), "enhanced-protection-link"));

  nav_observer.Wait();

  // There are two tabs open.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  // The second tab is visible.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Assert the interstitial is not present in the foreground tab.
  ASSERT_FALSE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Foreground tab displays the setting page.
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(GURL(kEnhancedProtectionUrl), new_tab->GetURL());

  // Interstitial should still display in the background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(interstitial_tab,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageEnhancedProtectionMessageTest,
                       VerifyEnhancedProtectionMessageNotShownAlreadyInEp) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  SetupWarningAndNavigateToURL(embedded_test_server()->GetURL("/empty.html"),
                               browser());
  EXPECT_TRUE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  // Check enhanced protection message is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        browser(), "enhanced-protection-message"));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBlockingPageEnhancedProtectionMessageTest,
                       VerifyEnhancedProtectionMessageNotShownManaged) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kSafeBrowsingProtectionLevel,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(/* standard protection */ 1), nullptr);
  UpdateProviderPolicy(policies);
  SetupWarningAndNavigateToURL(embedded_test_server()->GetURL("/empty.html"),
                               browser());

  EXPECT_TRUE(IsShowingInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents()));
  // Check enhanced protection message is not shown.
  EXPECT_EQ(HIDDEN, ::safe_browsing::GetVisibility(
                        browser(), "enhanced-protection-message"));
}

}  // namespace safe_browsing
