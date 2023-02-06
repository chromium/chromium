// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a safebrowsing service using test safebrowsing database
// and a test protocol manager. It is used to test logics in safebrowsing
// service.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/test/browser_test.h"

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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/metadata.pb.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kBlocklistResource[] = "/blacklisted/script.js";
const char kMaliciousResource[] = "/malware/script.js";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kEmptyPage[] = "/empty.html";
const char kMalwareFile[] = "/downloads/dangerous/dangerous.exe";
const char kMalwarePage[] = "/safe_browsing/malware.html";
const char kMalwareDelayedLoadsPage[] =
    "/safe_browsing/malware_delayed_loads.html";
const char kMalwareIFrame[] = "/safe_browsing/malware_iframe.html";
const char kMalwareImg[] = "/safe_browsing/malware_image.png";
const char kMalwareJsRequestPage[] = "/safe_browsing/malware_js_request.html";
const char kMalwareWebSocketPath[] = "/safe_browsing/malware-ws";
const char kNeverCompletesPath[] = "/never_completes";
const char kPrefetchMalwarePage[] = "/safe_browsing/prefetch_malware.html";
const char kBillingInterstitialPage[] = "/safe_browsing/billing.html";

std::unique_ptr<net::test_server::HttpResponse> HandleNeverCompletingRequests(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, kNeverCompletesPath,
                        base::CompareCase::SENSITIVE))
    return nullptr;
  return std::make_unique<net::test_server::HungResponse>();
}

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
    base::Base64Encode(
        base::SHA1HashString(key + net::websockets::kWebSocketGuid),
        &accept_hash_);
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

  NOTREACHED();
  return std::string();
}

std::string JsRequestTypeToString(JsRequestType request_type) {
  switch (request_type) {
    case JsRequestType::kWebSocket:
      return "websocket";
    case JsRequestType::kFetch:
      return "fetch";
  }

  NOTREACHED();
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
  NOTREACHED();
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
  void MaybeReportSafeBrowsingHit(const safe_browsing::HitReport& hit_report,
                                  content::WebContents* web_contents) override {
    EXPECT_FALSE(got_hit_report_);
    got_hit_report_ = true;
    hit_report_ = hit_report;
    SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(hit_report, web_contents);
  }

  bool got_hit_report_ = false;
  safe_browsing::HitReport hit_report_;

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
          arg.threat_type != SB_THREAT_TYPE_SAFE);
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

  std::string GetThreatHash() const { return threat_hash_; }

  void CheckDownloadUrl(const std::vector<GURL>& url_chain) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestSBClient::CheckDownloadUrlOnIOThread,
                                  this, url_chain));
    content::RunMessageLoop();  // Will stop in OnCheckDownloadUrlResult.
  }

  void CheckBrowseUrl(const GURL& url) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TestSBClient::CheckBrowseUrlOnIOThread, this, url));
    content::RunMessageLoop();  // Will stop in OnCheckBrowseUrlResult.
  }

  void CheckResourceUrl(const GURL& url) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TestSBClient::CheckResourceUrlOnIOThread, this, url));
    content::RunMessageLoop();  // Will stop in OnCheckResourceUrlResult.
  }

 private:
  friend class base::RefCountedThreadSafe<TestSBClient>;
  ~TestSBClient() override {}

  void CheckDownloadUrlOnIOThread(const std::vector<GURL>& url_chain) {
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckDownloadUrl(url_chain,
                                                                     this);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
    }
  }

  void CheckBrowseUrlOnIOThread(const GURL& url) {
    SBThreatTypeSet threat_types = CreateSBThreatTypeSet(
        {SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_URL_MALWARE,
         SB_THREAT_TYPE_URL_UNWANTED, SB_THREAT_TYPE_BILLING});

    // The async CheckDone() hook will not be called when we have a synchronous
    // safe signal, handle it right away.
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckBrowseUrl(
            url, threat_types, this,
            MechanismExperimentHashDatabaseCache::kNoExperiment);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
    }
  }

  void CheckResourceUrlOnIOThread(const GURL& url) {
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckResourceUrl(url, this);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
    }
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

  // Called when the result of checking a resource URL is known.
  void OnCheckResourceUrlResult(const GURL& /* url */,
                                SBThreatType threat_type,
                                const std::string& threat_hash) override {
    threat_type_ = threat_type;
    threat_hash_ = threat_hash;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestSBClient::CheckDone, this));
  }

  void CheckDone() { base::RunLoop::QuitCurrentWhenIdleDeprecated(); }

  SBThreatType threat_type_;
  std::string threat_hash_;
  raw_ptr<SafeBrowsingService> safe_browsing_service_;
};

}  // namespace

// Tests the safe browsing blocking page in a browser.
class V4SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  V4SafeBrowsingServiceTest() {}

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
  // prefixes for the given URL in the client incident store.
  void MarkUrlForResourceUnexpired(const GURL& bad_url) {
    MarkUrlForListIdUnexpired(bad_url, GetChromeUrlClientIncidentId(),
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
        base::BindRepeating(&HandleNeverCompletingRequests));
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleWebSocketRequests));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool ShowingInterstitialPage(Browser* browser) {
    WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    return helper &&
           (helper
                ->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
            nullptr);
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

 protected:
  StrictMock<MockObserver> observer_;

 private:
  std::unique_ptr<TestSafeBrowsingServiceFactory> sb_factory_;
  // Owned by the V4Database.
  raw_ptr<TestV4DatabaseFactory> v4_db_factory_;
  // Owned by the V4GetHashProtocolManager.
  raw_ptr<TestV4GetHashProtocolManagerFactory> v4_get_hash_factory_;
  // Owned by the V4Database.
  raw_ptr<TestV4StoreFactory> store_factory_;

#if defined(ADDRESS_SANITIZER)
  // TODO(lukasza): https://crbug.com/971820: Disallow renderer crashes once the
  // bug is fixed.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes_;
#endif
};

// Ensures that if an image is marked as UwS, the main page doesn't show an
// interstitial.
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, UnwantedImgIgnored) {
  GURL main_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL img_url = embedded_test_server()->GetURL(kMalwareImg);

  // Add the img url as coming from a site serving UwS and then load the parent
  // page.
  MarkUrlForUwsUnexpired(img_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
}

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
  Mock::VerifyAndClear(&observer_);

  // However, when we navigate to the malware page, we should still get
  // the interstitial.
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(malware_url)))
      .Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malware_url));
  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
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
}

// TODO(https://crbug.com/1345215): Flaky on Mac.
// TODO(https://crbug.com/1399454): Flaky on Linux.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_SubResourceHitWithMainFrameReferrer \
  DISABLED_SubResourceHitWithMainFrameReferrer
#else
#define MAYBE_SubResourceHitWithMainFrameReferrer \
  SubResourceHitWithMainFrameReferrer
#endif
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest,
                       MAYBE_SubResourceHitWithMainFrameReferrer) {
  GURL first_url = embedded_test_server()->GetURL(kEmptyPage);
  GURL second_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL bad_url = embedded_test_server()->GetURL(kMalwareImg);

  MarkUrlForMalwareUnexpired(bad_url);

  // Navigate to first, safe page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  Mock::VerifyAndClear(&observer_);

  // Navigate to page which has malware subresource, should show interstitial
  // and have first page in referrer.
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(bad_url)))
      .Times(1);

  NavigateParams params(browser(), second_url, ui::PAGE_TRANSITION_LINK);
  params.referrer.url = first_url;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  EXPECT_EQ(bad_url, hit_report().malicious_url);
  EXPECT_EQ(second_url, hit_report().page_url);
  EXPECT_EQ(first_url, hit_report().referrer_url);
  EXPECT_TRUE(hit_report().is_subresource);
}

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest,
                       SubResourceHitWithMainFrameRendererInitiatedSlowLoad) {
  GURL first_url = embedded_test_server()->GetURL(kEmptyPage);
  GURL second_url = embedded_test_server()->GetURL(kMalwareDelayedLoadsPage);
  GURL third_url = embedded_test_server()->GetURL(kNeverCompletesPath);
  GURL bad_url = embedded_test_server()->GetURL(kMalwareImg);

  MarkUrlForMalwareUnexpired(bad_url);

  // Navigate to first, safe page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  Mock::VerifyAndClear(&observer_);

  // Navigate to malware page. The malware subresources haven't loaded yet, so
  // no interstitial should show yet.
  NavigateParams params(browser(), second_url, ui::PAGE_TRANSITION_LINK);
  params.referrer.url = first_url;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  Mock::VerifyAndClear(&observer_);

  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(bad_url)))
      .Times(1);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::LoadStopObserver load_stop_observer(contents);
  // Run javascript function in the page which starts a timer to load the
  // malware image, and also starts a renderer-initiated top-level navigation to
  // a site that does not respond.  Should show interstitial and have first page
  // in referrer.
  contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"navigateAndLoadMalwareImage()", base::NullCallback());
  load_stop_observer.Wait();

  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  // Report URLs should be for the current page, not the pending load.
  EXPECT_EQ(bad_url, hit_report().malicious_url);
  EXPECT_EQ(second_url, hit_report().page_url);
  EXPECT_EQ(first_url, hit_report().referrer_url);
  EXPECT_TRUE(hit_report().is_subresource);
}

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest,
                       SubResourceHitWithMainFrameBrowserInitiatedSlowLoad) {
  GURL first_url = embedded_test_server()->GetURL(kEmptyPage);
  GURL second_url = embedded_test_server()->GetURL(kMalwareDelayedLoadsPage);
  GURL third_url = embedded_test_server()->GetURL(kNeverCompletesPath);
  GURL bad_url = embedded_test_server()->GetURL(kMalwareImg);

  MarkUrlForMalwareUnexpired(bad_url);

  // Navigate to first, safe page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  Mock::VerifyAndClear(&observer_);

  // Navigate to malware page. The malware subresources haven't loaded yet, so
  // no interstitial should show yet.
  NavigateParams params(browser(), second_url, ui::PAGE_TRANSITION_LINK);
  params.referrer.url = first_url;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
  Mock::VerifyAndClear(&observer_);

  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(bad_url)))
      .Times(1);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  content::LoadStopObserver load_stop_observer(contents);
  // Start a browser initiated top-level navigation to a site that does not
  // respond.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), third_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // While the top-level navigation is pending, run javascript
  // function in the page which loads the malware image.
  rfh->ExecuteJavaScriptForTests(u"loadMalwareImage()", base::NullCallback());

  // Wait for interstitial to show.
  load_stop_observer.Wait();

  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  // Report URLs should be for the current page, not the pending load.
  EXPECT_EQ(bad_url, hit_report().malicious_url);
  EXPECT_EQ(second_url, hit_report().page_url);
  EXPECT_EQ(first_url, hit_report().referrer_url);
  EXPECT_TRUE(hit_report().is_subresource);
}

IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, SubResourceHitOnFreshTab) {
  // Allow popups.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);

  // Add |kMalwareImg| to fake safebrowsing db.
  GURL img_url = embedded_test_server()->GetURL(kMalwareImg);
  MarkUrlForMalwareUnexpired(img_url);

  // Have the current tab open a new tab with window.open().
  WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_rfh = main_contents->GetPrimaryMainFrame();

  content::WebContentsAddedObserver web_contents_added_observer;
  main_rfh->ExecuteJavaScriptForTests(u"w=window.open();",
                                      base::NullCallback());
  WebContents* new_tab_contents = web_contents_added_observer.GetWebContents();
  content::RenderFrameHost* new_tab_rfh =
      new_tab_contents->GetPrimaryMainFrame();
  // A fresh WebContents should be on the initial NavigationEntry.
  EXPECT_TRUE(new_tab_contents->GetController()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());
  EXPECT_EQ(nullptr, new_tab_contents->GetController().GetPendingEntry());

  // Run javascript in the blank new tab to load the malware image.
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(img_url)))
      .Times(1);
  // Wait for 2 navigations to finish: the synchronous about:blank commit
  // triggered by the window.open() above, and the interstitial page
  // navigation triggered by Safe Browsing code for the image load below.
  content::TestNavigationObserver observer(new_tab_contents, 2);
  new_tab_rfh->ExecuteJavaScriptForTests(
      u"var img=new Image();"
      u"img.src=\"" +
          base::ASCIIToUTF16(img_url.spec()) +
          u"\";"
          u"document.body.appendChild(img);",
      base::NullCallback());
  // Wait for interstitial to show.
  observer.WaitForNavigationFinished();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  EXPECT_EQ(img_url, hit_report().malicious_url);
  EXPECT_TRUE(hit_report().is_subresource);
  // Page report URLs should be about:blank, as the last committed navigation is
  // the synchronous about:blank commit.
  EXPECT_EQ(GURL(url::kAboutBlankURL), hit_report().page_url);
  EXPECT_EQ(GURL(url::kAboutBlankURL), hit_report().referrer_url);

  // Proceed through it.
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          new_tab_contents);
  ASSERT_TRUE(helper);
  security_interstitials::SecurityInterstitialPage* interstitial =
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  ASSERT_TRUE(interstitial);
  // TODO(carlosil): 1 is CMD_PROCEED, this should be changed to the enum
  // values once CommandReceived is changed to accept integers.
  content::TestNavigationObserver proceed_observer(new_tab_contents);
  interstitial->CommandReceived("1");
  proceed_observer.WaitForNavigationFinished();
  EXPECT_FALSE(ShowingInterstitialPage());
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
  const absl::optional<base::OneShotTimer>& timer =
      reloader->next_reload_timer_for_testing();
  EXPECT_EQ(absl::nullopt, timer);
}

// Parameterised fixture to permit running the same test for Window and Worker
// scopes.
class V4SafeBrowsingServiceJsRequestTest
    : public ::testing::WithParamInterface<JsRequestTestParam>,
      public V4SafeBrowsingServiceTest {};

using V4SafeBrowsingServiceJsRequestInterstitialTest =
    V4SafeBrowsingServiceJsRequestTest;

IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceJsRequestInterstitialTest,
                       MalwareBlocked) {
  GURL base_url = embedded_test_server()->GetURL(kMalwareJsRequestPage);
  JsRequestTestParam param = GetParam();
  GURL js_request_url = ConstructJsRequestURL(base_url, param.request_type);
  GURL page_url = AddJsRequestParam(base_url, param);

  MarkUrlForMalwareUnexpired(js_request_url);

  EXPECT_CALL(observer_,
              OnSafeBrowsingHit(IsUnsafeResourceFor(js_request_url)));

  content::TestNavigationObserver error_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::ERR_BLOCKED_BY_CLIENT);
  error_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kNavigationFinished);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  error_observer.WaitForNavigationFinished();

  EXPECT_TRUE(ShowingInterstitialPage());
  EXPECT_TRUE(got_hit_report());
  EXPECT_EQ(js_request_url, hit_report().malicious_url);
  EXPECT_EQ(page_url, hit_report().page_url);
  EXPECT_TRUE(hit_report().is_subresource);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    V4SafeBrowsingServiceJsRequestInterstitialTest,
    ::testing::Values(
        JsRequestTestParam(ContextType::kWindow, JsRequestType::kWebSocket),
        JsRequestTestParam(ContextType::kWorker, JsRequestType::kWebSocket),
        JsRequestTestParam(ContextType::kWindow, JsRequestType::kFetch),
        JsRequestTestParam(ContextType::kWorker, JsRequestType::kFetch)));

using V4SafeBrowsingServiceJsRequestNoInterstitialTest =
    V4SafeBrowsingServiceJsRequestTest;

IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceJsRequestNoInterstitialTest,
                       MalwareBlocked) {
  GURL base_url = embedded_test_server()->GetURL(kMalwareJsRequestPage);
  JsRequestTestParam param = GetParam();
  MarkUrlForMalwareUnexpired(
      ConstructJsRequestURL(base_url, param.request_type));

  // Load the parent page after marking the JS request as malware.
  auto new_title = JsRequestTestNavigateAndWaitForTitle(
      browser(), AddJsRequestParam(base_url, param));

  EXPECT_EQ("ERROR", new_title);
  EXPECT_FALSE(ShowingInterstitialPage());

  // got_hit_report() is only set when an interstitial is shown.
  EXPECT_FALSE(got_hit_report());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    V4SafeBrowsingServiceJsRequestNoInterstitialTest,
    ::testing::Values(JsRequestTestParam(ContextType::kSharedWorker,
                                         JsRequestType::kWebSocket),
                      JsRequestTestParam(ContextType::kServiceWorker,
                                         JsRequestType::kWebSocket),
                      JsRequestTestParam(ContextType::kSharedWorker,
                                         JsRequestType::kFetch),
                      JsRequestTestParam(ContextType::kServiceWorker,
                                         JsRequestType::kFetch)));

using V4SafeBrowsingServiceJsRequestSafeTest =
    V4SafeBrowsingServiceJsRequestTest;

IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceJsRequestSafeTest,
                       RequestNotBlocked) {
  GURL base_url = embedded_test_server()->GetURL(kMalwareJsRequestPage);

  // Load the parent page without marking the JS request as malware.
  auto new_title = JsRequestTestNavigateAndWaitForTitle(
      browser(), AddJsRequestParam(base_url, GetParam()));

  EXPECT_EQ("NOT BLOCKED", new_title);
  EXPECT_FALSE(ShowingInterstitialPage());
  EXPECT_FALSE(got_hit_report());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    V4SafeBrowsingServiceJsRequestSafeTest,
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// This test is only enabled when GOOGLE_CHROME_BRANDING is true because the
// store that this test uses is only populated on GOOGLE_CHROME_BRANDING builds.
IN_PROC_BROWSER_TEST_F(V4SafeBrowsingServiceTest, CheckResourceUrl) {
  GURL blocklist_url = embedded_test_server()->GetURL(kBlocklistResource);
  GURL malware_url = embedded_test_server()->GetURL(kMaliciousResource);
  std::string blocklist_url_hash, malware_url_hash;

  scoped_refptr<TestSBClient> client(new TestSBClient);
  {
    MarkUrlForResourceUnexpired(blocklist_url);
    blocklist_url_hash = V4ProtocolManagerUtil::GetFullHash(blocklist_url);

    client->CheckResourceUrl(blocklist_url);
    EXPECT_EQ(SB_THREAT_TYPE_BLOCKLISTED_RESOURCE, client->GetThreatType());
    EXPECT_EQ(blocklist_url_hash, client->GetThreatHash());
  }
  {
    MarkUrlForMalwareUnexpired(malware_url);
    MarkUrlForResourceUnexpired(malware_url);
    malware_url_hash = V4ProtocolManagerUtil::GetFullHash(malware_url);

    // Since we're checking a resource url, we should receive result that it's
    // a blocklisted resource, not a malware.
    client = new TestSBClient;
    client->CheckResourceUrl(malware_url);
    EXPECT_EQ(SB_THREAT_TYPE_BLOCKLISTED_RESOURCE, client->GetThreatType());
    EXPECT_EQ(malware_url_hash, client->GetThreatHash());
  }

  client->CheckResourceUrl(embedded_test_server()->GetURL(kEmptyPage));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

///////////////////////////////////////////////////////////////////////////////
// END: These tests use SafeBrowsingService::Client to directly interact with
// SafeBrowsingService.
///////////////////////////////////////////////////////////////////////////////

// TODO(vakh): Add test for UnwantedMainFrame.

class V4SafeBrowsingServiceMetadataTest
    : public V4SafeBrowsingServiceTest,
      public ::testing::WithParamInterface<ThreatPatternType> {
 public:
  V4SafeBrowsingServiceMetadataTest() {}

  V4SafeBrowsingServiceMetadataTest(const V4SafeBrowsingServiceMetadataTest&) =
      delete;
  V4SafeBrowsingServiceMetadataTest& operator=(
      const V4SafeBrowsingServiceMetadataTest&) = delete;
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
}

// Irrespective of the threat_type classification, if the iframe URL is marked
// as Malware, an interstitial should be shown.
IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceMetadataTest, MalwareIFrame) {
  GURL main_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL iframe_url = embedded_test_server()->GetURL(kMalwareIFrame);

  // Add the iframe url as malware and then load the parent page.
  MarkUrlForMalwareUnexpired(iframe_url, GetParam());

  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(iframe_url)))
      .Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  // All types should show the interstitial.
  EXPECT_TRUE(ShowingInterstitialPage());

  EXPECT_TRUE(got_hit_report());
  EXPECT_EQ(iframe_url, hit_report().malicious_url);
  EXPECT_EQ(main_url, hit_report().page_url);
  EXPECT_EQ(GURL(), hit_report().referrer_url);
  EXPECT_TRUE(hit_report().is_subresource);
}

// Depending on the threat_type classification, if an embedded resource is
// marked as Malware, an interstitial may be shown.
// TODO(crbug.com/1320123): Re-enable this test
IN_PROC_BROWSER_TEST_P(V4SafeBrowsingServiceMetadataTest, DISABLED_MalwareImg) {
  GURL main_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL img_url = embedded_test_server()->GetURL(kMalwareImg);

  // Add the img url as malware and then load the parent page.
  MarkUrlForMalwareUnexpired(img_url, GetParam());

  switch (GetParam()) {
    case ThreatPatternType::NONE:  // Falls through.
    case ThreatPatternType::MALWARE_DISTRIBUTION:
      EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(img_url)))
          .Times(1);
      break;
    case ThreatPatternType::MALWARE_LANDING:
      // No interstitial shown, so no notifications expected.
      break;
    default:
      break;
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Subresource which is tagged as a landing page should not show an
  // interstitial, the other types should.
  switch (GetParam()) {
    case ThreatPatternType::NONE:  // Falls through.
    case ThreatPatternType::MALWARE_DISTRIBUTION:
      EXPECT_TRUE(ShowingInterstitialPage());
      EXPECT_TRUE(got_hit_report());
      EXPECT_EQ(img_url, hit_report().malicious_url);
      EXPECT_EQ(main_url, hit_report().page_url);
      EXPECT_EQ(GURL(), hit_report().referrer_url);
      EXPECT_TRUE(hit_report().is_subresource);
      break;
    case ThreatPatternType::MALWARE_LANDING:
      EXPECT_FALSE(ShowingInterstitialPage());
      EXPECT_FALSE(got_hit_report());
      break;
    default:
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    MaybeSetMetadata,
    V4SafeBrowsingServiceMetadataTest,
    testing::Values(ThreatPatternType::NONE,
                    ThreatPatternType::MALWARE_LANDING,
                    ThreatPatternType::MALWARE_DISTRIBUTION));

}  // namespace safe_browsing
