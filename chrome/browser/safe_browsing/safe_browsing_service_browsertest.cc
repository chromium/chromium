// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a safebrowsing service using test safebrowsing database
// and a test protocol manager. It is used to test logics in safebrowsing
// service.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/thread_test_helper.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_database.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "net/cookies/cookie_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if !BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#error This test requires SAFE_BROWSING_DB_LOCAL.
#endif

using content::WebContents;
using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

namespace safe_browsing {

namespace {

const char kEmptyPage[] = "/empty.html";
const char kMalwareFile[] = "/downloads/dangerous/dangerous.exe";
const char kMalwarePage[] = "/safe_browsing/malware.html";
const char kMalwareJsRequestPage[] = "/safe_browsing/malware_js_request.html";
const char kMalwareWebSocketPath[] = "/safe_browsing/malware-ws";
const char kPrefetchMalwarePage[] = "/safe_browsing/prefetch_malware.html";
const char kBillingInterstitialPage[] = "/safe_browsing/billing.html";

// This is not a proper WebSocket server. It does the minimum necessary to make
// the browser think the handshake succeeded.
// TODO(ricea): This could probably go in //net somewhere.
class QuasiWebSocketHttpResponse : public net::test_server::HttpResponse {
 public:
  explicit QuasiWebSocketHttpResponse(
      const net::test_server::HttpRequest& request) {
    const auto it = request.headers.find("Sec-WebSocket-Key");
    const std::string key =
        it == request.headers.end() ? std::string() : it->second;
    accept_hash_ = base::Base64Encode(
        base::SHA1HashString(key + net::websockets::kWebSocketGuid));
  }
  ~QuasiWebSocketHttpResponse() override {}

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    base::StringPairs response_headers = {
        {"Upgrade", "WebSocket"},
        {"Connection", "Upgrade"},
        {"Sec-WebSocket-Accept", accept_hash_}};

    delegate->SendResponseHeaders(net::HTTP_SWITCHING_PROTOCOLS,
                                  "WebSocket Protocol Handshake",
                                  response_headers);
    // Never call FinishResponse(). The connection should stay open.
  }

 private:
  std::string accept_hash_;
};

std::unique_ptr<net::test_server::HttpResponse> HandleWebSocketRequests(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kMalwareWebSocketPath)
    return nullptr;

  return std::make_unique<QuasiWebSocketHttpResponse>(request);
}

enum class ContextType { kWindow, kWorker, kSharedWorker, kServiceWorker };

enum class JsRequestType {
  kWebSocket,
  // Load a URL using the Fetch API.
  kFetch
};

struct JsRequestTestParam {
  JsRequestTestParam(ContextType in_context_type, JsRequestType in_request_type)
      : context_type(in_context_type), request_type(in_request_type) {}

  ContextType context_type;
  JsRequestType request_type;
};

std::string ContextTypeToString(ContextType context_type) {
  switch (context_type) {
    case ContextType::kWindow:
      return "window";
    case ContextType::kWorker:
      return "worker";
    case ContextType::kSharedWorker:
      return "shared-worker";
    case ContextType::kServiceWorker:
      return "service-worker";
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string JsRequestTypeToString(JsRequestType request_type) {
  switch (request_type) {
    case JsRequestType::kWebSocket:
      return "websocket";
    case JsRequestType::kFetch:
      return "fetch";
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Return a new URL with ?contextType=<context_type>&requestType=<request_type>
// appended.
GURL AddJsRequestParam(const GURL& base_url, const JsRequestTestParam& param) {
  GURL::Replacements add_query;
  std::string query =
      "contextType=" + ContextTypeToString(param.context_type) +
      "&requestType=" + JsRequestTypeToString(param.request_type);
  add_query.SetQueryStr(query);
  return base_url.ReplaceComponents(add_query);
}

// Given the URL of the malware_js_request.html page, calculate the URL of the
// WebSocket it will fetch.
GURL ConstructWebSocketURL(const GURL& main_url) {
  // This constructs the URL with the same logic as malware_js_request.html.
  GURL resolved = main_url.Resolve(kMalwareWebSocketPath);
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("ws");
  return resolved.ReplaceComponents(replace_scheme);
}

GURL ConstructJsRequestURL(const GURL& base_url, JsRequestType request_type) {
  switch (request_type) {
    case JsRequestType::kWebSocket:
      return ConstructWebSocketURL(base_url);
    case JsRequestType::kFetch:
      return base_url.Resolve(kMalwarePage);
  }
  NOTREACHED_IN_MIGRATION();
  return GURL();
}

// Navigate |browser| to |url| and wait for the title to change to "NOT BLOCKED"
// or "ERROR". This is specific to the tests using malware_js_request.html.
// Returns the new title.
std::string JsRequestTestNavigateAndWaitForTitle(Browser* browser,
                                                 const GURL& url) {
  std::u16string expected_title = u"ERROR";
  content::TitleWatcher title_watcher(
      browser->tab_strip_model()->GetActiveWebContents(), expected_title);
  title_watcher.AlsoWaitForTitle(u"NOT BLOCKED");

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  return base::UTF16ToUTF8(title_watcher.WaitAndGetTitle());
}

class FakeSafeBrowsingUIManager : public TestSafeBrowsingUIManager {
 public:
  void MaybeReportSafeBrowsingHit(
      std::unique_ptr<safe_browsing::HitReport> hit_report,
      content::WebContents* web_contents) override {
    EXPECT_FALSE(got_hit_report_);
    got_hit_report_ = true;
    hit_report_ = *(hit_report.get());
    SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(std::move(hit_report),
                                                      web_contents);
  }

  void MaybeSendClientSafeBrowsingWarningShownReport(
      std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest> report,
      content::WebContents* web_contents) override {
    EXPECT_FALSE(got_warning_shown_report_);
    got_warning_shown_report_ = true;
    warning_shown_report_ = *(report.get());
    SafeBrowsingUIManager::MaybeSendClientSafeBrowsingWarningShownReport(
        std::move(report), web_contents);
  }

  bool got_hit_report_ = false;
  safe_browsing::HitReport hit_report_;
  bool got_warning_shown_report_ = false;
  safe_browsing::ClientSafeBrowsingReportRequest warning_shown_report_;

 private:
  ~FakeSafeBrowsingUIManager() override {}
};

class MockObserver : public SafeBrowsingUIManager::Observer {
 public:
  MockObserver() {}
  ~MockObserver() override {}
  MOCK_METHOD1(OnSafeBrowsingHit,
               void(const security_interstitials::UnsafeResource&));
};

MATCHER_P(IsUnsafeResourceFor, url, "") {
  return (arg.url.spec() == url.spec() &&
          arg.threat_type != SBThreatType::SB_THREAT_TYPE_SAFE);
}

class ServiceEnabledHelper : public base::ThreadTestHelper {
 public:
  ServiceEnabledHelper(
      SafeBrowsingService* service,
      bool enabled,
      scoped_refptr<base::SingleThreadTaskRunner> target_thread)
      : base::ThreadTestHelper(target_thread),
        service_(service),
        expected_enabled_(enabled) {}

  void RunTest() override {
    set_test_result(service_->enabled() == expected_enabled_);
  }

 private:
  ~ServiceEnabledHelper() override {}

  scoped_refptr<SafeBrowsingService> service_;
  const bool expected_enabled_;
};

class TestSBClient : public base::RefCountedThreadSafe<TestSBClient>,
                     public SafeBrowsingDatabaseManager::Client {
 public:
  TestSBClient()
      : threat_type_(SB_THREAT_TYPE_SAFE),
        safe_browsing_service_(g_browser_process->safe_browsing_service()) {}

  TestSBClient(const TestSBClient&) = delete;
  TestSBClient& operator=(const TestSBClient&) = delete;

  SBThreatType GetThreatType() const { return threat_type_; }

  void CheckDownloadUrl(const std::vector<GURL>& url_chain) {
    base::RunLoop loop;
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckDownloadUrl(url_chain,
                                                                     this);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
    }
    set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  void CheckBrowseUrl(const GURL& url) {
    base::RunLoop loop;
    SBThreatTypeSet threat_types = CreateSBThreatTypeSet(
        {SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_URL_MALWARE,
         SB_THREAT_TYPE_URL_UNWANTED, SB_THREAT_TYPE_BILLING});

    // The async CheckDone() hook will not be called when we have a synchronous
    // safe signal, handle it right away.
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckBrowseUrl(
            url, threat_types, this,
            CheckBrowseUrlType::kHashDatabase);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
    }
    set_quit_closure(loop.QuitWhenIdleClosure());
    loop.Run();
  }

 private:
  using enum SBThreatType;

  friend class base::RefCountedThreadSafe<TestSBClient>;
  ~TestSBClient() override = default;

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  // Called when the result of checking a download URL is known.
  void OnCheckDownloadUrlResult(const std::vector<GURL>& /* url_chain */,
                                SBThreatType threat_type) override {
    threat_type_ = threat_type;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
  }

  // Called when the result of checking a browse URL is known.
  void OnCheckBrowseUrlResult(const GURL& /* url */,
                              SBThreatType threat_type,
                              const ThreatMetadata& /* metadata */) override {
    threat_type_ = threat_type;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
  }

  void CheckDone() { std::move(quit_closure_).Run(); }

  SBThreatType threat_type_;
  raw_ptr<SafeBrowsingService> safe_browsing_service_;
  base::OnceClosure quit_closure_;
};

}  // namespace

// Tests the safe browsing blocking page in a browser.
class V4SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  V4SafeBrowsingServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{safe_browsing::
                                  kCreateWarningShownClientSafeBrowsingReports},
        /*disabled_features=*/{});
  }

  V4SafeBrowsingServiceTest(const V4SafeBrowsingServiceTest&) = delete;
  V4SafeBrowsingServiceTest& operator=(const V4SafeBrowsingServiceTest&) =
      delete;

  void SetUp() override {
    sb_factory_ = std::make_unique<TestSafeBrowsingServiceFactory>();
    sb_factory_->SetTestUIManager(new FakeSafeBrowsingUIManager());
    sb_factory_->UseV4LocalDatabaseManager();
    SafeBrowsingService::RegisterFactory(sb_factory_.get());

    store_factory_ = new TestV4StoreFactory();
    V4Database::RegisterStoreFactoryForTest(
        base::WrapUnique(store_factory_.get()));

    v4_db_factory_ = new TestV4DatabaseFactory();
    V4Database::RegisterDatabaseFactoryForTest(
        base::WrapUnique(v4_db_factory_.get()));

    v4_get_hash_factory_ = new TestV4GetHashProtocolManagerFactory();
    V4GetHashProtocolManager::RegisterFactory(
        base::WrapUnique(v4_get_hash_factory_.get()));

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();

    // Unregister test factories after InProcessBrowserTest::TearDown
    // (which destructs SafeBrowsingService).
    V4GetHashProtocolManager::RegisterFactory(nullptr);
    V4Database::RegisterDatabaseFactoryForTest(nullptr);
    V4Database::RegisterStoreFactoryForTest(nullptr);
    SafeBrowsingService::RegisterFactory(nullptr);
  }

  void MarkUrlForListIdUnexpired(const GURL& bad_url,
                                 const ListIdentifier& list_id,
                                 ThreatPatternType threat_pattern_type) {
    ThreatMetadata metadata;
    metadata.threat_pattern_type = threat_pattern_type;
    FullHashInfo full_hash_info =
        GetFullHashInfoWithMetadata(bad_url, list_id, metadata);
    while (!v4_db_factory_->IsReady()) {
      content::RunAllTasksUntilIdle();
    }
    v4_db_factory_->MarkPrefixAsBad(list_id, full_hash_info.full_hash);
    v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
  }

  // Sets up the prefix database and the full hash cache to match one of the
  // prefixes for the given URL and metadata.
  void MarkUrlForMalwareUnexpired(
      const GURL& bad_url,
      ThreatPatternType threat_pattern_type = ThreatPatternType::NONE) {
    MarkUrlForListIdUnexpired(bad_url, GetUrlMalwareId(), threat_pattern_type);
  }

  // Sets up the prefix database and the full hash cache to match one of the
  // prefixes for the given URL in the UwS store.
  void MarkUrlForUwsUnexpired(const GURL& bad_url) {
    MarkUrlForListIdUnexpired(bad_url, GetUrlUwsId(), ThreatPatternType::NONE);
  }

  // Sets up the prefix database and the full hash cache to match one of the
  // prefixes for the given URL in the phishing store.
  void MarkUrlForPhishingUnexpired(const GURL& bad_url,
                                   ThreatPatternType threat_pattern_type) {
    MarkUrlForListIdUnexpired(bad_url, GetUrlSocEngId(), threat_pattern_type);
  }

  // Sets up the prefix database and the full hash cache to match one of the
  // prefixes for the given URL in the malware binary store.
  void MarkUrlForMalwareBinaryUnexpired(const GURL& bad_url) {
    MarkUrlForListIdUnexpired(bad_url, GetUrlMalBinId(),
                              ThreatPatternType::NONE);
  }

  // Sets up the prefix database and the full hash cache to match one of the
  // prefixes for the given URL in the Billing store.
  void MarkUrlForBillingUnexpired(const GURL& bad_url) {
    MarkUrlForListIdUnexpired(bad_url, GetUrlBillingId(),
                              ThreatPatternType::NONE);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void SetUpOnMainThread() override {
    g_browser_process->safe_browsing_service()->ui_manager()->AddObserver(
        &observer_);
  }

  void TearDownOnMainThread() override {
    g_browser_process->safe_browsing_service()->ui_manager()->RemoveObserver(
        &observer_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleWebSocketRequests));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool ShowingInterstitialPage(Browser* browser) {
    WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
    return chrome_browser_interstitials::IsShowingInterstitial(contents);
  }

  bool ShowingInterstitialPage() { return ShowingInterstitialPage(browser()); }

  FakeSafeBrowsingUIManager* ui_manager() {
    return static_cast<FakeSafeBrowsingUIManager*>(
        g_browser_process->safe_browsing_service()->ui_manager().get());
  }
  bool got_hit_report() { return ui_manager()->got_hit_report_; }
  const safe_browsing::HitReport& hit_report() {
    return ui_manager()->hit_report_;
  }
  bool got_warning_shown_report() {
    return ui_manager()->got_warning_shown_report_;
  }
  const safe_browsing::ClientSafeBrowsingReportRequest& report() {
    return ui_manager()->warning_shown_report_;
  }

 protected:
  using enum SBThreatType;

  StrictMock<MockObserver> observer_;

 private:
  std::unique_ptr<TestSafeBrowsingServiceFactory> sb_factory_;
  // Owned by the V4Database.
  raw_ptr<TestV4DatabaseFactory, AcrossTasksDanglingUntriaged> v4_db_factory_;
  // Owned by the V4GetHashProtocolManager.
  raw_ptr<TestV4GetHashProtocolManagerFactory, AcrossTasksDanglingUntriaged>
      v4_get_hash_factory_;
  // Owned by the V4Database.
  raw_ptr<TestV4StoreFactory, AcrossTasksDanglingUntriaged> store_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;

#if defined(ADDRESS_SANITIZER)
  // TODO(lukasza): https://crbug.com/971820: Disallow renderer crashes once the
  // bug is fixed.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes_;
#endif
};

// Proceeding through an interstitial should cause it to get allowlisted for
// that user.
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, MalwareWithAllowlist) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);

  // After adding the URL to SafeBrowsing database and full hash cache, we
  // should see the interstitial page.
  MarkUrlForMalwareUnexpired(url);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Mock::VerifyAndClearExpectations(&observer_);
  // There should be an InterstitialPage.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  security_interstitials::SecurityInterstitialPage* interstitial =
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  ASSERT_TRUE(helper);
  // TODO(carlosil): 1 is CMD_PROCEED, this should be changed to the enum
  // values once CommandReceived is changed to accept integers.
  content::TestNavigationObserver observer(contents);
  interstitial->CommandReceived("1");
  observer.WaitForNavigationFinished();
  EXPECT_FALSE(ShowingInterstitialPage());

  // Navigate to kEmptyPage again -- should hit the allowlist this time.
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(ShowingInterstitialPage());
}

// This test confirms that prefetches don't themselves get the interstitial
// treatment.
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, Prefetch) {
  GURL url = embedded_test_server()->GetURL(kPrefetchMalwarePage);
  GURL malware_url = embedded_test_server()->GetURL(kMalwarePage);

  // Even though we have added this URI to the SafeBrowsing database and
  // full hash result, we should not see the interstitial page since the
  // only malware was a prefetch target.
  MarkUrlForMalwareUnexpired(malware_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  EXPECT_FALSE(got_warning_shown_report());
  Mock::VerifyAndClear(&observer_);

  // However, when we navigate to the malware page, we should still get
  // the interstitial.
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(malware_url)))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malware_url));
  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  EXPECT_TRUE(got_warning_shown_report());
  Mock::VerifyAndClear(&observer_);
}

// Ensure that the referrer information is preserved in the hit report.
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, MainFrameHitWithReferrer) {
  GURL first_url = embedded_test_server()->GetURL(kEmptyPage);
  GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  MarkUrlForMalwareUnexpired(bad_url);

  // Navigate to first, safe page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  EXPECT_FALSE(got_warning_shown_report());
  Mock::VerifyAndClear(&observer_);

  // Navigate to malware page, should show interstitial and have first page in
  // referrer.
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(bad_url)))
      .Times(1);

  NavigateParams params(browser(), bad_url, ui::PAGE_TRANSITION_LINK);
  params.referrer.url = first_url;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  EXPECT_EQ(bad_url, hit_report().malicious_url);
  EXPECT_EQ(bad_url, hit_report().page_url);
  EXPECT_EQ(first_url, hit_report().referrer_url);
  EXPECT_FALSE(hit_report().is_subresource);
  EXPECT_TRUE(got_warning_shown_report());
  EXPECT_EQ(bad_url, report().url());
  EXPECT_EQ(bad_url, report().page_url());
  EXPECT_EQ(first_url, report().referrer_url());
}

///////////////////////////////////////////////////////////////////////////////
// START: These tests use SafeBrowsingService::Client to directly interact with
// SafeBrowsingService.
///////////////////////////////////////////////////////////////////////////////
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, CheckDownloadUrl) {
  GURL badbin_url = embedded_test_server()->GetURL(kMalwareFile);
  std::vector<GURL> badbin_urls(1, badbin_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_urls);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  MarkUrlForMalwareBinaryUnexpired(badbin_url);

  client->CheckDownloadUrl(badbin_urls);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_URL_BINARY_MALWARE, client->GetThreatType());
}

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, CheckUnwantedSoftwareUrl) {
  const GURL bad_url = embedded_test_server()->GetURL(kMalwareFile);
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    // Since bad_url is not in database, it is considered to be
    // safe.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

    MarkUrlForUwsUnexpired(bad_url);

    // Now, the bad_url is not safe since it is added to download
    // database.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED, client->GetThreatType());
  }

  // The unwantedness should survive across multiple clients.
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED, client->GetThreatType());
  }

  // An unwanted URL also marked as malware should be flagged as malware.
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    MarkUrlForMalwareUnexpired(bad_url);

    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }
}

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, CheckBrowseUrl) {
  const GURL bad_url = embedded_test_server()->GetURL(kMalwareFile);
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    // Since bad_url is not in database, it is considered to be
    // safe.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

    MarkUrlForMalwareUnexpired(bad_url);

    // Now, the bad_url is not safe since it is added to download
    // database.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }

  // The unwantedness should survive across multiple clients.
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }

  // Adding the unwanted state to an existing malware URL should have no impact
  // (i.e. a malware hit should still prevail).
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    MarkUrlForUwsUnexpired(bad_url);

    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }
}

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, CheckBrowseUrlForBilling) {
  const GURL bad_url = embedded_test_server()->GetURL(kBillingInterstitialPage);
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    // Since the feature isn't enabled and the URL isn't in the database, it is
    // considered to be safe.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

    // Since bad_url is not in database, it is considered to be
    // safe.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

    MarkUrlForBillingUnexpired(bad_url);

    // Now, the bad_url is not safe since it is added to the database.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_BILLING, client->GetThreatType());
  }
}

class V4SafeBrowsingServiceWithAutoReloadTest
    : public V4SafeBrowsingServiceTest {
 public:
  V4SafeBrowsingServiceWithAutoReloadTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(embedder_support::kEnableAutoReload);
    V4SafeBrowsingServiceTest::SetUpCommandLine(command_line);
  }
};

// SafeBrowsing interstitials should disable autoreload timer.
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceWithAutoReloadTest,
                       AutoReloadDisabled) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  MarkUrlForMalwareUnexpired(url);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(ShowingInterstitialPage());
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* reloader = error_page::NetErrorAutoReloader::FromWebContents(contents);
  const std::optional<base::OneShotTimer>& timer =
      reloader->next_reload_timer_for_testing();
  EXPECT_EQ(std::nullopt, timer);
}

class V4SafeBrowsingServiceWarningShownCSBRRsDisabled
    : public V4SafeBrowsingServiceTest {
 public:
  V4SafeBrowsingServiceWarningShownCSBRRsDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        safe_browsing::kCreateWarningShownClientSafeBrowsingReports);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceWarningShownCSBRRsDisabled,
                       CheckWarningShownReportNotSent) {
  GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);
  MarkUrlForMalwareUnexpired(bad_url);

  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(bad_url)))
      .Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));
  EXPECT_TRUE(ShowingInterstitialPage());

  EXPECT_FALSE(got_warning_shown_report());
}

// Parameterised fixture to permit running the same test for Window and Worker
// scopes.
class V4SafeBrowsingServiceJsRequestNoInterstitialTest
    : public ::testing::WithParamInterface<JsRequestTestParam>,
      public V4SafeBrowsingServiceTest {};

IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceJsRequestNoInterstitialTest,
                       MalwareNotBlocked) {
  GURL base_url = embedded_test_server()->GetURL(kMalwareJsRequestPage);
  JsRequestTestParam param = GetParam();
  MarkUrlForMalwareUnexpired(
      ConstructJsRequestURL(base_url, param.request_type));

  // Load the parent page after marking the JS request as malware.
  auto new_title = JsRequestTestNavigateAndWaitForTitle(
      browser(), AddJsRequestParam(base_url, param));

  EXPECT_EQ("NOT BLOCKED", new_title);
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  EXPECT_FALSE(got_warning_shown_report());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    V4SafeBrowsingServiceJsRequestNoInterstitialTest,
    ::testing::Values(
        JsRequestTestParam(ContextType::kWindow, JsRequestType::kWebSocket),
        JsRequestTestParam(ContextType::kWorker, JsRequestType::kWebSocket),
        JsRequestTestParam(ContextType::kSharedWorker,
                           JsRequestType::kWebSocket),
        JsRequestTestParam(ContextType::kServiceWorker,
                           JsRequestType::kWebSocket),
        JsRequestTestParam(ContextType::kWindow, JsRequestType::kFetch),
        JsRequestTestParam(ContextType::kWorker, JsRequestType::kFetch),
        JsRequestTestParam(ContextType::kSharedWorker, JsRequestType::kFetch),
        JsRequestTestParam(ContextType::kServiceWorker,
                           JsRequestType::kFetch)));

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, CheckDownloadUrlRedirects) {
  GURL original_url = embedded_test_server()->GetURL(kEmptyPage);
  GURL badbin_url = embedded_test_server()->GetURL(kMalwareFile);
  GURL final_url = embedded_test_server()->GetURL(kEmptyPage);
  std::vector<GURL> badbin_urls;
  badbin_urls.push_back(original_url);
  badbin_urls.push_back(badbin_url);
  badbin_urls.push_back(final_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_urls);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  MarkUrlForMalwareBinaryUnexpired(badbin_url);

  client->CheckDownloadUrl(badbin_urls);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_URL_BINARY_MALWARE, client->GetThreatType());
}

///////////////////////////////////////////////////////////////////////////////
// END: These tests use SafeBrowsingService::Client to directly interact with
// SafeBrowsingService.
///////////////////////////////////////////////////////////////////////////////

// TODO(vakh): Add test for UnwantedMainFrame.

class V4SafeBrowsingServiceMetadataTest
    : public V4SafeBrowsingServiceTest,
      public ::testing::WithParamInterface<ThreatPatternType> {
 public:
  V4SafeBrowsingServiceMetadataTest() {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kCreateWarningShownClientSafeBrowsingReports);
  }

  V4SafeBrowsingServiceMetadataTest(const V4SafeBrowsingServiceMetadataTest&) =
      delete;
  V4SafeBrowsingServiceMetadataTest& operator=(
      const V4SafeBrowsingServiceMetadataTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Irrespective of the threat_type classification, if the main frame URL is
// marked as Malware, an interstitial should be shown.
IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceMetadataTest, MalwareMainFrame) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  MarkUrlForMalwareUnexpired(url, GetParam());

  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // All types should show the interstitial.
  EXPECT_TRUE(ShowingInterstitialPage());

  EXPECT_TRUE(got_hit_report());
  EXPECT_EQ(url, hit_report().malicious_url);
  EXPECT_EQ(url, hit_report().page_url);
  EXPECT_EQ(GURL(), hit_report().referrer_url);
  EXPECT_FALSE(hit_report().is_subresource);
  EXPECT_TRUE(got_warning_shown_report());
  EXPECT_EQ(url, report().url());
  EXPECT_EQ(url, report().page_url());
  EXPECT_EQ(GURL(), report().referrer_url());
}

INSTANTIATE_TEST_SUITE_P(
    MaybeSetMetadata,
    V4SafeBrowsingServiceMetadataTest,
    testing::Values(ThreatPatternType::NONE,
                    ThreatPatternType::MALWARE_LANDING,
                    ThreatPatternType::MALWARE_DISTRIBUTION));

}  // namespace safe_browsing
