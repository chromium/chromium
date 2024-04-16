// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <map>
#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/predictors_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_features.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

constexpr char kTrialName[] = "t1";

class VariationHeaderSetter : public ChromeBrowserMainExtraParts {
 public:
  VariationHeaderSetter() = default;

  VariationHeaderSetter(const VariationHeaderSetter&) = delete;
  VariationHeaderSetter& operator=(const VariationHeaderSetter&) = delete;

  ~VariationHeaderSetter() override = default;

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override {
    // Set up some fake variations.
    auto* variations_provider =
        variations::VariationsIdsProvider::GetInstance();
    variations_provider->ForceVariationIds({"12", "456", "t789"}, "");
  }
};

class VariationsHttpHeadersBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  VariationsHttpHeadersBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    if (IsPlzDedicatedWorkerEnabled()) {
      feature_list_.InitAndEnableFeature(blink::features::kPlzDedicatedWorker);
    } else {
      feature_list_.InitAndDisableFeature(blink::features::kPlzDedicatedWorker);
    }
  }

  VariationsHttpHeadersBrowserTest(const VariationsHttpHeadersBrowserTest&) =
      delete;
  VariationsHttpHeadersBrowserTest& operator=(
      const VariationsHttpHeadersBrowserTest&) = delete;

  ~VariationsHttpHeadersBrowserTest() override = default;

  static bool IsPlzDedicatedWorkerEnabled() { return GetParam(); }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<VariationHeaderSetter>());
  }

  void SetUp() override {
    ASSERT_TRUE(server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);

    host_resolver()->AddRule("*", "127.0.0.1");

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    server()->ServeFilesFromDirectory(test_data_dir);

    server()->RegisterRequestHandler(
        base::BindRepeating(&VariationsHttpHeadersBrowserTest::RequestHandler,
                            base::Unretained(this)));

    server()->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  const net::EmbeddedTestServer* server() const { return &https_server_; }
  net::EmbeddedTestServer* server() { return &https_server_; }

  GURL GetGoogleUrlWithPath(const std::string& path) const {
    return server()->GetURL("www.google.com", path);
  }

  GURL GetGoogleUrl() const { return GetGoogleUrlWithPath("/landing.html"); }

  GURL GetGoogleIframeUrl() const {
    return GetGoogleUrlWithPath("/iframe.html");
  }

  GURL GetGoogleSubresourceFetchingWorkerUrl() const {
    return GetGoogleUrlWithPath("/subresource_fetch_worker.js");
  }

  GURL GetGoogleRedirectUrl1() const {
    return GetGoogleUrlWithPath("/redirect");
  }

  GURL GetGoogleRedirectUrl2() const {
    return GetGoogleUrlWithPath("/redirect2");
  }

  GURL GetGoogleSubresourceUrl() const {
    return GetGoogleUrlWithPath("/logo.png");
  }

  GURL GetExampleUrlWithPath(const std::string& path) const {
    return server()->GetURL("www.example.com", path);
  }

  GURL GetExampleUrl() const { return GetExampleUrlWithPath("/landing.html"); }

  void WaitForRequest(const GURL& url) {
    auto it = received_headers_.find(url);
    if (it != received_headers_.end())
      return;
    base::RunLoop loop;
    done_callbacks_.emplace(url, loop.QuitClosure());
    loop.Run();
  }

  // Returns whether a given |header| has been received for a |url|. If
  // |url| has not been observed, fails an EXPECT and returns false.
  bool HasReceivedHeader(const GURL& url, const std::string& header) const {
    auto it = received_headers_.find(url);
    EXPECT_TRUE(it != received_headers_.end());
    if (it == received_headers_.end())
      return false;
    return it->second.find(header) != it->second.end();
  }

  // Returns the |header| received by |url| or nullopt if it hasn't been
  // received. Fails an EXPECT if |url| hasn't been observed.
  std::optional<std::string> GetReceivedHeader(
      const GURL& url,
      const std::string& header) const {
    auto it = received_headers_.find(url);
    EXPECT_TRUE(it != received_headers_.end());
    if (it == received_headers_.end())
      return std::nullopt;
    auto it2 = it->second.find(header);
    if (it2 == it->second.end())
      return std::nullopt;
    return it2->second;
  }

  void ClearReceivedHeaders() { received_headers_.clear(); }

  bool LoadIframe(const content::ToRenderFrameHost& execution_target,
                  const GURL& url) {
    if (!url.is_valid())
      return false;
    return EvalJs(execution_target, content::JsReplace(R"(
          (async () => {
            return new Promise(resolve => {
              const iframe = document.createElement('iframe');
              iframe.addEventListener('load', () => { resolve(true); });
              iframe.addEventListener('error', () => { resolve(false); });
              iframe.src = $1;
              document.body.appendChild(iframe);
            });
          })();
        )",
                                                       url))
        .ExtractBool();
  }

  bool FetchResource(const content::ToRenderFrameHost& execution_target,
                     const GURL& url) {
    if (!url.is_valid()) {
      return false;
    }
    return EvalJs(execution_target, content::JsReplace(R"(
          (async () => {
            try {
              await fetch($1);
              return true;
            } catch {
              return false;
            }
          })();
        )",
                                                       url))
        .ExtractBool();
  }
  bool RunSubresourceFetchingWorker(
      const content::ToRenderFrameHost& execution_target,
      const GURL& worker_url,
      const GURL& subresource_url) {
    if (!worker_url.is_valid() || !subresource_url.is_valid()) {
      return false;
    }
    return EvalJs(execution_target,
                  content::JsReplace(R"(
          (async () => {
            return await new Promise(resolve => {
              const worker = new Worker($1);
              worker.addEventListener('message', (e) => { resolve(e.data); });
              worker.postMessage($2);
            });
          })();
        )",
                                     worker_url, subresource_url))
        .ExtractBool();
  }

  content::WebContents* GetWebContents() { return GetWebContents(browser()); }

  content::WebContents* GetWebContents(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  void GoogleWebVisibilityTopFrameTest(bool top_frame_is_first_party);

  // Registers a service worker for google.com root scope.
  void RegisterServiceWorker(const std::string& worker_path) {
    GURL url =
        GetGoogleUrlWithPath("/service_worker/create_service_worker.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ("DONE", EvalJs(GetWebContents(),
                             base::StringPrintf("register('%s', '/');",
                                                worker_path.c_str())));
  }

  // Registers the given service worker for google.com then tests navigation and
  // subresource requests through the worker have X-Client-Data when
  // appropriate.
  void ServiceWorkerTest(const std::string& worker_path) {
    RegisterServiceWorker(worker_path);

    // Navigate to a Google URL.
    GURL page_url =
        GetGoogleUrlWithPath("/service_worker/fetch_from_page.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    EXPECT_TRUE(HasReceivedHeader(page_url, "X-Client-Data"));
    // Check that there is a controller to check that the test is really testing
    // service worker.
    EXPECT_EQ(true,
              EvalJs(GetWebContents(), "!!navigator.serviceWorker.controller"));

    // Verify subresource requests from the page also have X-Client-Data.
    EXPECT_EQ("hello", EvalJs(GetWebContents(),
                              base::StrCat({"fetch_from_page('",
                                            GetGoogleUrl().spec(), "');"})));
    EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(), "X-Client-Data"));

    // But not if they are to non-Google domains.
    EXPECT_EQ("hello", EvalJs(GetWebContents(),
                              base::StrCat({"fetch_from_page('",
                                            GetExampleUrl().spec(), "');"})));
    EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));

    // Navigate to a Google URL which causes redirects.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetGoogleRedirectUrl1()));

    // Verify redirect requests from google domains.
    // Redirect to google domains.
    EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
    EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));

    // Redirect to non-google domains.
    EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
    EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
  }

  // Creates a worker and tests that the main script and import scripts have
  // X-Client-Data when appropriate. |page| is the page that creates the
  // specified |worker|, which should be an "import_*_worker.js" script that is
  // expected to import "empty.js" (as a relative path) and also accept an
  // "import=" parameter specifying another script to import. This allows
  // testing that the empty.js import request for google.com has the header, and
  // an import request to example.com does not have the header.
  void WorkerScriptTest(const std::string& page, const std::string& worker) {
    // Build a worker URL for a google.com worker that imports
    // an example.com script.
    GURL absolute_import = GetExampleUrlWithPath("/workers/empty.js");
    const std::string worker_path = base::StrCat(
        {worker, "?import=",
         base::EscapeQueryParamValue(absolute_import.spec(), false)});
    GURL worker_url = GetGoogleUrlWithPath(worker_path);

    // Build the page URL that tells the page to create the worker.
    const std::string page_path =
        base::StrCat({page, "?worker_url=",
                      base::EscapeQueryParamValue(worker_url.spec(), false)});
    GURL page_url = GetGoogleUrlWithPath(page_path);

    // Navigate and test.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
    EXPECT_EQ("DONE", EvalJs(GetWebContents(), "waitForMessage();"));

    // The header should be on the main script request.
    EXPECT_TRUE(HasReceivedHeader(worker_url, "X-Client-Data"));

    // And on import script requests to Google.
    EXPECT_TRUE(HasReceivedHeader(GetGoogleUrlWithPath("/workers/empty.js"),
                                  "X-Client-Data"));

    // But not on requests not to Google.
    EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));
  }

 private:
  // Custom request handler that record request headers and simulates a redirect
  // from google.com to example.com.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer https_server_;

  // Stores the observed HTTP Request headers.
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_;

  // For waiting for requests.
  std::map<GURL, base::OnceClosure> done_callbacks_;

  // To enable/disable the PlzDedicatedWorker feature during the test.
  base::test::ScopedFeatureList feature_list_;
};

std::unique_ptr<net::test_server::HttpResponse>
VariationsHttpHeadersBrowserTest::RequestHandler(
    const net::test_server::HttpRequest& request) {
  // Retrieve the host name (without port) from the request headers.
  std::string host;
  if (request.headers.find("Host") != request.headers.end())
    host = request.headers.find("Host")->second;
  if (host.find(':') != std::string::npos)
    host = host.substr(0, host.find(':'));

  // Recover the original URL of the request by replacing the host name in
  // request.GetURL() (which is 127.0.0.1) with the host name from the request
  // headers.
  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  GURL original_url = request.GetURL().ReplaceComponents(replacements);

  // Memorize the request headers for this URL for later verification.
  received_headers_[original_url] = request.headers;
  auto iter = done_callbacks_.find(original_url);
  if (iter != done_callbacks_.end()) {
    std::move(iter->second).Run();
  }

  // Set up a test server that redirects according to the
  // following redirect chain:
  // https://www.google.com:<port>/redirect
  // --> https://www.google.com:<port>/redirect2
  // --> https://www.example.com:<port>/
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  if (request.relative_url == GetGoogleRedirectUrl1().path()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetGoogleRedirectUrl2().spec());
  } else if (request.relative_url == GetGoogleRedirectUrl2().path()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetExampleUrl().spec());
  } else if (request.relative_url == GetExampleUrl().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/html");
  } else if (request.relative_url == GetGoogleIframeUrl().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/html");
  } else if (request.relative_url == GetGoogleSubresourceUrl().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("");
    http_response->set_content_type("image/png");
  } else if (request.relative_url ==
             GetGoogleSubresourceFetchingWorkerUrl().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(R"(
      self.addEventListener('message', async (e) => {
        try {
          await fetch(e.data);
          self.postMessage(true);
        } catch {
          self.postMessage(false);
        }
      });
    )");
    http_response->set_content_type("text/html");
  } else {
    return nullptr;
  }
  return http_response;
}

// Associates |id| with GOOGLE_WEB_PROPERTIES_SIGNED_IN and creates a field
// trial for it.
void CreateGoogleSignedInFieldTrial(variations::VariationID id) {
  scoped_refptr<base::FieldTrial> trial_1(variations::CreateTrialAndAssociateId(
      "t1", "g1", variations::GOOGLE_WEB_PROPERTIES_SIGNED_IN, id));

  auto* provider = variations::VariationsIdsProvider::GetInstance();
  variations::mojom::VariationsHeadersPtr signed_in_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/true);
  variations::mojom::VariationsHeadersPtr signed_out_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/false);

  EXPECT_NE(signed_in_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::ANY),
            signed_out_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::ANY));
  EXPECT_NE(signed_in_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::FIRST_PARTY),
            signed_out_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::FIRST_PARTY));
}

// Creates FieldTrials associated with the FIRST_PARTY IDCollectionKeys and
// their corresponding ANY_CONTEXT keys.
void CreateFieldTrialsWithDifferentVisibilities() {
  scoped_refptr<base::FieldTrial> trial_1(variations::CreateTrialAndAssociateId(
      "t1", "g1", variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 11));
  scoped_refptr<base::FieldTrial> trial_2(variations::CreateTrialAndAssociateId(
      "t2", "g2", variations::GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 22));
  scoped_refptr<base::FieldTrial> trial_3(variations::CreateTrialAndAssociateId(
      "t3", "g3", variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 33));
  scoped_refptr<base::FieldTrial> trial_4(variations::CreateTrialAndAssociateId(
      "t4", "g4", variations::GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 44));

  auto* provider = variations::VariationsIdsProvider::GetInstance();
  variations::mojom::VariationsHeadersPtr signed_in_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/true);
  variations::mojom::VariationsHeadersPtr signed_out_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/false);

  EXPECT_NE(signed_in_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::ANY),
            signed_in_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::FIRST_PARTY));
  EXPECT_NE(signed_out_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::ANY),
            signed_out_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::FIRST_PARTY));
}

// Sets the limited entropy randomization source to a custom value so that we
// can have an expectation about a specific group being chosen.
void SetUpLimitedEntropyRandomizationSource() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(
      metrics::prefs::kMetricsLimitedEntropyRandomizationSource,
      "00000000000000000000000000000001");
}

// Creates a trial named "t1" with 100 groups. If
// `with_google_web_experiment_ids` is true, each group will be associated with
// a variation ID.
// TODO(crbug.com/40729905): Refactor this so that creating the field trial
// either uses a different API or tighten the current API to set up a field
// trial that can only be made with the low entropy provider.
void CreateFieldTrial(const base::FieldTrial::EntropyProvider& entropy_provider,
                      bool with_google_web_experiment_ids) {
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, "default",
                                                 entropy_provider);
  for (int i = 1; i < 101; ++i) {
    const std::string group_name = base::StringPrintf("%d", i);
    if (with_google_web_experiment_ids) {
      variations::AssociateGoogleVariationID(
          variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, trial->trial_name(),
          group_name, i);
    }
    trial->AppendGroup(group_name, 1);
  }
  // Activate the trial. This corresponds to ACTIVATE_ON_STARTUP for server-side
  // studies.
  trial->Activate();
}

}  // namespace

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to network requests to Google but stripped on redirects.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromResourceRequest) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleRedirectUrl1()));

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

// Verify in an integration that that the variations header (X-Client-Data) is
// correctly attached and stripped from network requests.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromSubresourceRequest) {
  GURL url = server()->GetURL("/simple_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      FetchResource(GetWebContents(browser()), GetGoogleRedirectUrl1()));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest, Incognito) {
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, GetGoogleUrl()));

  EXPECT_FALSE(HasReceivedHeader(GetGoogleUrl(), "X-Client-Data"));

  EXPECT_TRUE(
      FetchResource(GetWebContents(incognito), GetGoogleSubresourceUrl()));
  EXPECT_FALSE(HasReceivedHeader(GetGoogleSubresourceUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest, UserSignedIn) {
  // Ensure GetClientDataHeader() returns different values when signed in vs
  // not signed in.
  variations::VariationID signed_in_id = 8;
  CreateGoogleSignedInFieldTrial(signed_in_id);

  // Sign the user in.
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "main_email@gmail.com", signin::ConsentLevel::kSync);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleUrl()));

  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(), "X-Client-Data");
  ASSERT_TRUE(header);

  // Verify that the received header contains the ID.
  std::set<variations::VariationID> ids;
  std::set<variations::VariationID> trigger_ids;
  ASSERT_TRUE(
      variations::ExtractVariationIds(header.value(), &ids, &trigger_ids));
  EXPECT_TRUE(base::Contains(ids, signed_in_id));

  // Verify that both headers returned by GetClientDataHeaders() contain the ID.
  variations::mojom::VariationsHeadersPtr headers =
      variations::VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          /*is_signed_in=*/true);

  const std::string variations_header_first_party = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);
  const std::string variations_header_any_context =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

  std::set<variations::VariationID> ids_first_party;
  std::set<variations::VariationID> trigger_ids_first_party;
  ASSERT_TRUE(variations::ExtractVariationIds(variations_header_first_party,
                                              &ids_first_party,
                                              &trigger_ids_first_party));
  EXPECT_TRUE(base::Contains(ids_first_party, signed_in_id));

  std::set<variations::VariationID> ids_any_context;
  std::set<variations::VariationID> trigger_ids_any_context;
  ASSERT_TRUE(variations::ExtractVariationIds(variations_header_any_context,
                                              &ids_any_context,
                                              &trigger_ids_any_context));

  EXPECT_TRUE(base::Contains(ids_any_context, signed_in_id));
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest, UserNotSignedIn) {
  // Ensure GetClientDataHeader() returns different values when signed in vs
  // not signed in.
  variations::VariationID signed_in_id = 8;
  CreateGoogleSignedInFieldTrial(signed_in_id);

  // By default the user is not signed in.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleUrl()));

  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(), "X-Client-Data");
  ASSERT_TRUE(header);

  // Verify that the received header does not contain the ID.
  std::set<variations::VariationID> ids;
  std::set<variations::VariationID> trigger_ids;
  ASSERT_TRUE(
      variations::ExtractVariationIds(header.value(), &ids, &trigger_ids));
  EXPECT_FALSE(base::Contains(ids, signed_in_id));

  // Verify that both headers returned by GetClientDataHeaders() do not contain
  // the ID.
  variations::mojom::VariationsHeadersPtr headers =
      variations::VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          /*is_signed_in=*/false);

  const std::string variations_header_first_party = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);
  const std::string variations_header_any_context =
      headers->headers_map.at(variations::mojom::GoogleWebVisibility::ANY);

  std::set<variations::VariationID> ids_first_party;
  std::set<variations::VariationID> trigger_ids_first_party;
  ASSERT_TRUE(variations::ExtractVariationIds(variations_header_first_party,
                                              &ids_first_party,
                                              &trigger_ids_first_party));
  EXPECT_FALSE(base::Contains(ids_first_party, signed_in_id));

  std::set<variations::VariationID> ids_any_context;
  std::set<variations::VariationID> trigger_ids_any_context;
  ASSERT_TRUE(variations::ExtractVariationIds(variations_header_any_context,
                                              &ids_any_context,
                                              &trigger_ids_any_context));

  EXPECT_FALSE(base::Contains(ids_any_context, signed_in_id));
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       PRE_CheckLowEntropySourceValue) {
  // We use the PRE_ prefix mechanism to ensure that this test always runs
  // before CheckLowEntropyValue(). None of the subclasses in the
  // InProcessBrowserTest class allow us to set this pref early enough to be
  // read by the variations code, which runs very early during the browser
  // startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(metrics::prefs::kMetricsLowEntropySource, 5);
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       CheckLowEntropySourceValue) {
  auto entropy_providers = g_browser_process->GetMetricsServicesManager()
                               ->CreateEntropyProvidersForTesting();
  // `with_google_web_experiment_ids` is true so that the low entropy provider
  // is used for randomization.
  CreateFieldTrial(entropy_providers->low_entropy(),
                   /*with_google_web_experiment_ids=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleUrl()));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(), "X-Client-Data");
  ASSERT_TRUE(header);

  std::set<variations::VariationID> variation_ids;
  std::set<variations::VariationID> trigger_ids;
  ASSERT_TRUE(variations::ExtractVariationIds(header.value(), &variation_ids,
                                              &trigger_ids));

  // 3320983 is the offset value of kLowEntropySourceVariationIdRangeMin + 5.
  EXPECT_TRUE(base::Contains(variation_ids, 3320983));

  // Check that the reported group in the header is consistent with the low
  // entropy source. 33 is the group that is derived from the low entropy source
  // value of 5.
  EXPECT_TRUE(base::Contains(variation_ids, 33));
}

// The PRE_ prefix ensures this runs before
// LimitedEntropyRandomization_ExperimentLogging.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       PRE_LimitedEntropyRandomization_ExperimentLogging) {
  SetUpLimitedEntropyRandomizationSource();
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       LimitedEntropyRandomization_ExperimentLogging) {
  // CreateEntropyProvidersForTesting() ensures a limited entropy provider is
  // created.
  auto entropy_providers = g_browser_process->GetMetricsServicesManager()
                               ->CreateEntropyProvidersForTesting();
  ASSERT_TRUE(entropy_providers->has_limited_entropy());
  // Create a field trial that will be randomized with the limited entropy
  // provider.
  CreateFieldTrial(entropy_providers->limited_entropy(),
                   /*with_google_web_experiment_ids=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleUrl()));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(), "X-Client-Data");
  ASSERT_TRUE(header);

  std::set<variations::VariationID> variation_ids;
  std::set<variations::VariationID> trigger_ids;
  ASSERT_TRUE(variations::ExtractVariationIds(header.value(), &variation_ids,
                                              &trigger_ids));

  // 56 is the group that is derived from the setup in
  // `PRE_CheckGoogleWebExperimentIdUnderLimitedEntropyRandomization`.
  EXPECT_EQ("56", base::FieldTrialList::FindFullName(kTrialName));
  // Check that the reported group in the header is consistent with the
  // limited entropy randomization source.
  EXPECT_TRUE(base::Contains(variation_ids, 56));
}

// The PRE_ prefix ensures this runs before
// LimitedEntropyRandomization_ExperimentLoggingWithoutGoogleWebExperimentationId.
IN_PROC_BROWSER_TEST_P(
    VariationsHttpHeadersBrowserTest,
    PRE_LimitedEntropyRandomization_ExperimentLoggingWithoutGoogleWebExperimentationId) {
  SetUpLimitedEntropyRandomizationSource();
}

IN_PROC_BROWSER_TEST_P(
    VariationsHttpHeadersBrowserTest,
    LimitedEntropyRandomization_ExperimentLoggingWithoutGoogleWebExperimentationId) {
  // CreateEntropyProvidersForTesting() ensures a limited entropy provider is
  // created.
  auto entropy_providers = g_browser_process->GetMetricsServicesManager()
                               ->CreateEntropyProvidersForTesting();
  ASSERT_TRUE(entropy_providers->has_limited_entropy());
  CreateFieldTrial(entropy_providers->limited_entropy(),
                   /*with_google_web_experiment_ids=*/false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleUrl()));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(), "X-Client-Data");
  ASSERT_TRUE(header);

  std::set<variations::VariationID> variation_ids;
  std::set<variations::VariationID> trigger_ids;
  ASSERT_TRUE(variations::ExtractVariationIds(header.value(), &variation_ids,
                                              &trigger_ids));

  // 56 is the group that is derived from the setup in
  // `PRE_CheckGoogleWebExperimentIdUnderLimitedEntropyRandomization`.
  EXPECT_EQ("56", base::FieldTrialList::FindFullName(kTrialName));
  // The experiment does not have a google_web_experiment_id and thus should
  // NOT appear in the header.
  EXPECT_FALSE(base::Contains(variation_ids, 56));
}

void VariationsHttpHeadersBrowserTest::GoogleWebVisibilityTopFrameTest(
    bool top_frame_is_first_party) {
  CreateFieldTrialsWithDifferentVisibilities();
  variations::mojom::VariationsHeadersPtr signed_out_headers =
      variations::VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          /*is_signed_in=*/false);

  const std::string expected_header_value =
      top_frame_is_first_party
          ? signed_out_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::FIRST_PARTY)
          : signed_out_headers->headers_map.at(
                variations::mojom::GoogleWebVisibility::ANY);

  // Load a top frame.
  const GURL top_frame_url =
      top_frame_is_first_party ? GetGoogleUrl() : GetExampleUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), top_frame_url));
  if (top_frame_is_first_party) {
    EXPECT_EQ(GetReceivedHeader(top_frame_url, "X-Client-Data"),
              expected_header_value);
  } else {
    EXPECT_FALSE(GetReceivedHeader(top_frame_url, "X-Client-Data"));
  }

  // Load Google iframe.
  EXPECT_TRUE(LoadIframe(GetWebContents(browser()), GetGoogleIframeUrl()));
  EXPECT_EQ(GetReceivedHeader(GetGoogleIframeUrl(), "X-Client-Data"),
            expected_header_value);

  // Fetch Google subresource.
  EXPECT_TRUE(FetchResource(ChildFrameAt(GetWebContents(browser()), 0),
                            GetGoogleSubresourceUrl()));
  EXPECT_EQ(GetReceivedHeader(GetGoogleSubresourceUrl(), "X-Client-Data"),
            expected_header_value);

  // Prepare for loading Google subresource from a dedicated worker. The same
  // URL subresource was loaded above. So need to clear `received_headers_`.
  ClearReceivedHeaders();

  // Start Google worker and fetch Google subresource from the worker.
  EXPECT_TRUE(RunSubresourceFetchingWorker(
      ChildFrameAt(GetWebContents(browser()), 0),
      GetGoogleSubresourceFetchingWorkerUrl(), GetGoogleSubresourceUrl()));
  EXPECT_EQ(GetReceivedHeader(GetGoogleSubresourceFetchingWorkerUrl(),
                              "X-Client-Data"),
            expected_header_value);
  EXPECT_EQ(GetReceivedHeader(GetGoogleSubresourceUrl(), "X-Client-Data"),
            expected_header_value);
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       TestGoogleWebVisibilityInFirstPartyContexts) {
  GoogleWebVisibilityTopFrameTest(/*top_frame_is_first_party=*/true);
}

IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       TestGoogleWebVisibilityInThirdPartyContexts) {
  GoogleWebVisibilityTopFrameTest(/*top_frame_is_first_party=*/false);
}

IN_PROC_BROWSER_TEST_P(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithProfileNetworkContext) {
  GURL url = GetGoogleRedirectUrl1();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;

  std::unique_ptr<network::SimpleURLLoader> loader =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request), variations::InIncognito::kNo,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  content::StoragePartition* partition =
      browser()->profile()->GetDefaultStoragePartition();
  network::SharedURLLoaderFactory* loader_factory =
      partition->GetURLLoaderFactoryForBrowserProcess().get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallbackDeprecated());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_EQ(net::OK, loader->NetError());
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_P(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithGlobalSystemNetworkContext) {
  GURL url = GetGoogleRedirectUrl1();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;

  std::unique_ptr<network::SimpleURLLoader> loader =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request), variations::InIncognito::kNo,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  network::SharedURLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory()
          .get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallbackDeprecated());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_EQ(net::OK, loader->NetError());
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to service worker navigation preload requests. Regression test
// for https://crbug.com/873061.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerNavigationPreload) {
  // Register a service worker that uses navigation preload.
  RegisterServiceWorker("/service_worker/navigation_preload_worker.js");

  // Verify "X-Client-Data" is present on the navigation to Google.
  // Also test that "Service-Worker-Navigation-Preload" is present to verify
  // we are really testing the navigation preload request.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetGoogleUrl()));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(), "X-Client-Data"));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleUrl(), "Service-Worker-Navigation-Preload"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests after the service worker falls back to network.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerNetworkFallback) {
  ServiceWorkerTest("/service_worker/network_fallback_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// not exposed in the service worker fetch event.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerDoesNotSeeHeader) {
  ServiceWorkerTest("/service_worker/fail_on_variations_header_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests after the service worker does
// respondWith(fetch(request)).
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerRespondWithFetch) {
  ServiceWorkerTest("/service_worker/respond_with_fetch_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for service worker scripts when installing and updating.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest, ServiceWorkerScript) {
  // Register a service worker that imports scripts.
  GURL absolute_import = GetExampleUrlWithPath("/service_worker/empty.js");
  const std::string worker_path =
      "/service_worker/import_scripts_worker.js?import=" +
      base::EscapeQueryParamValue(absolute_import.spec(), false);
  RegisterServiceWorker(worker_path);

  // Test that the header is present on the main script request.
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleUrlWithPath(worker_path), "X-Client-Data"));

  // And on import script requests to Google.
  EXPECT_TRUE(HasReceivedHeader(
      GetGoogleUrlWithPath("/service_worker/empty.js"), "X-Client-Data"));

  // But not on requests not to Google.
  EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));

  // Prepare for the update case.
  ClearReceivedHeaders();

  // Tries to update the service worker.
  EXPECT_EQ("DONE", EvalJs(GetWebContents(), "update();"));

  // Test that the header is present on the main script request.
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleUrlWithPath(worker_path), "X-Client-Data"));

  // And on import script requests to Google.
  EXPECT_TRUE(HasReceivedHeader(
      GetGoogleUrlWithPath("/service_worker/empty.js"), "X-Client-Data"));
  // But not on requests not to Google.
  EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for shared worker scripts.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest, SharedWorkerScript) {
  WorkerScriptTest("/workers/create_shared_worker.html",
                   "/workers/import_scripts_shared_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for dedicated worker scripts.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTest,
                       DedicatedWorkerScript) {
  WorkerScriptTest("/workers/create_dedicated_worker.html",
                   "/workers/import_scripts_dedicated_worker.js");
}

INSTANTIATE_TEST_SUITE_P(All,
                         VariationsHttpHeadersBrowserTest,
                         testing::Bool());

namespace {

// A test fixture for testing prefetches from the Loading Predictor.
class VariationsHttpHeadersBrowserTestWithOptimizationGuide
    : public VariationsHttpHeadersBrowserTest {
 public:
  VariationsHttpHeadersBrowserTestWithOptimizationGuide() {
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {features::kLoadingPredictorPrefetch, {}},
        {features::kLoadingPredictorUseOptimizationGuide,
         {{"use_predictions_for_preconnect", "true"}}},
        {optimization_guide::features::kOptimizationHints, {}}};
    std::vector<base::test::FeatureRef> disabled = {
        features::kLoadingPredictorUseLocalPredictions};
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    VariationsHttpHeadersBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kLoadingPredictorAllowLocalRequestForTesting);
  }

  std::unique_ptr<content::TestNavigationManager> NavigateToURLAsync(
      const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    DCHECK(tab);
    auto observer = std::make_unique<content::TestNavigationManager>(tab, url);
    tab->GetController().LoadURL(url, content::Referrer(),
                                 ui::PAGE_TRANSITION_TYPED, std::string());
    return observer;
  }

  void SetUpOptimizationHint(
      const GURL& url,
      const std::vector<std::string>& predicted_subresource_urls) {
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(optimization_guide_keyed_service);

    optimization_guide::proto::LoadingPredictorMetadata
        loading_predictor_metadata;
    for (const auto& subresource_url : predicted_subresource_urls) {
      loading_predictor_metadata.add_subresources()->set_url(subresource_url);
    }

    optimization_guide::OptimizationMetadata optimization_metadata;
    optimization_metadata.set_loading_predictor_metadata(
        loading_predictor_metadata);
    optimization_guide_keyed_service->AddHintForTesting(
        url, optimization_guide::proto::LOADING_PREDICTOR,
        optimization_metadata);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Verify in an integration test that that the variations header (X-Client-Data)
// is correctly attached to prefetch requests from the Loading Predictor.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTestWithOptimizationGuide,
                       Prefetch) {
  GURL url = server()->GetURL("test.com", "/simple_page.html");
  GURL google_url = GetGoogleSubresourceUrl();
  GURL non_google_url = GetExampleUrl();

  // Set up optimization hints.
  std::vector<std::string> hints = {google_url.spec(), non_google_url.spec()};
  SetUpOptimizationHint(url, hints);

  // Navigate.
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  WaitForRequest(google_url);
  WaitForRequest(non_google_url);

  // Expect header on google urls only.
  EXPECT_TRUE(HasReceivedHeader(google_url, "X-Client-Data"));
  EXPECT_FALSE(HasReceivedHeader(non_google_url, "X-Client-Data"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VariationsHttpHeadersBrowserTestWithOptimizationGuide,
                         testing::Bool());
