// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
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
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_features.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_switches.h"
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
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_ui_test_utils.h"
#else
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace variations {
namespace {

constexpr char kTrialName[] = "t1";
constexpr std::string_view kSomeStudyName = "SomeStudy";
constexpr std::string_view kLimitedLayerStudyName = "LimitedLayerStudy";
constexpr int kGenericExperimentGroupId = 12;
constexpr int kGenericExperimentGroupTriggerId = 789;

struct ExperimentIdOptions {
  std::optional<int> id;
  std::optional<int> trigger_id;
};

// Returns a group named "Group" with its weight equal to 1 and the specified
// ID, if any. CHECKs if both IDs are given.
Study::Experiment CreateExperimentGroup(
    const ExperimentIdOptions& id_options = {}) {
  Study::Experiment group;
  group.set_name("Group");
  group.set_probability_weight(1);

  if (id_options.id.has_value()) {
    CHECK(id_options.trigger_id == std::nullopt);
    group.set_google_web_experiment_id(*id_options.id);
  }
  if (id_options.trigger_id.has_value()) {
    CHECK(id_options.id == std::nullopt);
    group.set_google_web_trigger_experiment_id(*id_options.trigger_id);
  }
  return group;
}

// Returns a seed with the following:
// * A 100-slot limited layer with a 100-slot layer member.
// * A limited-layer-constrained study with the given group and permanent
//   consistency.
// * A generic study not constrained to any layers.
//
// If a group isn't given, then the seed contains a limited layer, no
// layer-constrained studies, and a generic study. In practice, clients aren't
// expected to receive layers without studies that reference them.
VariationsSeed CreateTestSeedWithLimitedEntropyLayer(
    std::optional<Study::Experiment> limited_layer_study_group) {
  VariationsSeed seed;

  auto* layer = seed.add_layers();
  layer->set_id(123);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);

  auto* layer_member = layer->add_members();
  layer_member->set_id(1);
  auto* slot_range = layer_member->add_slots();
  slot_range->set_start(0);
  slot_range->set_end(99);

  Study base_study;
  base_study.set_activation_type(Study::ACTIVATE_ON_STARTUP);
  base_study.set_consistency(Study::PERMANENT);
  auto* filter = base_study.mutable_filter();
  filter->add_channel(Study::UNKNOWN);
  filter->add_channel(Study::CANARY);
  filter->add_channel(Study::DEV);
  filter->add_channel(Study::BETA);
  filter->add_channel(Study::STABLE);
  filter->add_platform(Study::PLATFORM_WINDOWS);
  filter->add_platform(Study::PLATFORM_MAC);
  filter->add_platform(Study::PLATFORM_LINUX);
  filter->add_platform(Study::PLATFORM_CHROMEOS);
  filter->add_platform(Study::PLATFORM_ANDROID);

  Study some_study = base_study;
  some_study.set_name(kSomeStudyName);
  *some_study.add_experiment() = CreateExperimentGroup();
  *seed.add_study() = some_study;

  if (!limited_layer_study_group.has_value()) {
    // Skip creating a layer-constrained study.
    return seed;
  }

  Study layer_study = base_study;
  layer_study.set_name(kLimitedLayerStudyName);
  *layer_study.add_experiment() = *limited_layer_study_group;
  auto* layer_member_reference = layer_study.mutable_layer();
  layer_member_reference->set_layer_id(123);
  layer_member_reference->add_layer_member_ids(1);
  *seed.add_study() = layer_study;

  return seed;
}

#if BUILDFLAG(IS_ANDROID)
TabModel* FindTabModelForProfile(content::BrowserContext* context) {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() == context) {
      return model;
    }
  }
  return nullptr;
}

void OpenUrlInNewTab(content::BrowserContext* context,
                     content::WebContents* parent,
                     const GURL& url) {
  CHECK(parent);
  CHECK(context);
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(parent);

  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(content::WebContents::CreateParams(context));
  content::WebContents* raw_web_contents = contents.get();
  auto* new_tab =
      tab_model->CreateTab(TabAndroid::FromWebContents(parent),
                           std::move(contents), TabModel::kInvalidIndex,
                           TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND,
                           /*should_pin=*/false);

  content::NavigateToURLBlockUntilNavigationsComplete(
      raw_web_contents, url, /*number_of_navigations=*/1,
      /*ignore_uncommitted_navigations=*/false);

  TabModel* new_tab_model = FindTabModelForProfile(context);
  ASSERT_NE(new_tab_model, nullptr);
  ASSERT_TRUE(new_tab_model->IsOffTheRecord());

  tabs::TabHandle new_tab_handle = new_tab->GetHandle();
  ASSERT_NE(new_tab_handle.raw_value(), 0);
  new_tab_model->ActivateTab(new_tab_handle);
}
#endif  // BUILDFLAG(IS_ANDROID)

GURL GetGoogleUrlWithPath(const std::string& path,
                          net::EmbeddedTestServer* server) {
  return server->GetURL("www.google.com", path);
}

GURL GetGoogleUrl(net::EmbeddedTestServer* server) {
  return GetGoogleUrlWithPath("/landing.html", server);
}

GURL GetGoogleIframeUrl(net::EmbeddedTestServer* server) {
  return GetGoogleUrlWithPath("/iframe.html", server);
}

GURL GetGoogleSubresourceFetchingWorkerUrl(net::EmbeddedTestServer* server) {
  return GetGoogleUrlWithPath("/subresource_fetch_worker.js", server);
}

GURL GetGoogleRedirectUrl1(net::EmbeddedTestServer* server) {
  return GetGoogleUrlWithPath("/redirect", server);
}

GURL GetGoogleRedirectUrl2(net::EmbeddedTestServer* server) {
  return GetGoogleUrlWithPath("/redirect2", server);
}

GURL GetGoogleSubresourceUrl(net::EmbeddedTestServer* server) {
  return GetGoogleUrlWithPath("/logo.png", server);
}

GURL GetExampleUrlWithPath(const std::string& path,
                           net::EmbeddedTestServer* server) {
  return server->GetURL("www.example.com", path);
}

GURL GetExampleUrl(net::EmbeddedTestServer* server) {
  return GetExampleUrlWithPath("/landing.html", server);
}

// Custom request handler that record request headers and simulates a redirect
// from google.com to example.com. It's expected to run on the IO thread.
std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
    net::EmbeddedTestServer* server,
    base::RepeatingCallback<void(GURL url,
                                 net::test_server::HttpRequest::HeaderMap)>
        record_header_callback,
    const net::test_server::HttpRequest& request) {
  // Retrieve the host name (without port) from the request headers.
  std::string host;
  if (request.headers.find("Host") != request.headers.end()) {
    host = request.headers.find("Host")->second;
  }
  if (host.find(':') != std::string::npos) {
    host = host.substr(0, host.find(':'));
  }

  // Recover the original URL of the request by replacing the host name in
  // request.GetURL() (which is 127.0.0.1) with the host name from the request
  // headers.
  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  GURL original_url = request.GetURL().ReplaceComponents(replacements);

  // Memorize the request headers for this URL for later verification.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(record_header_callback), original_url,
                                request.headers));

  // Set up a test server that redirects according to the
  // following redirect chain:
  // https://www.google.com:<port>/redirect
  // --> https://www.google.com:<port>/redirect2
  // --> https://www.example.com:<port>/
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  if (request.relative_url == GetGoogleRedirectUrl1(server).GetPath()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location",
                                   GetGoogleRedirectUrl2(server).spec());
  } else if (request.relative_url == GetGoogleRedirectUrl2(server).GetPath()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetExampleUrl(server).spec());
  } else if (request.relative_url == GetExampleUrl(server).GetPath()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/html");
  } else if (request.relative_url == GetGoogleIframeUrl(server).GetPath()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/html");
  } else if (request.relative_url ==
             GetGoogleSubresourceUrl(server).GetPath()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("");
    http_response->set_content_type("image/png");
  } else if (request.relative_url ==
             GetGoogleSubresourceFetchingWorkerUrl(server).GetPath()) {
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

class VariationHeaderSetter : public ChromeBrowserMainExtraParts {
 public:
  VariationHeaderSetter() = default;

  VariationHeaderSetter(const VariationHeaderSetter&) = delete;
  VariationHeaderSetter& operator=(const VariationHeaderSetter&) = delete;

  ~VariationHeaderSetter() override = default;

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override {
    // Set up some fake variations.
    auto* variations_provider = VariationsIdsProvider::GetInstance();
    variations_provider->ForceVariationIdsForTesting(
        {base::NumberToString(kGenericExperimentGroupId),
         "t" + base::NumberToString(kGenericExperimentGroupTriggerId)},
        "");
  }
};

class VariationsHttpHeadersBrowserTest : public PlatformBrowserTest {
 public:
  VariationsHttpHeadersBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}

  VariationsHttpHeadersBrowserTest(const VariationsHttpHeadersBrowserTest&) =
      delete;
  VariationsHttpHeadersBrowserTest& operator=(
      const VariationsHttpHeadersBrowserTest&) = delete;

  ~VariationsHttpHeadersBrowserTest() override = default;

  void TearDownOnMainThread() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/480962318): Remove this workaround when fixed.
    // On Android there seems to be a race between deinitialization of the
    // FeatureList through the browsertest and Android actual UI thread.
    // This results in rare crash in
    // BluetoothNotificationManager.clearBluetoothNotifications().
    // The workaround is to drain the RunLoop before allowing the test
    // to tear down.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    sync();
#endif
    PlatformBrowserTest::TearDownOnMainThread();
  }

  // TODO(crbug.com/452922329): Share the helpers around Android Incognito in
  // more general helper library if it actually doesn't flake for a while.
  void CreateIncognitoTab() {
    CHECK(!chrome_test_utils::GetProfile(this)->IsIncognitoProfile());
    const GURL url("about:blank");

#if BUILDFLAG(IS_ANDROID)
    Profile* otr_profile =
        chrome_test_utils::GetProfile(this)->GetPrimaryOTRProfile(
            /*create_if_needed=*/true);
    OpenUrlInNewTab(otr_profile, GetWebContents(), url);
#else
    Browser* incognito =
        CreateIncognitoBrowser(chrome_test_utils::GetProfile(this));
    SetBrowser(incognito);
    NavigateToURL(url);
#endif
    ASSERT_TRUE(chrome_test_utils::GetProfile(this)->IsIncognitoProfile());
  }

  bool NavigateToURL(const GURL& url) {
    return chrome_test_utils::NavigateToURL(GetWebContents(), url);
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    PlatformBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<VariationHeaderSetter>());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&VariationsHttpHeadersBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
    PlatformBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SetUp() override {
    server()->SetCertHostnames(
        {"www.google.com", "www.example.com", "test.com"});
    ASSERT_TRUE(server()->InitializeAndListen());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    content::NetworkConnectionChangeSimulator().SetConnectionType(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET);

    host_resolver()->AddRule("*", "127.0.0.1");

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    server()->ServeFilesFromDirectory(test_data_dir);

    server()->RegisterRequestHandler(base::BindRepeating(
        RequestHandler, server(),
        base::BindRepeating(
            &VariationsHttpHeadersBrowserTest::RecordHeadersForURL,
            base::Unretained(this))));

    server()->StartAcceptingConnections();
  }

  const net::EmbeddedTestServer* server() const { return &https_server_; }
  net::EmbeddedTestServer* server() { return &https_server_; }

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

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void GoogleWebVisibilityTopFrameTest(bool top_frame_is_first_party);

  // Registers a service worker for google.com root scope.
  void RegisterServiceWorker(const std::string& worker_path) {
    GURL url = GetGoogleUrlWithPath(
        "/service_worker/create_service_worker.html", server());
    EXPECT_TRUE(NavigateToURL(url));
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
        GetGoogleUrlWithPath("/service_worker/fetch_from_page.html", server());
    ASSERT_TRUE(NavigateToURL(page_url));
    EXPECT_TRUE(HasReceivedHeader(page_url, "X-Client-Data"));
    // Check that there is a controller to check that the test is really testing
    // service worker.
    EXPECT_EQ(true,
              EvalJs(GetWebContents(), "!!navigator.serviceWorker.controller"));

    // Verify subresource requests from the page also have X-Client-Data.
    EXPECT_EQ("hello",
              EvalJs(GetWebContents(),
                     base::StrCat({"fetch_from_page('",
                                   GetGoogleUrl(server()).spec(), "');"})));
    EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(server()), "X-Client-Data"));

    // But not if they are to non-Google domains.
    EXPECT_EQ("hello",
              EvalJs(GetWebContents(),
                     base::StrCat({"fetch_from_page('",
                                   GetExampleUrl(server()).spec(), "');"})));
    EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(server()), "X-Client-Data"));

    // Navigate to a Google URL which causes redirects.
    ASSERT_TRUE(NavigateToURL(GetGoogleRedirectUrl1(server())));

    // Verify redirect requests from google domains.
    // Redirect to google domains.
    EXPECT_TRUE(
        HasReceivedHeader(GetGoogleRedirectUrl1(server()), "X-Client-Data"));
    EXPECT_TRUE(
        HasReceivedHeader(GetGoogleRedirectUrl2(server()), "X-Client-Data"));

    // Redirect to non-google domains.
    EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(server()), "Host"));
    EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(server()), "X-Client-Data"));
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
    GURL absolute_import = GetExampleUrlWithPath("/workers/empty.js", server());
    const std::string worker_path = base::StrCat(
        {worker, "?import=",
         base::EscapeQueryParamValue(absolute_import.spec(), false)});
    GURL worker_url = GetGoogleUrlWithPath(worker_path, server());

    // Build the page URL that tells the page to create the worker.
    const std::string page_path =
        base::StrCat({page, "?worker_url=",
                      base::EscapeQueryParamValue(worker_url.spec(), false)});
    GURL page_url = GetGoogleUrlWithPath(page_path, server());

    // Navigate and test.
    EXPECT_TRUE(NavigateToURL(page_url));
    EXPECT_EQ("DONE", EvalJs(GetWebContents(), "waitForMessage();"));

    // The header should be on the main script request.
    EXPECT_TRUE(HasReceivedHeader(worker_url, "X-Client-Data"));

    // And on import script requests to Google.
    EXPECT_TRUE(HasReceivedHeader(
        GetGoogleUrlWithPath("/workers/empty.js", server()), "X-Client-Data"));

    // But not on requests not to Google.
    EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));
  }

 private:
  void RecordHeadersForURL(GURL url,
                           net::test_server::HttpRequest::HeaderMap headers);

  net::EmbeddedTestServer https_server_;

  // Stores the observed HTTP Request headers.
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_;

  // For waiting for requests.
  std::map<GURL, base::OnceClosure> done_callbacks_;

  // Holds the subscription to ensure the callback remains active during setup.
  base::CallbackListSubscription create_services_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);
};

void VariationsHttpHeadersBrowserTest::RecordHeadersForURL(
    GURL url,
    net::test_server::HttpRequest::HeaderMap headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_headers_[url] = headers;

  // Trigger the callback if we were waiting for this URL
  auto iter = done_callbacks_.find(url);
  if (iter != done_callbacks_.end()) {
    std::move(iter->second).Run();
    done_callbacks_.erase(iter);
  }
}
struct LimitedLayerTestParams {
  std::string test_name;
  Study::Experiment group;
};

class VariationsHttpHeadersBrowserTestWithLimitedLayerBase
    : public VariationsHttpHeadersBrowserTest {
 public:
  VariationsHttpHeadersBrowserTestWithLimitedLayerBase() = default;
  ~VariationsHttpHeadersBrowserTestWithLimitedLayerBase() override = default;

 protected:
  // BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAcceptEmptySeedSignatureForTesting);
    DisableTestingConfig();
    VariationsHttpHeadersBrowserTest::SetUpCommandLine(command_line);
  }

  bool SetUpUserDataDirectoryWithGroup(std::optional<Study::Experiment> group) {
    const base::FilePath user_data_dir =
        base::PathService::CheckedGet(chrome::DIR_USER_DATA);
    const base::FilePath seed_file_path =
        user_data_dir.AppendASCII("VariationsSeedV1");
    const base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);

    std::string serialized_seed = CreateTestSeedWithLimitedEntropyLayer(
                                      /*limited_layer_study_group=*/group)
                                      .SerializeAsString();
    std::string compressed_seed;
    compression::GzipCompress(serialized_seed, &compressed_seed);

    // Write the seed for the seed file experiment's treatment-group clients.
    CHECK(base::WriteFile(seed_file_path, compressed_seed));

    // Write the seed for the seed file experiment's control-group clients.
    base::DictValue local_state;
    local_state.SetByDottedPath(prefs::kVariationsCompressedSeed,
                                base::Base64Encode(compressed_seed));
    CHECK(JSONFileValueSerializer(local_state_path).Serialize(local_state));
    return true;
  }

  bool IsPrefDefaultValue(std::string_view pref_name) {
    return local_state()->FindPreference(pref_name)->IsDefaultValue();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }
};

class VariationsHttpHeadersBrowserTestWithActiveLimitedLayer
    : public VariationsHttpHeadersBrowserTestWithLimitedLayerBase,
      public ::testing::WithParamInterface<LimitedLayerTestParams> {
 public:
  VariationsHttpHeadersBrowserTestWithActiveLimitedLayer() = default;
  ~VariationsHttpHeadersBrowserTestWithActiveLimitedLayer() override = default;

 protected:
  // PlatformBrowserTest:
  bool SetUpUserDataDirectory() override {
    return VariationsHttpHeadersBrowserTestWithLimitedLayerBase::
        SetUpUserDataDirectoryWithGroup(/*group=*/GetParam().group);
  }
};

class VariationsHttpHeadersBrowserTestWithInactiveLimitedLayer
    : public VariationsHttpHeadersBrowserTestWithLimitedLayerBase {
 public:
  VariationsHttpHeadersBrowserTestWithInactiveLimitedLayer() = default;
  ~VariationsHttpHeadersBrowserTestWithInactiveLimitedLayer() override =
      default;

 protected:
  // PlatformBrowserTest:
  bool SetUpUserDataDirectory() override {
    return VariationsHttpHeadersBrowserTestWithLimitedLayerBase::
        SetUpUserDataDirectoryWithGroup(/*group=*/std::nullopt);
  }
};

class VariationsHttpHeadersBrowserTestWithSetLowEntropyRandomizationSource
    : public VariationsHttpHeadersBrowserTest {
 public:
  VariationsHttpHeadersBrowserTestWithSetLowEntropyRandomizationSource() =
      default;
  ~VariationsHttpHeadersBrowserTestWithSetLowEntropyRandomizationSource()
      override = default;

 protected:
  // PlatformBrowserTest:
  bool SetUpUserDataDirectory() override {
    const base::FilePath user_data_dir =
        base::PathService::CheckedGet(chrome::DIR_USER_DATA);
    const base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);

    base::DictValue local_state;
    local_state.SetByDottedPath(
        metrics::prefs::kMetricsLimitedEntropyRandomizationSource,
        "00000000000000000000000000000001");
    CHECK(JSONFileValueSerializer(local_state_path).Serialize(local_state));
    return true;
  }
};

class VariationsHttpHeadersBrowserTestWithSetLowEntropySource
    : public VariationsHttpHeadersBrowserTest {
 public:
  VariationsHttpHeadersBrowserTestWithSetLowEntropySource() = default;
  ~VariationsHttpHeadersBrowserTestWithSetLowEntropySource() override = default;

 protected:
  // PlatformBrowserTest:
  bool SetUpUserDataDirectory() override {
    const base::FilePath user_data_dir =
        base::PathService::CheckedGet(chrome::DIR_USER_DATA);
    const base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);

    // Write the seed for the seed file experiment's control-group clients.
    base::DictValue local_state;
    local_state.SetByDottedPath(metrics::prefs::kMetricsLowEntropySource, 5);
    CHECK(JSONFileValueSerializer(local_state_path).Serialize(local_state));
    return true;
  }
};

// Associates |id| with GOOGLE_WEB_PROPERTIES_SIGNED_IN and creates a field
// trial for it.
void CreateGoogleSignedInFieldTrial(VariationID id) {
  scoped_refptr<base::FieldTrial> trial_1(CreateTrialAndAssociateId(
      "t1", "g1", GOOGLE_WEB_PROPERTIES_SIGNED_IN, id));

  auto* provider = VariationsIdsProvider::GetInstance();
  mojom::VariationsHeadersPtr signed_in_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/true);
  mojom::VariationsHeadersPtr signed_out_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(signed_in_headers);
  ASSERT_TRUE(signed_out_headers);
  EXPECT_NE(
      signed_in_headers->headers_map.at(mojom::GoogleWebVisibility::ANY),
      signed_out_headers->headers_map.at(mojom::GoogleWebVisibility::ANY));
  EXPECT_NE(signed_in_headers->headers_map.at(
                mojom::GoogleWebVisibility::FIRST_PARTY),
            signed_out_headers->headers_map.at(
                mojom::GoogleWebVisibility::FIRST_PARTY));
}

// Creates FieldTrials associated with the FIRST_PARTY IDCollectionKeys and
// their corresponding ANY_CONTEXT keys.
void CreateFieldTrialsWithDifferentVisibilities() {
  scoped_refptr<base::FieldTrial> trial_1(CreateTrialAndAssociateId(
      "t1", "g1", GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, 11));
  scoped_refptr<base::FieldTrial> trial_2(CreateTrialAndAssociateId(
      "t2", "g2", GOOGLE_WEB_PROPERTIES_FIRST_PARTY, 22));
  scoped_refptr<base::FieldTrial> trial_3(CreateTrialAndAssociateId(
      "t3", "g3", GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT, 33));
  scoped_refptr<base::FieldTrial> trial_4(CreateTrialAndAssociateId(
      "t4", "g4", GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY, 44));

  auto* provider = VariationsIdsProvider::GetInstance();
  mojom::VariationsHeadersPtr signed_in_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/true);
  mojom::VariationsHeadersPtr signed_out_headers =
      provider->GetClientDataHeaders(/*is_signed_in=*/false);
  ASSERT_TRUE(signed_in_headers);
  ASSERT_TRUE(signed_out_headers);
  EXPECT_NE(signed_in_headers->headers_map.at(mojom::GoogleWebVisibility::ANY),
            signed_in_headers->headers_map.at(
                mojom::GoogleWebVisibility::FIRST_PARTY));
  EXPECT_NE(signed_out_headers->headers_map.at(mojom::GoogleWebVisibility::ANY),
            signed_out_headers->headers_map.at(
                mojom::GoogleWebVisibility::FIRST_PARTY));
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
      AssociateGoogleVariationIDForTesting(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                           trial->trial_name(), group_name, i);
    }
    trial->AppendGroup(group_name, 1);
  }
  // Activate the trial. This corresponds to ACTIVATE_ON_STARTUP for server-side
  // studies.
  trial->Activate();
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to network requests to Google but stripped on redirects.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromResourceRequest) {
  ASSERT_TRUE(NavigateToURL(GetGoogleRedirectUrl1(server())));

  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl1(server()), "X-Client-Data"));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl2(server()), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(server()), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(server()), "X-Client-Data"));
}

// Verify in an integration that that the variations header (X-Client-Data) is
// correctly attached and stripped from network requests.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromSubresourceRequest) {
  GURL url = server()->GetURL("/simple_page.html");
  NavigateToURL(url);
  EXPECT_TRUE(FetchResource(GetWebContents(), GetGoogleRedirectUrl1(server())));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl1(server()), "X-Client-Data"));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl2(server()), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(server()), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(server()), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, Incognito) {
  CreateIncognitoTab();
  ASSERT_TRUE(chrome_test_utils::GetProfile(this)->IsIncognitoProfile());
  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));

  EXPECT_FALSE(HasReceivedHeader(GetGoogleUrl(server()), "X-Client-Data"));

  EXPECT_TRUE(
      FetchResource(GetWebContents(), GetGoogleSubresourceUrl(server())));
  EXPECT_FALSE(
      HasReceivedHeader(GetGoogleSubresourceUrl(server()), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, UserSignedIn) {
  // Ensure GetClientDataHeader() returns different values when signed in vs
  // not signed in.
  VariationID signed_in_id = 8;
  CreateGoogleSignedInFieldTrial(signed_in_id);

  IdentityTestEnvironmentProfileAdaptor adaptor(
      chrome_test_utils::GetProfile(this));

  adaptor.identity_test_env()->MakePrimaryAccountAvailable(
      "main_email@gmail.com",
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync);

  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));

  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_TRUE(header);

  // Verify that the received header contains the ID.
  std::set<VariationID> ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(header.value(), &ids, &trigger_ids));
  EXPECT_TRUE(ids.contains(signed_in_id));

  // Verify that both headers returned by GetClientDataHeaders() contain the ID.
  mojom::VariationsHeadersPtr headers =
      VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          /*is_signed_in=*/true);
  ASSERT_TRUE(headers);

  const std::string variations_header_first_party =
      headers->headers_map.at(mojom::GoogleWebVisibility::FIRST_PARTY);
  const std::string variations_header_any_context =
      headers->headers_map.at(mojom::GoogleWebVisibility::ANY);

  std::set<VariationID> ids_first_party;
  std::set<VariationID> trigger_ids_first_party;
  ASSERT_TRUE(ExtractVariationIds(variations_header_first_party,
                                  &ids_first_party, &trigger_ids_first_party));
  EXPECT_TRUE(ids_first_party.contains(signed_in_id));

  std::set<VariationID> ids_any_context;
  std::set<VariationID> trigger_ids_any_context;
  ASSERT_TRUE(ExtractVariationIds(variations_header_any_context,
                                  &ids_any_context, &trigger_ids_any_context));

  EXPECT_TRUE(ids_any_context.contains(signed_in_id));
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, UserNotSignedIn) {
  // Ensure GetClientDataHeader() returns different values when signed in vs
  // not signed in.
  VariationID signed_in_id = 8;
  CreateGoogleSignedInFieldTrial(signed_in_id);

  // By default the user is not signed in.
  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));

  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_TRUE(header);

  // Verify that the received header does not contain the ID.
  std::set<VariationID> ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(header.value(), &ids, &trigger_ids));
  EXPECT_FALSE(ids.contains(signed_in_id));

  // Verify that both headers returned by GetClientDataHeaders() do not contain
  // the ID.
  mojom::VariationsHeadersPtr headers =
      VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          /*is_signed_in=*/false);
  ASSERT_TRUE(headers);

  const std::string variations_header_first_party =
      headers->headers_map.at(mojom::GoogleWebVisibility::FIRST_PARTY);
  const std::string variations_header_any_context =
      headers->headers_map.at(mojom::GoogleWebVisibility::ANY);

  std::set<VariationID> ids_first_party;
  std::set<VariationID> trigger_ids_first_party;
  ASSERT_TRUE(ExtractVariationIds(variations_header_first_party,
                                  &ids_first_party, &trigger_ids_first_party));
  EXPECT_FALSE(ids_first_party.contains(signed_in_id));

  std::set<VariationID> ids_any_context;
  std::set<VariationID> trigger_ids_any_context;
  ASSERT_TRUE(ExtractVariationIds(variations_header_any_context,
                                  &ids_any_context, &trigger_ids_any_context));

  EXPECT_FALSE(ids_any_context.contains(signed_in_id));
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTestWithSetLowEntropySource,
                       CheckLowEntropySourceValue) {
  auto entropy_providers = g_browser_process->GetMetricsServicesManager()
                               ->CreateEntropyProvidersForTesting();
  // `with_google_web_experiment_ids` is true so that the low entropy provider
  // is used for randomization.
  CreateFieldTrial(entropy_providers->low_entropy(),
                   /*with_google_web_experiment_ids=*/true);

  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_TRUE(header);

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(
      ExtractVariationIds(header.value(), &variation_ids, &trigger_ids));

  // 3320983 is the offset value of kLowEntropySourceVariationIdRangeMin + 5.
  EXPECT_TRUE(variation_ids.contains(3320983));

  // Check that the reported group in the header is consistent with the low
  // entropy source. 33 is the group that is derived from the low entropy source
  // value of 5.
  EXPECT_TRUE(variation_ids.contains(33));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VariationsHttpHeadersBrowserTestWithActiveLimitedLayer,
    ::testing::Values(
        LimitedLayerTestParams{.test_name = "LimitedLayerStudyWithExperimentID",
                               .group = CreateExperimentGroup({.id = 3389050})},
        LimitedLayerTestParams{
            .test_name = "LimitedLayerStudyWithTriggerExperimentID",
            .group = CreateExperimentGroup({.trigger_id = 3389051})},
        LimitedLayerTestParams{
            .test_name = "LimitedLayerStudyWithoutExperimentIDs",
            .group = CreateExperimentGroup()}),
    [](const ::testing::TestParamInfo<LimitedLayerTestParams>& params) {
      return params.param.test_name;
    });

// Verifies that a client's low entropy source value is omitted from the
// X-Client-Data header when a seed with an active limited layer is applied. A
// limited layer is active when a limited-layer-constrained study applies to the
// client's channel, platform, and Chrome version.
IN_PROC_BROWSER_TEST_P(VariationsHttpHeadersBrowserTestWithActiveLimitedLayer,
                       OmitLowEntropySource) {
  // Check that both the low and limited entropy sources have been generated.
  ASSERT_FALSE(IsPrefDefaultValue(
      metrics::prefs::kMetricsLimitedEntropyRandomizationSource));
  ASSERT_FALSE(IsPrefDefaultValue(metrics::prefs::kMetricsLowEntropySource));

  // Check that the seed was applied by checking that the generic study was
  // registered.
  ASSERT_TRUE(base::FieldTrialList::TrialExists(kSomeStudyName));

  // Check that the limited-layer-constrained study was also registered.
  ASSERT_TRUE(base::FieldTrialList::TrialExists(kLimitedLayerStudyName));

  // Cause the study group's experiment ID (if any) to be included in eligible
  // X-Client-Data headers.
  base::FieldTrialList::Find(kLimitedLayerStudyName)->Activate();

  // Make a request and get its VariationIDs.
  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_FALSE(header == std::nullopt);
  std::set<VariationID> ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(header.value(), &ids, &trigger_ids));

  // Check that the client's offset low entropy source value was omitted from
  // the X-Client-Data header.
  const int low_entropy_source =
      local_state()->GetInteger(metrics::prefs::kMetricsLowEntropySource);
  const int offset_low_entropy_source =
      low_entropy_source + internal::kLowEntropySourceVariationIdRangeMin;
  EXPECT_FALSE(ids.contains(offset_low_entropy_source));
  EXPECT_FALSE(trigger_ids.contains(offset_low_entropy_source));

  std::set<VariationID> expected_ids{kGenericExperimentGroupId};
  std::set<VariationID> expected_trigger_ids{kGenericExperimentGroupTriggerId};
  std::optional<Study::Experiment> limited_layer_study_group = GetParam().group;
  if (limited_layer_study_group.has_value() &&
      limited_layer_study_group->has_google_web_experiment_id()) {
    expected_ids.insert(limited_layer_study_group->google_web_experiment_id());
  } else if (limited_layer_study_group.has_value() &&
             limited_layer_study_group
                 ->has_google_web_trigger_experiment_id()) {
    expected_trigger_ids.insert(
        limited_layer_study_group->google_web_trigger_experiment_id());
  }
  EXPECT_THAT(ids, ::testing::UnorderedElementsAreArray(expected_ids));
  EXPECT_THAT(trigger_ids,
              ::testing::UnorderedElementsAreArray(expected_trigger_ids));
}

// Verifies that a client's low entropy source value is included in the
// X-Client-Data header when a seed with an inactive limited layer is applied. A
// limited layer is inactive when the seed contains a limited layer but no
// limited-layer-constrained studies apply to the client's channel, platform,
// and Chrome version.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTestWithInactiveLimitedLayer,
                       SendLowEntropySource) {
  // Check that both the low and limited entropy sources have been generated.
  ASSERT_FALSE(IsPrefDefaultValue(
      metrics::prefs::kMetricsLimitedEntropyRandomizationSource));
  ASSERT_FALSE(IsPrefDefaultValue((metrics::prefs::kMetricsLowEntropySource)));

  // Check that the seed was applied by checking that the generic study was
  // registered.
  ASSERT_TRUE(base::FieldTrialList::TrialExists(kSomeStudyName));

  // Check that the limited-layer-constrained study was not registered.
  ASSERT_FALSE(base::FieldTrialList::TrialExists(kLimitedLayerStudyName));

  // Make a request and get its VariationIDs.
  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_FALSE(header == std::nullopt);
  std::set<VariationID> ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(ExtractVariationIds(header.value(), &ids, &trigger_ids));

  // Check that the client's offset low entropy source value was included in
  // the X-Client-Data header.
  const int low_entropy_source =
      local_state()->GetInteger(metrics::prefs::kMetricsLowEntropySource);
  const int offset_low_entropy_source =
      low_entropy_source + internal::kLowEntropySourceVariationIdRangeMin;
  EXPECT_THAT(ids, ::testing::UnorderedElementsAreArray(
                       {kGenericExperimentGroupId, offset_low_entropy_source}));
  EXPECT_THAT(trigger_ids, ::testing::UnorderedElementsAreArray(
                               {kGenericExperimentGroupTriggerId}));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTestWithSetLowEntropyRandomizationSource,
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

  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_TRUE(header);

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(
      ExtractVariationIds(header.value(), &variation_ids, &trigger_ids));

  // 56 is the group that is derived from the setup in
  // `PRE_CheckGoogleWebExperimentIdUnderLimitedEntropyRandomization`.
  EXPECT_EQ("56", base::FieldTrialList::FindFullName(kTrialName));
  // Check that the reported group in the header is consistent with the
  // limited entropy randomization source.
  EXPECT_TRUE(variation_ids.contains(56));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTestWithSetLowEntropyRandomizationSource,
    LimitedEntropyRandomization_ExperimentLoggingWithoutGoogleWebExperimentationId) {
  // CreateEntropyProvidersForTesting() ensures a limited entropy provider is
  // created.
  auto entropy_providers = g_browser_process->GetMetricsServicesManager()
                               ->CreateEntropyProvidersForTesting();
  ASSERT_TRUE(entropy_providers->has_limited_entropy());
  CreateFieldTrial(entropy_providers->limited_entropy(),
                   /*with_google_web_experiment_ids=*/false);

  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));
  std::optional<std::string> header =
      GetReceivedHeader(GetGoogleUrl(server()), "X-Client-Data");
  ASSERT_TRUE(header);

  std::set<VariationID> variation_ids;
  std::set<VariationID> trigger_ids;
  ASSERT_TRUE(
      ExtractVariationIds(header.value(), &variation_ids, &trigger_ids));

  // 56 is the group that is derived from the setup in initial local state.
  EXPECT_EQ("56", base::FieldTrialList::FindFullName(kTrialName));
  // The experiment does not have a google_web_experiment_id and thus should
  // NOT appear in the header.
  EXPECT_FALSE(variation_ids.contains(56));
}

void VariationsHttpHeadersBrowserTest::GoogleWebVisibilityTopFrameTest(
    bool top_frame_is_first_party) {
  CreateFieldTrialsWithDifferentVisibilities();
  mojom::VariationsHeadersPtr signed_out_headers =
      VariationsIdsProvider::GetInstance()->GetClientDataHeaders(
          /*is_signed_in=*/false);
  ASSERT_TRUE(signed_out_headers);

  const std::string expected_header_value =
      top_frame_is_first_party
          ? signed_out_headers->headers_map.at(
                mojom::GoogleWebVisibility::FIRST_PARTY)
          : signed_out_headers->headers_map.at(mojom::GoogleWebVisibility::ANY);

  // Load a top frame.
  const GURL top_frame_url = top_frame_is_first_party ? GetGoogleUrl(server())
                                                      : GetExampleUrl(server());
  ASSERT_TRUE(NavigateToURL(top_frame_url));
  if (top_frame_is_first_party) {
    EXPECT_EQ(GetReceivedHeader(top_frame_url, "X-Client-Data"),
              expected_header_value);
  } else {
    EXPECT_FALSE(GetReceivedHeader(top_frame_url, "X-Client-Data"));
  }

  // Load Google iframe.
  EXPECT_TRUE(LoadIframe(GetWebContents(), GetGoogleIframeUrl(server())));
  EXPECT_EQ(GetReceivedHeader(GetGoogleIframeUrl(server()), "X-Client-Data"),
            expected_header_value);

  // Fetch Google subresource.
  EXPECT_TRUE(FetchResource(ChildFrameAt(GetWebContents(), 0),
                            GetGoogleSubresourceUrl(server())));
  EXPECT_EQ(
      GetReceivedHeader(GetGoogleSubresourceUrl(server()), "X-Client-Data"),
      expected_header_value);

  // Prepare for loading Google subresource from a dedicated worker. The same
  // URL subresource was loaded above. So need to clear `received_headers_`.
  ClearReceivedHeaders();

  // Start Google worker and fetch Google subresource from the worker.
  EXPECT_TRUE(RunSubresourceFetchingWorker(
      ChildFrameAt(GetWebContents(), 0),
      GetGoogleSubresourceFetchingWorkerUrl(server()),
      GetGoogleSubresourceUrl(server())));
  EXPECT_EQ(GetReceivedHeader(GetGoogleSubresourceFetchingWorkerUrl(server()),
                              "X-Client-Data"),
            expected_header_value);
  EXPECT_EQ(
      GetReceivedHeader(GetGoogleSubresourceUrl(server()), "X-Client-Data"),
      expected_header_value);
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestGoogleWebVisibilityInFirstPartyContexts) {
  GoogleWebVisibilityTopFrameTest(/*top_frame_is_first_party=*/true);
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestGoogleWebVisibilityInThirdPartyContexts) {
  GoogleWebVisibilityTopFrameTest(/*top_frame_is_first_party=*/false);
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithProfileNetworkContext) {
  GURL url = GetGoogleRedirectUrl1(server());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> loader =
      CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request), InIncognito::kNo,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  content::StoragePartition* partition =
      chrome_test_utils::GetProfile(this)->GetDefaultStoragePartition();
  network::SharedURLLoaderFactory* loader_factory =
      partition->GetURLLoaderFactoryForBrowserProcess().get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallback());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_EQ(net::OK, loader->NetError());
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl1(server()), "X-Client-Data"));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl2(server()), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(server()), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(server()), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithGlobalSystemNetworkContext) {
  GURL url = GetGoogleRedirectUrl1(server());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;

  std::unique_ptr<network::SimpleURLLoader> loader =
      CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request), InIncognito::kNo,
          TRAFFIC_ANNOTATION_FOR_TESTS);

  network::SharedURLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory()
          .get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallback());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_EQ(net::OK, loader->NetError());
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl1(server()), "X-Client-Data"));
  EXPECT_TRUE(
      HasReceivedHeader(GetGoogleRedirectUrl2(server()), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(server()), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(server()), "X-Client-Data"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to service worker navigation preload requests. Regression test
// for https://crbug.com/40588908.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerNavigationPreload) {
  // Register a service worker that uses navigation preload.
  RegisterServiceWorker("/service_worker/navigation_preload_worker.js");

  // Verify "X-Client-Data" is present on the navigation to Google.
  // Also test that "Service-Worker-Navigation-Preload" is present to verify
  // we are really testing the navigation preload request.
  ASSERT_TRUE(NavigateToURL(GetGoogleUrl(server())));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(server()), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleUrl(server()),
                                "Service-Worker-Navigation-Preload"));
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests after the service worker falls back to network.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerNetworkFallback) {
  ServiceWorkerTest("/service_worker/network_fallback_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// not exposed in the service worker fetch event.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerDoesNotSeeHeader) {
  ServiceWorkerTest("/service_worker/fail_on_variations_header_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests after the service worker does
// respondWith(fetch(request)).
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       ServiceWorkerRespondWithFetch) {
  ServiceWorkerTest("/service_worker/respond_with_fetch_worker.js");
}

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for service worker scripts when installing and updating.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, ServiceWorkerScript) {
  // Register a service worker that imports scripts.
  GURL absolute_import =
      GetExampleUrlWithPath("/service_worker/empty.js", server());
  const std::string worker_path =
      "/service_worker/import_scripts_worker.js?import=" +
      base::EscapeQueryParamValue(absolute_import.spec(), false);
  RegisterServiceWorker(worker_path);

  // Test that the header is present on the main script request.
  EXPECT_TRUE(HasReceivedHeader(GetGoogleUrlWithPath(worker_path, server()),
                                "X-Client-Data"));

  // And on import script requests to Google.
  EXPECT_TRUE(HasReceivedHeader(
      GetGoogleUrlWithPath("/service_worker/empty.js", server()),
      "X-Client-Data"));

  // But not on requests not to Google.
  EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));

  // Prepare for the update case.
  ClearReceivedHeaders();

  // Tries to update the service worker.
  EXPECT_EQ("DONE", EvalJs(GetWebContents(), "update();"));

  // Test that the header is present on the main script request.
  EXPECT_TRUE(HasReceivedHeader(GetGoogleUrlWithPath(worker_path, server()),
                                "X-Client-Data"));

  // And on import script requests to Google.
  EXPECT_TRUE(HasReceivedHeader(
      GetGoogleUrlWithPath("/service_worker/empty.js", server()),
      "X-Client-Data"));
  // But not on requests not to Google.
  EXPECT_FALSE(HasReceivedHeader(absolute_import, "X-Client-Data"));
}

#if !BUILDFLAG(IS_ANDROID)
// Shared workers are not available on Android.

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for shared worker scripts.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest, SharedWorkerScript) {
  WorkerScriptTest("/workers/create_shared_worker.html",
                   "/workers/import_scripts_shared_worker.js");
}
#endif

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to requests for dedicated worker scripts.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       DedicatedWorkerScript) {
  WorkerScriptTest("/workers/create_dedicated_worker.html",
                   "/workers/import_scripts_dedicated_worker.js");
}

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
        ::switches::kLoadingPredictorAllowLocalRequestForTesting);
  }

  std::unique_ptr<content::TestNavigationManager> NavigateToURLAsync(
      const GURL& url) {
    content::WebContents* tab = GetWebContents();
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
            chrome_test_utils::GetProfile(this));
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

// Verify in an integration test that that the variations header (X-Client-Data)
// is correctly attached to prefetch requests from the Loading Predictor.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTestWithOptimizationGuide,
                       Prefetch) {
  GURL url = server()->GetURL("test.com", "/simple_page.html");
  GURL google_url = GetGoogleSubresourceUrl(server());
  GURL non_google_url = GetExampleUrl(server());

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

}  // namespace
}  // namespace variations
