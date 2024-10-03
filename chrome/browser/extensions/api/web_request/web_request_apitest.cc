// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/error_console/error_console_test_observer.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/google/core/common/google_switches.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "google_apis/gaia/gaia_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::WebContents;

namespace extensions {

namespace {

// This is the public key of tools/origin_trials/eftest.key, used to validate
// origin trial tokens generated by tools/origin_trials/generate_token.py.
constexpr char kOriginTrialPublicKeyForTesting[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

// Observer that listens for messages from chrome.test.sendMessage to allow them
// to be used to trigger browser initiated naviagations from the javascript for
// testing purposes.
class NavigateTabMessageHandler {
 public:
  explicit NavigateTabMessageHandler(Profile* profile) : profile_(profile) {
    navigate_listener_.SetOnRepeatedlySatisfied(base::BindRepeating(
        &NavigateTabMessageHandler::HandleNavigateTabMessage,
        base::Unretained(this)));
  }

  ~NavigateTabMessageHandler() = default;

 private:
  void HandleNavigateTabMessage(const std::string& message) {
    std::optional<base::Value> command = base::JSONReader::Read(message);
    if (command && command->is_dict()) {  // Check the message decoded from JSON
      base::Value::Dict* data = command->GetDict().FindDict("navigate");
      if (data) {
        int tab_id = data->FindInt("tabId").value();
        GURL url = GURL(*data->FindString("url"));
        ASSERT_TRUE(url.is_valid());

        content::WebContents* contents = nullptr;
        ExtensionTabUtil::GetTabById(
            tab_id, profile_, profile_->HasPrimaryOTRProfile(), &contents);
        ASSERT_NE(contents, nullptr)
            << "Could not find tab with id: " << tab_id;
        content::NavigationController::LoadURLParams params(url);
        contents->GetController().LoadURLWithParams(params);
      }
    }
    navigate_listener_.Reset();
  }

  raw_ptr<Profile, DanglingUntriaged> profile_;
  ExtensionTestMessageListener navigate_listener_;
};

// Sends an XHR request to the provided host, port, and path, and responds when
// the request was sent.
const char kPerformXhrJs[] =
    "var url = 'http://%s:%d/%s';\n"
    "var xhr = new XMLHttpRequest();\n"
    "xhr.open('GET', url);\n"
    "new Promise(resolve => {"
    "  xhr.onload = function() {\n"
    "    resolve(true);\n"
    "  };\n"
    "  xhr.onerror = function() {\n"
    "    resolve(false);\n"
    "  };\n"
    "  xhr.send();\n"
    "});\n";

// Header values set by the server and by the extension.
const char kHeaderValueFromExtension[] = "ValueFromExtension";
const char kHeaderValueFromServer[] = "ValueFromServer";

constexpr char kCORSUrl[] = "http://cors.test/cors";
constexpr char kCORSProxyUser[] = "testuser";
constexpr char kCORSProxyPass[] = "testpass";
constexpr char kCustomPreflightHeader[] = "x-testheader";

// Performs an XHR in the given |frame|, replying when complete.
void PerformXhrInFrame(content::RenderFrameHost* frame,
                      const std::string& host,
                      int port,
                      const std::string& page) {
  EXPECT_EQ(true, EvalJs(frame, base::StringPrintf(kPerformXhrJs, host.c_str(),
                                                   port, page.c_str())));
}

base::Value ExecuteScriptAndReturnValue(const ExtensionId& extension_id,
                                        content::BrowserContext* context,
                                        const std::string& script) {
  return BackgroundScriptExecutor::ExecuteScript(
      context, extension_id, script,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
}

std::optional<bool> ExecuteScriptAndReturnBool(const ExtensionId& extension_id,
                                               content::BrowserContext* context,
                                               const std::string& script) {
  std::optional<bool> result;
  base::Value script_result =
      ExecuteScriptAndReturnValue(extension_id, context, script);
  if (script_result.is_bool())
    result = script_result.GetBool();
  return result;
}

std::optional<std::string> ExecuteScriptAndReturnString(
    const ExtensionId& extension_id,
    content::BrowserContext* context,
    const std::string& script) {
  std::optional<std::string> result;
  base::Value script_result =
      ExecuteScriptAndReturnValue(extension_id, context, script);
  if (script_result.is_string())
    result = script_result.GetString();
  return result;
}

// Returns the current count of a variable stored in the |extension| background
// script context (either background page or service worker). Returns -1 if
// something goes awry.
int GetCountFromBackgroundScript(const Extension* extension,
                                 content::BrowserContext* context,
                                 const std::string& variable_name) {
  const std::string script = base::StringPrintf(
      "chrome.test.sendScriptResult(%s)", variable_name.c_str());
  base::Value value =
      ExecuteScriptAndReturnValue(extension->id(), context, script);
  if (!value.is_int())
    return -1;
  return value.GetInt();
}

// Returns the current count of webRequests received by the |extension| in
// the background script, either background page or service worker. Assumes the
// extension stores a value on the `self` object. Returns -1 if something goes
// awry.
int GetWebRequestCountFromBackgroundScript(const Extension* extension,
                                           content::BrowserContext* context) {
  return GetCountFromBackgroundScript(extension, context,
                                      "self.webRequestCount");
}

// Returns true if the |extension|'s background script saw an event for a
// request with the given |hostname| (|hostname| should exclude port).
bool HasSeenWebRequestInBackgroundScript(const Extension* extension,
                                         content::BrowserContext* context,
                                         const std::string& hostname) {
  const std::string script = base::StringPrintf(
      R"(chrome.test.sendScriptResult(
             self.requestedHostnames.includes('%s'));)",
      hostname.c_str());
  base::Value value =
      ExecuteScriptAndReturnValue(extension->id(), context, script);
  DCHECK(value.is_bool());
  return value.GetBool();
}

void WaitForExtraHeadersListener(base::WaitableEvent* event,
                                 content::BrowserContext* browser_context) {
  if (BrowserContextKeyedAPIFactory<WebRequestAPI>::Get(browser_context)
          ->HasExtraHeadersListenerForTesting()) {
    event->Signal();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WaitForExtraHeadersListener, event, browser_context));
}

}  // namespace

class ExtensionWebRequestApiTest : public ExtensionApiTest {
 public:
  explicit ExtensionWebRequestApiTest(
      ContextType context_type = ContextType::kFromManifest)
      : ExtensionApiTest(context_type) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        /*disabled_features=*/
        {features::kHttpsUpgrades, features::kHttpsFirstModeIncognito});
  }
  ExtensionWebRequestApiTest(const ExtensionWebRequestApiTest&) = delete;
  ExtensionWebRequestApiTest& operator=(const ExtensionWebRequestApiTest&) =
      delete;
  ~ExtensionWebRequestApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    navigationHandler_ = std::make_unique<NavigateTabMessageHandler>(profile());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kGaiaUrl, "http://gaia.com");
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialPublicKeyForTesting);
  }

  void RunPermissionTest(const char* extension_directory,
                         bool load_extension_with_incognito_permission,
                         bool wait_for_extension_loaded_in_incognito,
                         const char* expected_content_regular_window,
                         const char* exptected_content_incognito_window,
                         ContextType context_type);

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateURLLoaderFactory() {
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->automatically_assign_isolation_info = true;
    params->is_orb_enabled = false;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory;
    profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->CreateURLLoaderFactory(
            loader_factory.InitWithNewPipeAndPassReceiver(), std::move(params));
    return loader_factory;
  }

  void InstallWebRequestExtension(const std::string& name) {
    constexpr char kManifest[] = R"({
      "name": "%s",
      "version": "1",
      "manifest_version": 2,
      "permissions": [
        "webRequest"
      ]
    })";
    TestExtensionDir dir;
    dir.WriteManifest(base::StringPrintf(kManifest, name.c_str()));
    LoadExtension(dir.UnpackedPath());
    test_dirs_.push_back(std::move(dir));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::vector<TestExtensionDir> test_dirs_;
  std::unique_ptr<NavigateTabMessageHandler> navigationHandler_;
};

using ContextType = ExtensionBrowserTest::ContextType;

enum class BackgroundResourceFetchTestCase {
  kBackgroundResourceFetchEnabled,
  kBackgroundResourceFetchDisabled,
};

class ExtensionWebRequestApiTestWithContextType
    : public ExtensionWebRequestApiTest,
      public testing::WithParamInterface<
          std::pair<ContextType, BackgroundResourceFetchTestCase>> {
 public:
  ExtensionWebRequestApiTestWithContextType()
      : ExtensionWebRequestApiTest(GetParam().first) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsBackgroundResourceFetchEnabled()) {
      enabled_features.push_back(blink::features::kBackgroundResourceFetch);
    } else {
      disabled_features.push_back(blink::features::kBackgroundResourceFetch);
    }
    feature_background_resource_fetch_.InitWithFeatures(enabled_features,
                                                        disabled_features);
  }
  ExtensionWebRequestApiTestWithContextType(
      const ExtensionWebRequestApiTestWithContextType&) = delete;
  ExtensionWebRequestApiTestWithContextType& operator=(
      const ExtensionWebRequestApiTestWithContextType&) = delete;
  ~ExtensionWebRequestApiTestWithContextType() override = default;

  struct PrintToStringParamName {
    std::string operator()(
        const testing::TestParamInfo<
            std::pair<ContextType, BackgroundResourceFetchTestCase>>& info)
        const {
      switch (info.param.second) {
        case BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled:
          return "BackgroundResourceFetchEnabled";
        case BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled:
          return "BackgroundResourceFetchDisabled";
      }
    }
  };

 protected:
  ContextType GetContextType() const { return GetParam().first; }

 private:
  bool IsBackgroundResourceFetchEnabled() const {
    return GetParam().second ==
           BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled;
  }
  base::test::ScopedFeatureList feature_background_resource_fetch_;
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ExtensionWebRequestApiTestWithContextType,
    ::testing::Values(
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionWebRequestApiTestWithContextType,
    ::testing::Values(
        std::make_pair(
            ContextType::kServiceWorkerMV2,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kServiceWorkerMV2,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

class DevToolsFrontendInWebRequestApiTest : public ExtensionApiTest {
 public:
  DevToolsFrontendInWebRequestApiTest() {
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    int port = embedded_test_server()->port();

    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&DevToolsFrontendInWebRequestApiTest::OnIntercept,
                            base::Unretained(this), port));

    navigationHandler_ = std::make_unique<NavigateTabMessageHandler>(profile());
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ExtensionApiTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);

    test_root_dir_ = test_data_dir_.AppendASCII("webrequest");

    embedded_test_server()->ServeFilesFromDirectory(test_root_dir_);
    ASSERT_TRUE(StartEmbeddedTestServer());
    command_line->AppendSwitchASCII(
        switches::kCustomDevtoolsFrontend,
        embedded_test_server()
            ->GetURL("customfrontend.example.com", "/devtoolsfrontend/")
            .spec());
  }

 private:
  bool OnIntercept(int test_server_port,
                   content::URLLoaderInterceptor::RequestParams* params) {
    // The devtools remote frontend URLs are hardcoded into Chrome and are
    // requested by some of the tests here to exercise their behavior with
    // respect to WebRequest.
    //
    // We treat any URL request not targeting the test server as targeting the
    // remote frontend, and we intercept them to fulfill from test data rather
    // than hitting the network.
    if (params->url_request.url.EffectiveIntPort() == test_server_port)
      return false;

    std::string status_line;
    std::string contents;
    GetFileContents(
        test_root_dir_.AppendASCII(params->url_request.url.path().substr(1)),
        &status_line, &contents);
    content::URLLoaderInterceptor::WriteResponse(status_line, contents,
                                                 params->client.get());
    return true;
  }

  static void GetFileContents(const base::FilePath& path,
                              std::string* status_line,
                              std::string* contents) {
    base::ScopedAllowBlockingForTesting allow_io;
    if (!base::ReadFileToString(path, contents)) {
      *status_line = "HTTP/1.0 404 Not Found\n\n";
      return;
    }

    std::string content_type;
    if (path.Extension() == FILE_PATH_LITERAL(".html"))
      content_type = "Content-type: text/html\n";
    else if (path.Extension() == FILE_PATH_LITERAL(".js"))
      content_type = "Content-type: application/javascript\n";

    *status_line =
        base::StringPrintf("HTTP/1.0 200 OK\n%s\n", content_type.c_str());
  }

  base::test::ScopedFeatureList feature_list_;
  base::FilePath test_root_dir_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  std::unique_ptr<NavigateTabMessageHandler> navigationHandler_;
};

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestApi) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_api")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestSimple) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_simple")) << message_;
}

// TODO(crbug.com/333791060): Parameterized test is flaky on multiple bots.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_WebRequestComplex) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_complex")) << message_;
}

class ExtensionDevToolsProtocolTest
    : public ExtensionWebRequestApiTestWithContextType,
      public content::TestDevToolsProtocolClient {
 protected:
  void Attach() { AttachToWebContents(web_contents()); }

  void TearDownOnMainThread() override {
    DetachProtocolClient();
    ExtensionWebRequestApiTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ExtensionDevToolsProtocolTest,
    ::testing::Values(
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionDevToolsProtocolTest,
    ::testing::Values(
        std::make_pair(
            ContextType::kServiceWorkerMV2,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kServiceWorkerMV2,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(ExtensionDevToolsProtocolTest,
                       HeaderOverriddenByExtension) {
  Attach();
  ASSERT_TRUE(embedded_test_server()->Start());
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Header Override Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        chrome.webRequest.onHeadersReceived.addListener(function(details) {
            var headers = details.responseHeaders;
            headers.push({name: "extensionHeaderName",
                value: "extensionHeaderValue"});
            return {responseHeaders: headers};
          },
          {urls: ['<all_urls>']},
          ['responseHeaders', 'extraHeaders', 'blocking']);
        chrome.test.sendMessage('ready');
      )");

  ExtensionTestMessageListener listener("ready");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  SendCommand("Network.enable", base::Value::Dict(), true);
  const GURL url(
      embedded_test_server()->GetURL("/set-cookie?cookieName=cookieValue"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // Check that `Network.responseReceived` contains the response header added
  // by the extension
  base::Value::Dict response_received_result =
      WaitForNotification("Network.responseReceived", false);
  auto* extension_header = response_received_result.FindByDottedPath(
      "response.headers.extensionHeaderName");
  ASSERT_TRUE(extension_header);
  ASSERT_EQ(*extension_header, "extensionHeaderValue");

  // Check that the cookie as specified in the original headers has been set
  auto* get_all_cookies_result =
      SendCommand("Network.getAllCookies", base::Value::Dict(), true);
  const base::Value::List* cookies =
      get_all_cookies_result->FindList("cookies");
  ASSERT_TRUE(cookies);
  ASSERT_EQ(cookies->size(), 1u);
  ASSERT_TRUE(cookies->front().is_dict());
  auto* cookie_name = cookies->front().GetDict().FindString("name");
  ASSERT_TRUE(cookie_name);
  ASSERT_EQ(*cookie_name, "cookieName");
  auto* cookie_value = cookies->front().GetDict().FindString("value");
  ASSERT_TRUE(cookie_value);
  ASSERT_EQ(*cookie_value, "cookieValue");
}

IN_PROC_BROWSER_TEST_P(ExtensionDevToolsProtocolTest,
                       HeaderOverrideViaProtocolAllowedByExtension) {
  Attach();
  ASSERT_TRUE(embedded_test_server()->Start());
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Header Override Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        chrome.webRequest.onHeadersReceived.addListener(function(details) {
            var headers = details.responseHeaders;
            headers.push({name: "extensionHeaderName",
                value: "extensionHeaderValue"});
            return {responseHeaders: headers};
          },
          {urls: ['<all_urls>']},
          ['responseHeaders', 'extraHeaders', 'blocking']);
        chrome.test.sendMessage('ready');
      )");

  ExtensionTestMessageListener listener("ready");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  SendCommand("Network.enable", base::Value::Dict(), true);

  base::Value::Dict enable_params;
  base::Value::List patterns;
  base::Value::Dict pattern;
  pattern.Set("requestStage", "Response");
  patterns.Append(std::move(pattern));
  enable_params.Set("patterns", std::move(patterns));
  SendCommand("Fetch.enable", std::move(enable_params), true);

  const GURL url(
      embedded_test_server()->GetURL("/set-cookie?cookieName=cookieValue"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  base::Value::Dict request_paused_result =
      WaitForNotification("Fetch.requestPaused", true);
  std::string* request_id = request_paused_result.FindString("requestId");

  // Checks that `Fetch.requestPaused` contains the response headers added by
  // the extension
  base::Value::List* response_headers =
      request_paused_result.FindListByDottedPath("responseHeaders");
  auto* header_name = response_headers->back().GetDict().FindString("name");
  ASSERT_TRUE(header_name);
  ASSERT_EQ(*header_name, "extensionHeaderName");
  auto* header_value = response_headers->back().GetDict().FindString("value");
  ASSERT_TRUE(header_value);
  ASSERT_EQ(*header_value, "extensionHeaderValue");

  // Response headers are replaced by new overrides
  base::Value::Dict params;
  params.Set("requestId", *request_id);
  base::Value::Dict header_1;
  header_1.Set("name", "firstName");
  header_1.Set("value", "firstValue");
  base::Value::Dict header_2;
  header_2.Set("name", "secondName");
  header_2.Set("value", "secondValue");
  base::Value::List headers;
  headers.Append(std::move(header_1));
  headers.Append(std::move(header_2));
  params.Set("responseHeaders", std::move(headers));
  params.Set("responseCode", 200);
  params.Set("body", "");
  SendCommand("Fetch.fulfillRequest", std::move(params), false);

  // Check that `Network.responseReceived` contains the response headers as
  // specified via `Fetch.fulfillRequest`
  base::Value::Dict response_received_result =
      WaitForNotification("Network.responseReceived", false);
  auto* first_header =
      response_received_result.FindByDottedPath("response.headers.firstName");
  ASSERT_TRUE(first_header);
  ASSERT_EQ(*first_header, "firstValue");
  auto* second_header =
      response_received_result.FindByDottedPath("response.headers.secondName");
  ASSERT_TRUE(second_header);
  ASSERT_EQ(*second_header, "secondValue");
  ASSERT_EQ(response_received_result.FindByDottedPath("response.headers")
                ->GetDict()
                .size(),
            2u);

  // Check that the cookie as specified in the original headers has been set
  auto* get_all_cookies_result =
      SendCommand("Network.getAllCookies", base::Value::Dict(), true);
  const base::Value::List* cookies =
      get_all_cookies_result->FindList("cookies");
  ASSERT_TRUE(cookies);
  ASSERT_EQ(cookies->size(), 1u);
  auto* cookie_name = cookies->front().GetDict().FindString("name");
  ASSERT_TRUE(cookie_name);
  ASSERT_EQ(*cookie_name, "cookieName");
  auto* cookie_value = cookies->front().GetDict().FindString("value");
  ASSERT_TRUE(cookie_value);
  ASSERT_EQ(*cookie_value, "cookieValue");
}

// TODO(crbug.com/40168662) The test is flaky on multiple bots.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, DISABLED_WebRequestTypes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_types")) << message_;
}

// Test that a request to an OpenSearch description document (OSDD) generates
// an event with the expected details.
// Flaky on Windows and Mac: https://crbug.com/1218893
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_WebRequestTestOSDD DISABLED_WebRequestTestOSDD
#else
#define MAYBE_WebRequestTestOSDD WebRequestTestOSDD
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_WebRequestTestOSDD) {
  // An OSDD request is only generated when a main frame at is loaded at /, so
  // serve osdd/index.html from the root of the test server:
  embedded_test_server()->ServeFilesFromDirectory(
      test_data_dir_.AppendASCII("webrequest/osdd"));
  ASSERT_TRUE(StartEmbeddedTestServer());

  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(RunExtensionTest("webrequest/test_osdd")) << message_;
}

// Test that the webRequest events are dispatched with the expected details when
// a frame or tab is removed while a response is being received.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestUnloadAfterRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_unload.html?1"}))
      << message_;
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_unload.html?2"}))
      << message_;
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_unload.html?3"}))
      << message_;
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_unload.html?4"}))
      << message_;
}

// Test that the webRequest events are dispatched with the expected details when
// a frame or tab is immediately removed after starting a request.
// Flaky on all platforms. See crbug.com/780369 for detail.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestUnloadImmediately) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_unload.html?5"}))
      << message_;
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_unload.html?6"}))
      << message_;
}

enum class ProfileMode {
  kUserProfile,
  kIncognito,
};

struct ARTestParams {
  ProfileMode profile_mode;
  ContextType context_type;
};

class ExtensionWebRequestApiAuthRequiredTest
    : public ExtensionWebRequestApiTest,
      public testing::WithParamInterface<ARTestParams> {
 public:
  ExtensionWebRequestApiAuthRequiredTest()
      : ExtensionWebRequestApiTest(GetParam().context_type) {}
  ~ExtensionWebRequestApiAuthRequiredTest() override = default;
  ExtensionWebRequestApiAuthRequiredTest(
      const ExtensionWebRequestApiAuthRequiredTest&) = delete;
  ExtensionWebRequestApiAuthRequiredTest& operator=(
      ExtensionWebRequestApiAuthRequiredTest&) = delete;

 protected:
  static bool GetEnableIncognito() {
    return GetParam().profile_mode == ProfileMode::kIncognito;
  }

  static std::string FormatCustomArg(const char* test_name) {
    static constexpr char custom_arg_format[] =
        R"({"testName": "%s", "runInIncognito": %s})";

    return base::StringPrintf(custom_arg_format, test_name,
                              GetEnableIncognito() ? "true" : "false");
  }
};

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiAuthRequiredTest,
                       WebRequestAuthRequired) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // If running in incognito, create an incognito browser so the test
  // framework can create an incognito window.
  const bool incognito = GetEnableIncognito();
  if (incognito)
    CreateIncognitoBrowser(profile());

  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required",
      {.custom_arg = FormatCustomArg("authRequiredNonBlocking").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required",
      {.custom_arg = FormatCustomArg("authRequiredSyncNoAction").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required",
      {.custom_arg = FormatCustomArg("authRequiredSyncCancelAuth").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required",
      {.custom_arg = FormatCustomArg("authRequiredSyncSetAuth").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiAuthRequiredTest,
                       WebRequestAuthRequiredAsync) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // If running in incognito, create an incognito browser so the tests
  // run in an incognito window.
  const bool incognito = GetEnableIncognito();
  if (incognito)
    CreateIncognitoBrowser(profile());

  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required_async",
      {.custom_arg = FormatCustomArg("authRequiredAsyncNoAction").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required_async",
      {.custom_arg = FormatCustomArg("authRequiredAsyncCancelAuth").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
  ASSERT_TRUE(RunExtensionTest(
      "webrequest/test_auth_required_async",
      {.custom_arg = FormatCustomArg("authRequiredAsyncSetAuth").c_str()},
      {.allow_in_incognito = incognito}))
      << message_;
}

// This is flaky on wide variety of platforms (beyond that tracked previously in
// https://crbug.com/998369). See https://crbug.com/1026001.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiAuthRequiredTest,
                       DISABLED_WebRequestAuthRequiredParallel) {
  const bool incognito = GetEnableIncognito();
  if (incognito)
    CreateIncognitoBrowser(profile());

  const char* const custom_arg = incognito ? R"({"runInIncognito": true})"
                                           : R"({"runInIncognito": false})";
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_auth_required_parallel",
                               {.custom_arg = custom_arg},
                               {.allow_in_incognito = incognito}))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ExtensionWebRequestApiAuthRequiredTest,
    ::testing::Values(ARTestParams(ProfileMode::kUserProfile,
                                   ContextType::kPersistentBackground)));
INSTANTIATE_TEST_SUITE_P(
    PersistentBackgroundIncognito,
    ExtensionWebRequestApiAuthRequiredTest,
    ::testing::Values(ARTestParams(ProfileMode::kIncognito,
                                   ContextType::kPersistentBackground)));

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionWebRequestApiAuthRequiredTest,
    ::testing::Values(ARTestParams(ProfileMode::kUserProfile,
                                   ContextType::kServiceWorkerMV2)));
INSTANTIATE_TEST_SUITE_P(
    ServiceWorkerIncognito,
    ExtensionWebRequestApiAuthRequiredTest,
    ::testing::Values(ARTestParams(ProfileMode::kIncognito,
                                   ContextType::kServiceWorkerMV2)));

struct AuthRequiredServiceWorkerTestParams {
  bool under_service_worker_control;
  ContextType context_type;
};

// OnAuthRequired tests for subresource and sub frame, under service worker
// control and not under service worker control.
class ExtensionWebRequestApiAuthRequiredTestVariousContext
    : public testing::WithParamInterface<AuthRequiredServiceWorkerTestParams>,
      public ExtensionApiTest {
 public:
  ExtensionWebRequestApiAuthRequiredTestVariousContext()
      : ExtensionApiTest(GetParam().context_type) {}
  ~ExtensionWebRequestApiAuthRequiredTestVariousContext() override = default;
  ExtensionWebRequestApiAuthRequiredTestVariousContext(
      const ExtensionWebRequestApiAuthRequiredTestVariousContext&) = delete;
  ExtensionWebRequestApiAuthRequiredTestVariousContext& operator=(
      const ExtensionWebRequestApiAuthRequiredTestVariousContext&) = delete;

  void InstallRequestOnAuthRequiredTypeReportingExtension() {
    TestExtensionDir test_dir;
    test_dir.WriteManifest(R"({
        "name": "Web Request onAuthRequired Type Reporting Extension",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");

    // Extension script that will send message about the request type of
    // onAuthRequired event, and cancel the request.
    static constexpr char kBackgroundScript[] = R"(
        console.log('extension running');
        chrome.webRequest.onAuthRequired.addListener(
            function(details, callback) {
              console.log('onAuthRequired fired for ' + details.type);
              chrome.test.sendMessage(details.type);
              return {cancel: true};
            },
            {urls: ['<all_urls>']},
            ['blocking']);
        chrome.test.sendMessage('ready');
      )";
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);

    ExtensionTestMessageListener listener("ready");
    ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  void RegisterServiceWorker() {
    GURL url =
        embedded_test_server()->GetURL("/workers/service_worker_setup.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ("ok", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "setup();"));
  }

  void RunCommonTestSetup() {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    InstallRequestOnAuthRequiredTypeReportingExtension();

    // Register a service worker for a variation of the test and not register it
    // for another variation, so that we can verify that onAuthRequired event is
    // fired regardless of whether the page is under service worker control.
    if (GetParam().under_service_worker_control) {
      RegisterServiceWorker();
    }

    // Navigate to the test page.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/workers/simple.html")));
  }

  // Ensures auth required event is received for subresource fetch.
  void RunAuthRequiredTestForSubResource() {
    RunCommonTestSetup();

    // Make a fetch from the test page for a resource that requires auth.
    ExtensionTestMessageListener listener("xmlhttprequest");
    static constexpr char kSubResourceUrl[] =
        "/auth-basic/auth_required_subresource?realm=auth_required_subresource";
    std::string fetch_url =
        embedded_test_server()->GetURL(kSubResourceUrl).spec();
    EXPECT_EQ(401, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                          "try_fetch_status('" + fetch_url + "');"));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  // Ensures auth required event is received for sub frame navigation.
  void RunAuthRequiredTestForSubFrame() {
    RunCommonTestSetup();

    // Add an iframe for a source that requires auth.
    ExtensionTestMessageListener listener("sub_frame");
    static constexpr char kSubFrameUrl[] =
        "/auth-basic/auth_required_subframe?realm=auth_required_subframe";
    std::string frame_url = embedded_test_server()->GetURL(kSubFrameUrl).spec();
    static constexpr char kAddIframeScript[] = R"(
        const el = document.createElement('iframe');
        el.src = $1;
        document.body.appendChild(el);
      )";
    content::EvalJsResult result =
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
               content::JsReplace(kAddIframeScript, frame_url));
    ASSERT_THAT(result, content::EvalJsResult::IsOk());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackgroundWithServiceWorker,
                         ExtensionWebRequestApiAuthRequiredTestVariousContext,
                         ::testing::Values(AuthRequiredServiceWorkerTestParams(
                             true,
                             ContextType::kPersistentBackground)));
INSTANTIATE_TEST_SUITE_P(PersistentBackgroundWithoutServiceWorker,
                         ExtensionWebRequestApiAuthRequiredTestVariousContext,
                         ::testing::Values(AuthRequiredServiceWorkerTestParams(
                             false,
                             ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(ServiceWorkerExtensionWithServiceWorker,
                         ExtensionWebRequestApiAuthRequiredTestVariousContext,
                         ::testing::Values(AuthRequiredServiceWorkerTestParams(
                             true,
                             ContextType::kServiceWorkerMV2)));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerExtensionWithoutServiceWorker,
                         ExtensionWebRequestApiAuthRequiredTestVariousContext,
                         ::testing::Values(AuthRequiredServiceWorkerTestParams(
                             false,
                             ContextType::kServiceWorkerMV2)));

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiAuthRequiredTestVariousContext,
                       SubFrame) {
  RunAuthRequiredTestForSubFrame();
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiAuthRequiredTestVariousContext,
                       SubResource) {
  RunAuthRequiredTestForSubResource();
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestBlocking) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_blocking",
                               {.custom_arg = R"({"testSuite": "normal"})"}))
      << message_;
}

// This test times out regularly on win_rel trybots. See http://crbug.com/122178
// Also on Linux/ChromiumOS debug, ASAN and MSAN builds.
// https://crbug.com/670415
// Slower and flaky tests should be isolated in the "slow" group of tests in
// the JS file. This prevents losing test coverage for those tests that are
// not causing timeouts and flakes.
// TODO(crbug.com/40916455): Investigate the flakiness across all
// platforms and re-enable.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_WebRequestBlockingSlow) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_blocking",
                               {.custom_arg = R"({"testSuite": "slow"})"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestBlockingSetCookieHeader) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_blocking_cookie")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestExtraHeaders) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_extra_headers")) << message_;
}

// Flaky on all platforms: https://crbug.com/1003661
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_WebRequestExtraHeaders_Auth) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_extra_headers_auth"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestChangeCSPHeaders) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_change_csp_headers"))
      << message_;
}

// TODO: crbug.com/1450976 - Re-enable tests on Mac and CrOS.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_WebRequestCORSWithExtraHeaders \
  DISABLED_WebRequestCORSWithExtraHeaders
#else
#define MAYBE_WebRequestCORSWithExtraHeaders WebRequestCORSWithExtraHeaders
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_WebRequestCORSWithExtraHeaders) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_cors")) << message_;
}

#if defined(ADDRESS_SANITIZER)
#define MAYBE_WebRequestRedirects DISABLED_WebRequestRedirects
#else
#define MAYBE_WebRequestRedirects WebRequestRedirects
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_WebRequestRedirects) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_redirects")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestRedirectsWithExtraHeaders) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_redirects",
                               {.custom_arg = R"({"useExtraHeaders": true})"}))
      << message_;
}

// Tests that redirects from secure to insecure don't send the referrer header.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestRedirectsToInsecure) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL insecure_destination =
      embedded_test_server()->GetURL("/extensions/test_file.html");
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(https_test_server.Start());

  GURL url = https_test_server.GetURL("/webrequest/simulate_click.html");

  base::Value::List custom_args;
  custom_args.Append(url.spec());
  custom_args.Append(insecure_destination.spec());

  std::string config_string;
  base::JSONWriter::Write(custom_args, &config_string);
  ASSERT_TRUE(RunExtensionTest("webrequest/test_redirects_from_secure",
                               {.custom_arg = config_string.c_str()}))
      << message_;
}

// Tests redirects around workers. To test service workers, the HTTPS test
// server is used.
// TODO(crbug.com/40255652): test is flaky on linux-chromeos-rel.
// TODO(crbug.com/40259518): test is flaky on Mac10.14.
// TODO(crbug.com/40282182): test is flaky on linux tests.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebRequestRedirectsWorkers DISABLED_WebRequestRedirectsWorkers
#else
#define MAYBE_WebRequestRedirectsWorkers WebRequestRedirectsWorkers
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_WebRequestRedirectsWorkers) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(https_test_server.Start());

  GURL base_url =
      https_test_server.GetURL("/webrequest/test_redirects_workers/page/");
  base::Value::Dict custom_args;
  custom_args.Set("base_url", base_url.spec());
  std::string config_string;
  base::JSONWriter::Write(custom_args, &config_string);

  ASSERT_TRUE(RunExtensionTest("webrequest/test_redirects_workers",
                               {.custom_arg = config_string.c_str()}))
      << message_;
}

// TODO(crbug.com/40916455): test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_WebRequestSubresourceRedirects) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_subresource_redirects"))
      << message_;
}

// TODO(crbug.com/40916455): test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_WebRequestSubresourceRedirectsWithExtraHeaders) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_subresource_redirects",
                               {.custom_arg = R"({"useExtraHeaders": true})"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestNewTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webrequest/test_new_tab")) << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ResultCatcher catcher;

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const Extension* extension =
      registry->enabled_extensions().GetByID(last_loaded_extension_id());
  GURL url = extension->GetResourceURL("newTab/a.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // There's a link on a.html with target=_blank. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDeclarative1) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_declarative",
                               {.custom_arg = R"({"testSuite": "normal1"})"}))
      << message_;
}

// This test fixture runs all of the broken and flaky tests. It's disabled
// until these tests are fixed and moved to the set of tests that aren't
// broken or flaky. Should tests become flaky, they can be moved here.
// See https://crbug.com/846555.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_WebRequestDeclarative1Broken) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_declarative",
                               {.custom_arg = R"({"testSuite": "broken"})"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDeclarative2) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest/test_declarative",
                               {.custom_arg = R"({"testSuite": "normal2"})"}))
      << message_;
}

void ExtensionWebRequestApiTest::RunPermissionTest(
    const char* extension_directory,
    bool load_extension_with_incognito_permission,
    bool wait_for_extension_loaded_in_incognito,
    const char* expected_content_regular_window,
    const char* exptected_content_incognito_window,
    ContextType context_type) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  ExtensionTestMessageListener listener("done");
  ExtensionTestMessageListener listener_incognito("done_incognito");

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("webrequest_permissions")
          .AppendASCII(extension_directory),
      {.allow_in_incognito = load_extension_with_incognito_permission,
       .context_type = context_type}));

  // Test that navigation in regular window is properly redirected.
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // This navigation should be redirected.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_content_regular_window,
            content::EvalJs(tab, "document.body.textContent"));

  // Test that navigation in OTR window is properly redirected.
  Browser* otr_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  if (wait_for_extension_loaded_in_incognito)
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // This navigation should be redirected if
  // load_extension_with_incognito_permission is true.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      otr_browser,
      embedded_test_server()->GetURL("/extensions/test_file.html")));

  WebContents* otr_tab = otr_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(exptected_content_incognito_window,
            content::EvalJs(otr_tab, "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDeclarativePermissionSpanning1) {
  // Test spanning with incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("spanning", true, false, "redirected1", "redirected1",
                    GetContextType());
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDeclarativePermissionSpanning2) {
  // Test spanning without incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("spanning", false, false, "redirected1", "",
                    GetContextType());
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDeclarativePermissionSplit1) {
  // Test split with incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("split", true, true, "redirected1", "redirected2",
                    GetContextType());
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDeclarativePermissionSplit2) {
  // Test split without incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("split", false, false, "redirected1", "", GetContextType());
}

// TODO(crbug.com/41010858): Cure these flaky tests.
// TODO(crbug.com/40734863): Bulk-disabled as part of mac arm64 bot greening
// TODO(crbug.com/40773828): Further disabled due to ongoing flakiness.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, DISABLED_PostData1) {
  // Test HTML form POST data access with the default and "url" encoding.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_post1.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, DISABLED_PostData2) {
  // Test HTML form POST data access with the multipart and plaintext encoding.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_post2.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DeclarativeSendMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest_sendmessage")) << message_;
}

// Check that reloading an extension that runs in incognito split mode and
// has two active background pages with registered events does not crash the
// browser. Regression test for http://crbug.com/224094
// Flaky on linux-lacros and Linux. See http://crbug.com/1423252
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_IncognitoSplitModeReload DISABLED_IncognitoSplitModeReload
#else
#define MAYBE_IncognitoSplitModeReload IncognitoSplitModeReload
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_IncognitoSplitModeReload) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Wait for rules to be set up.
  ExtensionTestMessageListener listener("done");
  ExtensionTestMessageListener listener_incognito("done_incognito");

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_reload"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // Reload extension and wait for rules to be set up again. This should not
  // crash the browser.
  ExtensionTestMessageListener listener2("done");
  ExtensionTestMessageListener listener_incognito2("done_incognito");

  ReloadExtension(extension->id());

  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       ExtensionRequests) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ExtensionTestMessageListener listener_main1("web_request_status1",
                                              ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_main2("web_request_status2",
                                              ReplyBehavior::kWillReply);

  ExtensionTestMessageListener listener_app("app_done");
  ExtensionTestMessageListener listener_extension("extension_done");

  // Set up webRequest listener
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/main")));
  EXPECT_TRUE(listener_main1.WaitUntilSatisfied());
  EXPECT_TRUE(listener_main2.WaitUntilSatisfied());

  // Perform some network activity in an app and another extension.
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("webrequest_extensions/app"),
                    {.context_type = ContextType::kFromManifest}));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("webrequest_extensions/extension"),
      {.context_type = ContextType::kFromManifest}));

  EXPECT_TRUE(listener_app.WaitUntilSatisfied());
  EXPECT_TRUE(listener_extension.WaitUntilSatisfied());

  // Load a page, a content script from "webrequest_extensions/extension" will
  // ping us when it is ready.
  ExtensionTestMessageListener listener_pageready("contentscript_ready",
                                                  ReplyBehavior::kWillReply);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/test_file.html?match_webrequest_test")));
  EXPECT_TRUE(listener_pageready.WaitUntilSatisfied());

  // The extension and app-generated requests should not have triggered any
  // webRequest event filtered by type 'xmlhttprequest'.
  // (check this here instead of before the navigation, in case the webRequest
  // event routing is slow for some reason).
  ExtensionTestMessageListener listener_result;
  listener_main1.Reply("");
  EXPECT_TRUE(listener_result.WaitUntilSatisfied());
  EXPECT_EQ("Did not intercept any requests.", listener_result.message());

  ExtensionTestMessageListener listener_contentscript("contentscript_done");
  ExtensionTestMessageListener listener_framescript("framescript_done");

  // Proceed with the final tests: Let the content script fire a request and
  // then load an iframe which also fires a XHR request.
  listener_pageready.Reply("");
  EXPECT_TRUE(listener_contentscript.WaitUntilSatisfied());
  EXPECT_TRUE(listener_framescript.WaitUntilSatisfied());

  // Collect the visited URLs. The content script and subframe does not run in
  // the extension's process, so the requests should be visible to the main
  // extension.
  listener_result.Reset();
  listener_main2.Reply("");
  EXPECT_TRUE(listener_result.WaitUntilSatisfied());

  // The extension frame does run in the extension's process. Any requests made
  // by it should not be visible to other extensions, since they won't have
  // access to the request initiator.
  //
  // OTOH, the content script executes fetches/XHRs as-if they were initiated by
  // the webpage that the content script got injected into.  Here, the webpage
  // has origin of http://127.0.0.1:<some port>, and so the webRequest API
  // extension should have access to the request.
  EXPECT_EQ("Intercepted requests: ?contentscript", listener_result.message());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, HostedAppRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL hosted_app_url(
      embedded_test_server()->GetURL(
          "/extensions/api_test/webrequest_hosted_app/index.html"));
  scoped_refptr<const Extension> hosted_app =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "Some hosted app")
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Set("app",
                       base::Value::Dict().Set(
                           "launch", base::Value::Dict().Set(
                                         "web_url", hosted_app_url.spec()))))
          .Build();
  extension_service()->AddExtension(hosted_app.get());

  ExtensionTestMessageListener listener1("main_frame");
  ExtensionTestMessageListener listener2("xmlhttprequest");

  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_hosted_app")));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), hosted_app_url));

  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
}

// Tests that WebRequest works with runtime host permissions.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestWithWithheldPermissions) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Navigate the browser to a page in a new tab. The page at "a.com" has two
  // two cross-origin iframes to "b.com" and "c.com".
  const std::string kHost = "a.com";
  GURL url = embedded_test_server()->GetURL(kHost, "/iframe_cross_site.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  int port = embedded_test_server()->port();
  const std::string kXhrPath = "simple.html";

  // The extension shouldn't have currently received any webRequest events,
  // since it doesn't have any permissions.
  {
    EXPECT_EQ(0, GetWebRequestCountFromBackgroundScript(extension, profile()));

    content::RenderFrameHostWrapper main_frame(
        web_contents->GetPrimaryMainFrame());
    content::RenderFrameHostWrapper child_frame(
        ChildFrameAt(main_frame.get(), 0));
    ASSERT_TRUE(child_frame);
    const std::string kChildHost = child_frame->GetLastCommittedURL().host();

    // The extension shouldn't be able to intercept the xhr requests since it
    // doesn't have any permissions.
    PerformXhrInFrame(main_frame.get(), kHost, port, kXhrPath);
    PerformXhrInFrame(child_frame.get(), kChildHost, port, kXhrPath);
    EXPECT_EQ(0, GetWebRequestCountFromBackgroundScript(extension, profile()));
    EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST,
              runner->GetBlockedActions(extension->id()));

    // Grant activeTab permission.
    runner->accept_bubble_for_testing(true);
    runner->RunAction(extension, true);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  }

  // The runner will have refreshed the page, and the extension will have
  // received access to the main-frame ("a.com"). It should still not be able to
  // intercept the cross-origin sub-frame requests to "b.com" and "c.com".
  content::RenderFrameHostWrapper main_frame(
      web_contents->GetPrimaryMainFrame());
  content::RenderFrameHostWrapper child_frame(
      ChildFrameAt(main_frame.get(), 0));
  const std::string kChildHost = child_frame->GetLastCommittedURL().host();

  ASSERT_TRUE(child_frame);
  EXPECT_TRUE(
      HasSeenWebRequestInBackgroundScript(extension, profile(), "a.com"));
  EXPECT_FALSE(
      HasSeenWebRequestInBackgroundScript(extension, profile(), "b.com"));
  EXPECT_FALSE(
      HasSeenWebRequestInBackgroundScript(extension, profile(), "c.com"));

  // The withheld sub-frame requests should not show up as a blocked action.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension->id()));

  int request_count =
      GetWebRequestCountFromBackgroundScript(extension, profile());

  // ... and the extension should receive future events.
  PerformXhrInFrame(main_frame.get(), kHost, port, kXhrPath);
  ++request_count;
  EXPECT_EQ(request_count,
            GetWebRequestCountFromBackgroundScript(extension, profile()));

  // However, activeTab only grants access to the main frame, not to child
  // frames. As such, trying to XHR in the child frame should still fail.
  PerformXhrInFrame(child_frame.get(), kChildHost, port, kXhrPath);
  EXPECT_EQ(request_count,
            GetWebRequestCountFromBackgroundScript(extension, profile()));
  // But since there's no way for the user to currently grant access to child
  // frames, this shouldn't show up as a blocked action.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension->id()));

  // Revoke the extension's tab permissions.
  ActiveTabPermissionGranter* granter =
      TabHelper::FromWebContents(web_contents)->active_tab_permission_granter();
  ASSERT_TRUE(granter);
  granter->RevokeForTesting();
  base::RunLoop().RunUntilIdle();

  // The extension should no longer receive webRequest events since they are
  // withheld. The extension icon should get updated to show the wants-to-run
  // badge UI.
  TestExtensionActionAPIObserver action_updated_waiter(profile(),
                                                       extension->id());
  PerformXhrInFrame(main_frame.get(), kHost, port, kXhrPath);
  action_updated_waiter.Wait();
  EXPECT_EQ(web_contents, action_updated_waiter.last_web_contents());

  EXPECT_EQ(request_count,
            GetWebRequestCountFromBackgroundScript(extension, profile()));
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST,
            runner->GetBlockedActions(extension->id()));
}

// Test that extensions with granted runtime host permissions to a tab can
// intercept cross-origin requests from that tab.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestWithheldPermissionsCrossOriginRequests) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/extensions/cross_site_script.html")));

  const std::string kCrossSiteHost("b.com");
  EXPECT_FALSE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                   kCrossSiteHost));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST,
            runner->GetBlockedActions(extension->id()));

  // Grant runtime host permission to the page. The page should refresh. Even
  // though the request is for b.com (and the extension only has access to
  // a.com), it should still see the request. This is necessary for extensions
  // with webRequest to work with runtime host permissions.
  // https://crbug.com/851722.
  runner->accept_bubble_for_testing(true);
  runner->RunAction(extension, true /* grant tab permissions */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension->id()));

  EXPECT_TRUE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                  kCrossSiteHost));
}

// Tests behavior when an extension has withheld access to a request's URL, but
// not the initiator's (tab's) URL. Regression test for
// https://crbug.com/891586.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WithheldHostPermissionsForCrossOriginWithoutInitiator) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // TODO(devlin): This is essentially copied from the webrequest_activetab
  // API test extension, but has different permissions. Maybe it's worth having
  // all tests use a common pattern?
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Web Request Withheld Hosts",
           "manifest_version": 2,
           "version": "0.1",
           "background": { "scripts": ["background.js"], "persistent": true },
           "permissions": ["*://b.com:*/*", "webRequest"]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     R"(self.webRequestCount = 0;
         self.requestedHostnames = [];

         chrome.webRequest.onBeforeRequest.addListener(function(details) {
           ++self.webRequestCount;
           self.requestedHostnames.push((new URL(details.url)).hostname);
         }, {urls:['<all_urls>']});
         chrome.test.sendMessage('ready');)");

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to example.com, which has a cross-site script to b.com.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/extensions/cross_site_script.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  // Even though the extension has access to b.com, it shouldn't show that it
  // wants to run, because example.com is not a requested host.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension->id()));
  EXPECT_FALSE(
      HasSeenWebRequestInBackgroundScript(extension, profile(), "b.com"));

  // Navigating to b.com (so that the script is hosted on the same origin as
  // the WebContents) should show the extension wants to run.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "b.com", "/extensions/cross_site_script.html")));
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST,
            runner->GetBlockedActions(extension->id()));
}

// Verify that requests to clientsX.google.com are protected properly.
// First test requests from a standard renderer and then a request from the
// browser process.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestClientsGoogleComProtection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrequest_clients_google_com"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  auto get_clients_google_request_count = [this, extension]() {
    return GetCountFromBackgroundScript(extension, profile(),
                                        "self.clientsGoogleWebRequestCount");
  };
  auto get_yahoo_request_count = [this, extension]() {
    return GetCountFromBackgroundScript(extension, profile(),
                                        "self.yahooWebRequestCount");
  };

  EXPECT_EQ(0, get_clients_google_request_count());
  EXPECT_EQ(0, get_yahoo_request_count());

  GURL main_frame_url =
      embedded_test_server()->GetURL("www.example.com", "/simple.html");
  NavigateParams params(browser(), main_frame_url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_EQ(0, get_clients_google_request_count());
  EXPECT_EQ(0, get_yahoo_request_count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Attempt to issue a request to clients1.google.com from the renderer. This
  // will fail, but should still be visible to the WebRequest API.
  const char kRequest[] = R"(
      var xhr = new XMLHttpRequest();
      xhr.open('GET', 'http://clients1.google.com');
      new Promise(resolve => {
        xhr.onload = () => {resolve(true);};
        xhr.onerror = () => {resolve(false);};
        xhr.send();
      });
      )";
  EXPECT_EQ(false, EvalJs(web_contents->GetPrimaryMainFrame(), kRequest));
  // Requests always fail due to cross origin nature.

  EXPECT_EQ(1, get_clients_google_request_count());
  EXPECT_EQ(0, get_yahoo_request_count());

  auto make_browser_request = [this](const GURL& url) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->destination = network::mojom::RequestDestination::kEmpty;
    request->resource_type =
        static_cast<int>(blink::mojom::ResourceType::kSubResource);

    auto* url_loader_factory = profile()
                                   ->GetDefaultStoragePartition()
                                   ->GetURLLoaderFactoryForBrowserProcess()
                                   .get();
    content::SimpleURLLoaderTestHelper loader_helper;
    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory, loader_helper.GetCallbackDeprecated());

    // Wait for the response to complete.
    loader_helper.WaitForCallback();
    EXPECT_TRUE(loader_helper.response_body());
    EXPECT_EQ(200, loader->ResponseInfo()->headers->response_code());
  };

  // Now perform a request to "client1.google.com" from the browser process.
  // This should *not* be visible to the WebRequest API. We should still have
  // only seen the single render-initiated request from the first half of the
  // test.
  make_browser_request(
      embedded_test_server()->GetURL("clients1.google.com", "/simple.html"));
  EXPECT_EQ(1, get_clients_google_request_count());

  // Other non-navigation browser requests should also be hidden from
  // extensions.
  make_browser_request(
      embedded_test_server()->GetURL("yahoo.com", "/simple.html"));
  EXPECT_EQ(0, get_yahoo_request_count());
}

// Verify that requests for PAC scripts are protected properly.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestPacRequestProtection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_pac_request"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Configure a PAC script. Need to do this after the extension is loaded, so
  // that the PAC isn't already loaded by the time the extension starts
  // affecting requests.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetDict(proxy_config::prefs::kProxy,
                        ProxyConfigDictionary::CreatePacScript(
                            embedded_test_server()->GetURL("/self.pac").spec(),
                            true /* pac_mandatory */));
  // Flush the proxy configuration change over the Mojo pipe to avoid any races.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  // Navigate to a page. The URL doesn't matter.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve.test/title2.html")));

  // The extension should not have seen the PAC request.
  EXPECT_EQ(0, GetCountFromBackgroundScript(extension, profile(),
                                            "self.pacRequestCount"));

  // The extension should have seen the request for the main frame.
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.title2RequestCount"));

  // The PAC request should have succeeded, as should the subsequent URL
  // request.
  EXPECT_EQ(content::PAGE_TYPE_NORMAL, browser()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetController()
                                           .GetLastCommittedEntry()
                                           ->GetPageType());
}

// Checks that the Dice response header is protected for Gaia URLs, but not
// other URLs.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestDiceHeaderProtection) {
  // Load an extension that registers a listener for webRequest events, and
  // wait until it is initialized.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_dice_header"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ASSERT_TRUE(embedded_test_server()->Start());

  // Setup a web contents observer to inspect the response headers after the
  // extension was run.
  class TestWebContentsObserver : public content::WebContentsObserver {
   public:
    explicit TestWebContentsObserver(content::WebContents* contents)
        : WebContentsObserver(contents) {}

    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override {
      // Check that the extension cannot add a Dice header.
      const net::HttpResponseHeaders* headers =
          navigation_handle->GetResponseHeaders();
      EXPECT_TRUE(headers->GetNormalizedHeader(
          "X-Chrome-ID-Consistency-Response", &dice_header_value_));
      EXPECT_TRUE(
          headers->GetNormalizedHeader("X-New-Header", &new_header_value_));
      EXPECT_TRUE(
          headers->GetNormalizedHeader("X-Control", &control_header_value_));
      did_finish_navigation_called_ = true;
    }

    bool did_finish_navigation_called() const {
      return did_finish_navigation_called_;
    }

    const std::string& dice_header_value() const { return dice_header_value_; }

    const std::string& new_header_value() const { return new_header_value_; }

    const std::string& control_header_value() const {
      return control_header_value_;
    }

    void Clear() {
      did_finish_navigation_called_ = false;
      dice_header_value_.clear();
      new_header_value_.clear();
      control_header_value_.clear();
    }

   private:
    bool did_finish_navigation_called_ = false;
    std::string dice_header_value_;
    std::string new_header_value_;
    std::string control_header_value_;
  };

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestWebContentsObserver test_webcontents_observer(web_contents);

  // Navigate to the Gaia URL intercepted by the extension.
  GURL url =
      embedded_test_server()->GetURL("gaia.com", "/extensions/dice.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that the Dice header was not changed by the extension.
  EXPECT_TRUE(test_webcontents_observer.did_finish_navigation_called());
  EXPECT_EQ(kHeaderValueFromServer,
            test_webcontents_observer.dice_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.new_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.control_header_value());

  // Check that the Dice header cannot be read by the extension.
  EXPECT_EQ(0, GetCountFromBackgroundScript(extension, profile(),
                                            "self.diceResponseHeaderCount"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.controlResponseHeaderCount"));

  // Navigate to a non-Gaia URL intercepted by the extension.
  test_webcontents_observer.Clear();
  url = embedded_test_server()->GetURL("example.com", "/extensions/dice.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that the Dice header was changed by the extension.
  EXPECT_TRUE(test_webcontents_observer.did_finish_navigation_called());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.dice_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.new_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.control_header_value());

  // Check that the Dice header can be read by the extension.
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.diceResponseHeaderCount"));
  EXPECT_EQ(2, GetCountFromBackgroundScript(extension, profile(),
                                            "self.controlResponseHeaderCount"));
}

// Test that the webRequest events are dispatched for the WebSocket handshake
// requests.
// TODO(crbug.com/40715657): Test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, DISABLED_WebSocketRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_websocket.html"}))
      << message_;
}

// Test that the webRequest events are dispatched for the WebSocket handshake
// requests when authenrication is requested by server.
// TODO(crbug.com/40168662) Re-enable test
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebSocketRequestAuthRequired) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory(), true));
  ASSERT_TRUE(RunExtensionTest("webrequest",
                               {.extension_url = "test_websocket_auth.html"}))
      << message_;
}

// Test that the webRequest events are dispatched for the WebSocket handshake
// requests.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebSocketRequestOnWorker) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(RunExtensionTest("webrequest",
                               {.extension_url = "test_websocket_worker.html"}))
      << message_;
}

// Tests that a clean close from the server is not reported as an error when
// there is a race between OnDropChannel and SendFrame.
// Regression test for https://crbug.com/937790.
//
// TODO(b:332825952): Flaky on linux-chromeos-dbg
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_WebSocketCleanClose DISABLED_WebSocketCleanClose
#else
#define MAYBE_WebSocketCleanClose WebSocketCleanClose
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebSocketCleanClose) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(RunExtensionTest(
      "webrequest", {.extension_url = "test_websocket_clean_close.html"}))
      << message_;
}

class ExtensionWebRequestApiWebTransportTest
    : public ExtensionWebRequestApiTest {
 public:
  ExtensionWebRequestApiWebTransportTest() { server_.Start(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionWebRequestApiTest::SetUpCommandLine(command_line);
    server_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionWebRequestApiTest::SetUpOnMainThread();
    ASSERT_TRUE(StartEmbeddedTestServer());
    GetTestConfig()->Set("testWebTransportPort",
                         server_.server_address().port());
  }

 protected:
  bool RunTest(const char* page_url) {
    return RunExtensionTest("webrequest", {.extension_url = page_url});
  }

  content::WebTransportSimpleTestServer server_;
};

// Test that the webRequest events are dispatched for the WebTransport
// handshake.
// TODO(crbug.com/326122304): Re-enable this test
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiWebTransportTest, DISABLED_Main) {
  ASSERT_TRUE(RunTest("test_webtransport.html")) << message_;
}

// Test that the webRequest events are dispatched for the WebTransport
// handshake in a dedicated worker.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiWebTransportTest,
                       DedicaterWorker) {
  ASSERT_TRUE(RunTest("test_webtransport_dedicated_worker.html")) << message_;
}

// Test that the webRequest events are dispatched for the WebTransport
// handshake in a shared worker.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiWebTransportTest, SharedWorker) {
  ASSERT_TRUE(RunTest("test_webtransport_shared_worker.html")) << message_;
}

// Test that the webRequest events are dispatched for the WebTransport
// handshake in a service worker.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiWebTransportTest, ServiceWorker) {
  ASSERT_TRUE(RunTest("test_webtransport_service_worker.html")) << message_;
}

// Test behavior when intercepting requests from a browser-initiated url fetch.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       WebRequestURLLoaderInterception) {
  // Create an extension that intercepts (and blocks) requests to example.com.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "web_request_browser_interception",
           "description": "tests that browser requests aren't intercepted",
           "version": "0.1",
           "permissions": ["webRequest", "webRequestBlocking", "*://*/*"],
           "manifest_version": 2,
           "background": { "scripts": ["background.js"], "persistent": true }
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     R"(chrome.webRequest.onBeforeRequest.addListener(
             function(details) {
               return {cancel: details.url.indexOf('example.com') != -1};
             },
             {urls: ["<all_urls>"]},
             ["blocking"]);
         chrome.test.sendMessage('ready');)");

  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  // Taken from test/data/extensions/body1.html.
  const char kGoogleBodyContent[] = "dog";
  const char kGoogleFullContent[] = "<html>\n<body>dog</body>\n</html>\n";

  // Taken from test/data/extensions/body2.html.
  const char kExampleBodyContent[] = "cat";
  const char kExampleFullContent[] = "<html>\n<body>cat</body>\n</html>\n";

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        std::unique_ptr<net::test_server::BasicHttpResponse> response(
            new net::test_server::BasicHttpResponse);
        if (request.relative_url == "/extensions/body1.html") {
          response->set_code(net::HTTP_OK);
          response->set_content(kGoogleFullContent);
          return std::move(response);
        } else if (request.relative_url == "/extensions/body2.html") {
          response->set_code(net::HTTP_OK);
          response->set_content(kExampleFullContent);
          return std::move(response);
        }

        return nullptr;
      }));
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL google_url =
      embedded_test_server()->GetURL("google.com", "/extensions/body1.html");

  // First, check normal requests (e.g., navigations) to verify the extension
  // is working correctly.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  EXPECT_EQ(google_url, web_contents->GetLastCommittedURL());

  // google.com should succeed.
  EXPECT_EQ(kGoogleBodyContent,
            content::EvalJs(web_contents, "document.body.textContent.trim();"));

  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/extensions/body2.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_url));
  {
    // example.com should fail.
    content::NavigationEntry* nav_entry =
        web_contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(nav_entry);
    EXPECT_EQ(content::PAGE_TYPE_ERROR, nav_entry->GetPageType());
    EXPECT_NE(
        kExampleBodyContent,
        content::EvalJs(web_contents, "document.body.textContent.trim();"));
  }

  // A callback allow waiting for responses to complete with an expected status
  // and given content.
  auto make_browser_request =
      [](network::mojom::URLLoaderFactory* url_loader_factory, const GURL& url,
         const std::optional<std::string>& expected_response,
         int expected_net_code) {
        auto request = std::make_unique<network::ResourceRequest>();
        request->url = url;

        content::SimpleURLLoaderTestHelper simple_loader_helper;
        auto simple_loader = network::SimpleURLLoader::Create(
            std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
        simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
            url_loader_factory, simple_loader_helper.GetCallbackDeprecated());

        simple_loader_helper.WaitForCallback();

        if (expected_response.has_value()) {
          EXPECT_TRUE(!!simple_loader_helper.response_body());
          EXPECT_EQ(*simple_loader_helper.response_body(), *expected_response);
          EXPECT_EQ(200,
                    simple_loader->ResponseInfo()->headers->response_code());
        } else {
          EXPECT_FALSE(!!simple_loader_helper.response_body());
          EXPECT_EQ(simple_loader->NetError(), expected_net_code);
        }
      };

  // Next, try a series of requests through URLRequestFetchers (rather than a
  // renderer).
  auto* url_loader_factory = profile()
                                 ->GetDefaultStoragePartition()
                                 ->GetURLLoaderFactoryForBrowserProcess()
                                 .get();

  {
    // google.com should be unaffected by the extension and should succeed.
    SCOPED_TRACE("google.com with Profile's url loader");
    make_browser_request(url_loader_factory, google_url, kGoogleFullContent,
                         net::OK);
  }

  {
    // example.com should also succeed since non-navigation browser-initiated
    // requests are hidden from extensions. See crbug.com/884932.
    SCOPED_TRACE("example.com with Profile's url loader");
    make_browser_request(url_loader_factory, example_url, kExampleFullContent,
                         net::OK);
  }

  // Requests going through the system network context manager should always
  // succeed.
  SystemNetworkContextManager* system_network_context_manager =
      g_browser_process->system_network_context_manager();
  network::mojom::URLLoaderFactory* system_url_loader_factory =
      system_network_context_manager->GetURLLoaderFactory();
  {
    // google.com should succeed (again).
    SCOPED_TRACE("google.com with System's network context manager");
    make_browser_request(system_url_loader_factory, google_url,
                         kGoogleFullContent, net::OK);
  }
  {
    // example.com should also succeed, since it's not through the profile's
    // request context.
    SCOPED_TRACE("example.com with System's network context manager");
    make_browser_request(system_url_loader_factory, example_url,
                         kExampleFullContent, net::OK);
  }
}

// Test that extensions need host permissions to both the request url and
// initiator to intercept a request.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       InitiatorAccessRequired) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrequest_permissions/initiator"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  struct TestCase {
    std::string navigate_before_start;
    std::string xhr_domain;
    std::string expected_initiator;
  } testcases[] = {{"example.com", "example.com", "example.com"},
                   {"example2.com", "example3.com", "example2.com"},
                   // No access to the initiator.
                   {"no-permission.com", "example4.com", ""},
                   // No access to the request url.
                   {"example.com", "no-permission.com", ""}};

  int port = embedded_test_server()->port();

  int expected_requests_intercepted_count = 0;
  for (const auto& testcase : testcases) {
    SCOPED_TRACE(testcase.navigate_before_start + ":" + testcase.xhr_domain +
                 ":" + testcase.expected_initiator);
    ExtensionTestMessageListener initiator_listener;
    initiator_listener.set_extension_id(extension->id());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(testcase.navigate_before_start,
                                       "/extensions/body1.html")));
    PerformXhrInFrame(web_contents->GetPrimaryMainFrame(), testcase.xhr_domain,
                      port, "extensions/api_test/webrequest/xhr/data.json");

    // Ensure that the extension wasn't able to intercept the request if it
    // didn't have permission to the initiator or the request url.
    if (!testcase.expected_initiator.empty())
      ++expected_requests_intercepted_count;

    // Run a script in the extensions background page to ensure that we have
    // received the initiator message from the extension.
    ASSERT_EQ(expected_requests_intercepted_count,
              GetCountFromBackgroundScript(extension, profile(),
                                           "self.requestsIntercepted"));

    if (testcase.expected_initiator.empty()) {
      EXPECT_FALSE(initiator_listener.was_satisfied());
    } else {
      ASSERT_TRUE(initiator_listener.was_satisfied());
      EXPECT_EQ("http://" + testcase.expected_initiator + ":" +
                    base::NumberToString(port),
                initiator_listener.message());
    }
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestApiClearsBindingOnFirstListener) {
  // Skip if the proxy is forced since the bindings will never be cleared in
  // that case.
  if (base::FeatureList::IsEnabled(
          extensions_features::kForceWebRequestProxyForTest)) {
    return;
  }

  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory(
      CreateURLLoaderFactory());
  bool has_connection_error = false;
  loader_factory.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { has_connection_error = true; }));

  InstallWebRequestExtension("extension1");
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  EXPECT_TRUE(has_connection_error);
  loader_factory.reset();

  // The second time there should be no connection error.
  loader_factory.Bind(CreateURLLoaderFactory());
  has_connection_error = false;
  loader_factory.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { has_connection_error = true; }));
  InstallWebRequestExtension("extension2");
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  EXPECT_FALSE(has_connection_error);
}

// Regression test for http://crbug.com/878366.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestApiDoesNotCrashOnErrorAfterProfileDestroyed) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a profile that will be destroyed later.
  base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::unique_ptr<Profile> temp_profile = Profile::CreateProfile(
      profile_manager->user_data_dir().AppendASCII("profile"), nullptr,
      Profile::CreateMode::kSynchronous);
  // Create a WebRequestAPI instance that we can control the lifetime of.
  auto api = std::make_unique<WebRequestAPI>(temp_profile.get());
  // Make sure we are proxying for |temp_profile|.
  api->ForceProxyForTesting();
  temp_profile->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();

  network::URLLoaderFactoryBuilder factory_builder;

  auto temp_web_contents =
      WebContents::Create(WebContents::CreateParams(temp_profile.get()));
  content::RenderFrameHost* frame = temp_web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(api->MaybeProxyURLLoaderFactory(
      frame->GetProcess()->GetBrowserContext(), frame,
      frame->GetProcess()->GetID(),
      content::ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      std::nullopt, ukm::kInvalidSourceIdObj, factory_builder, nullptr,
      nullptr));
  temp_web_contents.reset();
  auto params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = 0;
  mojo::Remote<network::mojom::URLLoaderFactory> factory(
      std::move(factory_builder)
          .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
              temp_profile->GetDefaultStoragePartition()->GetNetworkContext(),
              std::move(params)));

  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  network::ResourceRequest resource_request;
  resource_request.url = embedded_test_server()->GetURL("/hung");
  factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, resource_request,
      client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Destroy profile, unbind client to cause a connection error, and delete the
  // WebRequestAPI. This will cause the connection error that will reach the
  // proxy before the ProxySet shutdown code runs on the IO thread.
  api->Shutdown();

  // We are about to destroy a profile. In production that will only happen
  // as part of the destruction of BrowserProcess's ProfileManager. This
  // happens in PostMainMessageLoopRun(). This means that to have this test
  // represent production we have to make sure that no tasks are pending on the
  // main thread before we destroy the profile. We also would need to prohibit
  // the posting of new tasks on the main thread as in production the main
  // thread's message loop will not be accepting them. We fallback on flushing
  // the ThreadPool here to avoid the posts coming from it.
  content::RunAllTasksUntilIdle();

  ProfileDestroyer::DestroyOriginalProfileWhenAppropriate(
      std::move(temp_profile));
  client.Unbind();
  api.reset();
}

// Tests that webRequest API can inspect window.open() requests initiated from
// chrome-untrusted:// pages to Web origins, but not other WebUI origins.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       OpenNewTabFromChromeUntrusted) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test"));
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test2"));

  // Loads a test extension.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Opens a chrome-untrusted:// page.
  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://test/title1.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  {
    // Trigger a `window.open()` to a Web origin from chrome-untrusted:// page.
    const GURL web_url =
        embedded_test_server()->GetURL("example.com", "/simple.html");
    content::TestNavigationObserver navigation_observer(web_url);
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(content::ExecJs(
        rfh, content::JsReplace("window.open($1, '_blank');", web_url.spec())));
    navigation_observer.Wait();
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());

    // The extension should see the request to the Web origin.
    EXPECT_TRUE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                    web_url.host()));
  }

  {
    // Trigger a `window.open()` to a WebUI origin from chrome-untrusted://
    // page.
    const GURL webui_url = GURL("chrome-untrusted://test2/title2.html");
    content::TestNavigationObserver navigation_observer(webui_url);
    navigation_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(content::ExecJs(
        rfh,
        content::JsReplace("window.open($1, '_blank');", webui_url.spec())));
    navigation_observer.Wait();
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());

    // The extension shouldn't see the request to the WebUI pages.
    EXPECT_FALSE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                     webui_url.host()));
  }
}

// Tests that webRequest API can inspect a chrome-untrusted:// main frame
// navigating itself to Web origins.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       NavigateMainFrameToWebOriginFromChromeUntrusted) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test"));

  // Loads a test extension.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Opens a chrome-untrusted:// page.
  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://test/title1.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate the main frame itself to Web origin, this extension should see
  // the request.
  const auto web_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::TestNavigationObserver navigation_observer(web_url);
  navigation_observer.WatchExistingWebContents();
  ASSERT_TRUE(content::ExecJs(
      rfh, content::JsReplace("location.href=$1;", web_url.spec())));
  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_TRUE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                  web_url.host()));
}

// Tests that webRequest API can't inspect a chrome-untrusted:// main frame
// navigating itself to another WebUI origin.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       NavigateMainFrameToWebUIOriginFromChromeUntrusted) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test"));
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test2"));

  // Loads a test extension.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Opens a chrome-untrusted:// page.
  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://test/title1.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate the main frame itself to Web origin, this extension should see
  // the request.
  const auto webui_url = GURL("chrome-untrusted://test2/title2.html");
  content::TestNavigationObserver navigation_observer(webui_url);
  navigation_observer.WatchExistingWebContents();
  ASSERT_TRUE(content::ExecJs(
      rfh, content::JsReplace("location.href=$1;", webui_url.spec())));
  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_FALSE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                   webui_url.host()));
}

// Tests that webRequest API can't inspect a subframe inside chrome-untrusted://
// navigating to a Web origin.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       SubframeNavigationsInChromeUntrustedPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Allow embedding child frames;
  content::TestUntrustedDataSourceHeaders headers;
  headers.child_src = "child-src *;";
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test", headers));

  // Loads a test extension.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Opens a chrome-untrusted:// page.
  auto* rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome-untrusted://test/title1.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Start a subframe navigation to Web origin.
  const auto web_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::TestNavigationObserver navigation_observer(web_url);
  navigation_observer.WatchExistingWebContents();
  ASSERT_TRUE(content::ExecJs(rfh, content::JsReplace(R"javascript(
        const el = document.createElement("iframe");
        document.body.appendChild(el);
        el.src = $1;
      )javascript",
                                                      web_url.spec())));
  navigation_observer.Wait();

  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_FALSE(HasSeenWebRequestInBackgroundScript(extension, profile(),
                                                   web_url.host()));
}

// Test fixture which sets a custom NTP Page.
class NTPInterceptionWebRequestAPITest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  NTPInterceptionWebRequestAPITest()
      : ExtensionApiTest(GetParam()),
        https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  NTPInterceptionWebRequestAPITest(const NTPInterceptionWebRequestAPITest&) =
      delete;
  NTPInterceptionWebRequestAPITest& operator=(
      const NTPInterceptionWebRequestAPITest&) = delete;
  ~NTPInterceptionWebRequestAPITest() override = default;

  // ExtensionApiTest override:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    test_data_dir_ = test_data_dir_.AppendASCII("webrequest")
                         .AppendASCII("ntp_request_interception");
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());

    GURL ntp_url = https_test_server_.GetURL("/fake_ntp.html");
    ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        profile(), https_test_server_.base_url().spec(), ntp_url.spec());
  }

  const net::EmbeddedTestServer* https_test_server() const {
    return &https_test_server_;
  }

 private:
  net::EmbeddedTestServer https_test_server_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         NTPInterceptionWebRequestAPITest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         NTPInterceptionWebRequestAPITest,
                         ::testing::Values(ContextType::kServiceWorker));

// Ensures that requests made by the NTP Instant renderer are hidden from the
// Web Request API. Regression test for crbug.com/797461.
IN_PROC_BROWSER_TEST_P(NTPInterceptionWebRequestAPITest,
                       NTPRendererRequestsHidden) {
  // Loads an extension which tries to intercept requests to
  // "fake_ntp_script.js", which will be loaded as part of the NTP renderer.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  // Wait for webRequest listeners to be set up.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();

  // Have the extension listen for requests to |fake_ntp_script.js|.
  listener.Reply(https_test_server()->GetURL("/fake_ntp_script.js").spec());

  // Returns true if the given extension was able to intercept the request to
  // "fake_ntp_script.js".
  auto was_script_request_intercepted =
      [this](const ExtensionId& extension_id) {
        const std::optional<bool> result = ExecuteScriptAndReturnBool(
            extension_id, profile(), "getAndResetRequestIntercepted();");
        DCHECK(result);
        return *result;
      };

  // Returns true if the given |web_contents| has window.scriptExecuted set to
  // true;
  auto was_ntp_script_loaded = [](content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "!!window.scriptExecuted;")
        .ExtractBool();
  };

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the NTP. The request for "fake_ntp_script.js" should not have
  // reached the extension, since it was made by the instant NTP renderer, which
  // is semi-privileged.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(was_ntp_script_loaded(web_contents));
  ASSERT_TRUE(search::IsInstantNTP(web_contents));
  EXPECT_FALSE(was_script_request_intercepted(extension->id()));

  // However, when a normal webpage requests the same script, the request should
  // be seen by the extension.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/page_with_ntp_script.html")));
  EXPECT_TRUE(was_ntp_script_loaded(web_contents));
  ASSERT_FALSE(search::IsInstantNTP(web_contents));
  EXPECT_TRUE(was_script_request_intercepted(extension->id()));
}

// Test fixture testing that requests made for the OneGoogleBar on behalf of
// the WebUI NTP can't be intercepted by extensions.
class WebUiNtpInterceptionWebRequestAPITest
    : public ExtensionApiTest,
      public OneGoogleBarServiceObserver,
      public testing::WithParamInterface<ContextType> {
 public:
  WebUiNtpInterceptionWebRequestAPITest()
      : ExtensionApiTest(GetParam()),
        https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  WebUiNtpInterceptionWebRequestAPITest(
      const WebUiNtpInterceptionWebRequestAPITest&) = delete;
  WebUiNtpInterceptionWebRequestAPITest& operator=(
      const WebUiNtpInterceptionWebRequestAPITest&) = delete;
  ~WebUiNtpInterceptionWebRequestAPITest() override = default;

  // ExtensionApiTest override:
  void SetUp() override {
    https_test_server_.RegisterRequestMonitor(base::BindRepeating(
        &WebUiNtpInterceptionWebRequestAPITest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_test_server_.InitializeAndListen());
    ExtensionApiTest::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kGoogleBaseURL,
                                    https_test_server_.base_url().spec());
  }
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    https_test_server_.StartAcceptingConnections();

    one_google_bar_url_ =
        one_google_bar_service()->loader_for_testing()->GetLoadURLForTesting();

    // Can't declare |runloop_| as a data member on the stack since it needs to
    // be be constructed from a single-threaded context.
    runloop_ = std::make_unique<base::RunLoop>();
    one_google_bar_service()->AddObserver(this);
  }

  // OneGoogleBarServiceObserver overrides:
  void OnOneGoogleBarDataUpdated() override { runloop_->Quit(); }
  void OnOneGoogleBarServiceShuttingDown() override {
    one_google_bar_service()->RemoveObserver(this);
  }

  GURL one_google_bar_url() const { return one_google_bar_url_; }

  // Waits for OneGoogleBar data to be updated. Should only be used once.
  void WaitForOneGoogleBarDataUpdate() { runloop_->Run(); }

  bool GetAndResetOneGoogleBarRequestSeen() {
    base::AutoLock lock(lock_);
    bool result = one_google_bar_request_seen_;
    one_google_bar_request_seen_ = false;
    return result;
  }

 private:
  OneGoogleBarService* one_google_bar_service() {
    return OneGoogleBarServiceFactory::GetForProfile(profile());
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (request.GetURL() == one_google_bar_url_) {
      base::AutoLock lock(lock_);
      one_google_bar_request_seen_ = true;
    }
  }

  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<base::RunLoop> runloop_;

  // Initialized on the UI thread in SetUpOnMainThread. Read on UI and Embedded
  // Test Server IO thread thereafter.
  GURL one_google_bar_url_;

  // Accessed on multiple threads- UI and Embedded Test Server IO thread. Access
  // requires acquiring |lock_|.
  bool one_google_bar_request_seen_ = false;

  base::Lock lock_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         WebUiNtpInterceptionWebRequestAPITest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         WebUiNtpInterceptionWebRequestAPITest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(WebUiNtpInterceptionWebRequestAPITest,
                       OneGoogleBarRequestsHidden) {
  // Loads an extension which tries to intercept requests to the OneGoogleBar.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest")
                        .AppendASCII("ntp_request_interception")
                        .AppendASCII("extension"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Have the extension listen for requests to |one_google_bar_url()|.
  listener.Reply(one_google_bar_url().spec());

  // Returns true if the given extension was able to intercept the request to
  // |one_google_bar_url()|.
  auto was_script_request_intercepted =
      [this](const ExtensionId& extension_id) {
        std::optional<bool> result = ExecuteScriptAndReturnBool(
            extension_id, profile(), "getAndResetRequestIntercepted();");
        DCHECK(result);
        return *result;
      };

  ASSERT_FALSE(GetAndResetOneGoogleBarRequestSeen());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ASSERT_EQ(ntp_test_utils::GetFinalNtpUrl(browser()->profile()),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
  WaitForOneGoogleBarDataUpdate();
  ASSERT_TRUE(GetAndResetOneGoogleBarRequestSeen());

  // Ensure that the extension wasn't able to intercept the request.
  EXPECT_FALSE(was_script_request_intercepted(extension->id()));

  // A normal request to |one_google_bar_url()| (i.e. not made by
  // OneGoogleBarFetcher) should be intercepted by extensions.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), one_google_bar_url()));
  EXPECT_TRUE(was_script_request_intercepted(extension->id()));
  ASSERT_TRUE(GetAndResetOneGoogleBarRequestSeen());
}

// Ensure that devtools frontend requests are hidden from the webRequest API.
IN_PROC_BROWSER_TEST_F(DevToolsFrontendInWebRequestApiTest, HiddenRequests) {
  ASSERT_TRUE(
      RunExtensionTest("webrequest", {.extension_url = "test_devtools.html"}))
      << message_;
}

class WebRequestApiTestWithManagementPolicy
    : public ExtensionApiTestWithManagementPolicy,
      public testing::WithParamInterface<ContextType> {
 public:
  WebRequestApiTestWithManagementPolicy()
      : ExtensionApiTestWithManagementPolicy(GetParam()) {}
  ~WebRequestApiTestWithManagementPolicy() override = default;
  WebRequestApiTestWithManagementPolicy(
      const WebRequestApiTestWithManagementPolicy&) = delete;
  WebRequestApiTestWithManagementPolicy& operator=(
      const WebRequestApiTestWithManagementPolicy&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         WebRequestApiTestWithManagementPolicy,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         WebRequestApiTestWithManagementPolicy,
                         ::testing::Values(ContextType::kServiceWorker));

// Tests that the webRequest events aren't dispatched when the request initiator
// is protected by policy.
IN_PROC_BROWSER_TEST_P(WebRequestApiTestWithManagementPolicy,
                       InitiatorProtectedByPolicy) {
  // We expect that no request will be hidden or modification blocked. This
  // means that the request to example.com will be seen by the extension.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://notexample.com");
  }

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Host navigated to.
  const std::string example_com = "example.com";

  // URL of a page that initiates a cross domain requests when navigated to.
  const GURL extension_test_url = embedded_test_server()->GetURL(
      example_com,
      "/extensions/api_test/webrequest/policy_blocked/ref_remote_js.html");

  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest/policy_blocked"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Extension communicates back using this listener name.
  const std::string listener_message = "protected_origin";

  // The number of requests initiated by a protected origin is tracked in
  // the extension's background page under this variable name.
  const std::string request_counter_name = "self.protectedOriginCount";

  EXPECT_EQ(0, GetCountFromBackgroundScript(extension, profile(),
                                            request_counter_name));

  // Wait until all remote Javascript files have been blocked / pulled down.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Domain that hosts javascript file referenced by example_com.
  const std::string example2_com = "example2.com";

  // The server saw a request for the remote Javascript file.
  EXPECT_TRUE(BrowsedTo(example2_com));

  // The request was seen by the extension.
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            request_counter_name));

  // Clear the list of domains the server has seen.
  ClearRequestLog();

  // Make sure we've cleared the embedded server history.
  EXPECT_FALSE(BrowsedTo(example2_com));

  // Set the policy to hide requests to example.com or any resource
  // it includes. We expect that in this test, the request to example2.com
  // will not be seen by the extension.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://" + example_com);
  }

  // Wait until all remote Javascript files have been pulled down.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The server saw a request for the remote Javascript file.
  EXPECT_TRUE(BrowsedTo(example2_com));

  // The request was hidden from the extension.
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            request_counter_name));
}

// Tests that the webRequest events aren't dispatched when the URL of the
// request is protected by policy.
IN_PROC_BROWSER_TEST_P(WebRequestApiTestWithManagementPolicy,
                       UrlProtectedByPolicy) {
  // Host protected by policy.
  const std::string protected_domain = "example.com";

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://" + protected_domain);
  }

  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadExtension(test_data_dir_.AppendASCII("webrequest/policy_blocked"));

  // Listen in case extension sees the request.
  ExtensionTestMessageListener before_request_listener("protected_url");

  // Path to resolve during test navigations.
  const std::string test_path = "/defaultresponse?protected_url";

  // Navigate to the protected domain and wait until page fully loads.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL(protected_domain, test_path),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The server saw a request for the protected site.
  EXPECT_TRUE(BrowsedTo(protected_domain));

  // The request was hidden from the extension.
  EXPECT_FALSE(before_request_listener.was_satisfied());

  // Host not protected by policy.
  const std::string unprotected_domain = "notblockedexample.com";

  // Now we'll test browsing to a non-protected website where we expect the
  // extension to see the request.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL(unprotected_domain, test_path),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The server saw a request for the non-protected site.
  EXPECT_TRUE(BrowsedTo(unprotected_domain));

  // The request was visible from the extension.
  EXPECT_TRUE(before_request_listener.was_satisfied());
}

// Test that no webRequest events are seen for a protected host during normal
// navigation. This replicates most of the tests from
// WebRequestWithWithheldPermissions with a protected host. Granting a tab
// specific permission shouldn't bypass our policy.
IN_PROC_BROWSER_TEST_P(WebRequestApiTestWithManagementPolicy,
                       WebRequestProtectedByPolicy) {
  // Host protected by policy.
  const std::string protected_domain = "example.com";

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://" + protected_domain);
  }

  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Navigate the browser to a page in a new tab.
  GURL url = embedded_test_server()->GetURL(protected_domain, "/empty.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  int port = embedded_test_server()->port();
  const std::string kXhrPath = "simple.html";

  // The extension shouldn't have currently received any webRequest events,
  // since it doesn't have permission (and shouldn't receive any from an XHR).
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundScript(extension, profile()));
  PerformXhrInFrame(web_contents->GetPrimaryMainFrame(), protected_domain, port,
                    kXhrPath);
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundScript(extension, profile()));

  // Grant activeTab permission, and perform another XHR. The extension should
  // still be blocked due to ExtensionSettings policy on example.com.
  // Only records ACCESS_WITHHELD, not ACCESS_DENIED, this is why it matches
  // BLOCKED_ACTION_NONE.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension->id()));
  runner->accept_bubble_for_testing(true);
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension->id()));
  int xhr_count = GetWebRequestCountFromBackgroundScript(extension, profile());
  // ... which means that we should have a non-zero xhr count if the policy
  // didn't block the events.
  EXPECT_EQ(0, xhr_count);
  // And the extension should also block future events.
  PerformXhrInFrame(web_contents->GetPrimaryMainFrame(), protected_domain, port,
                    kXhrPath);
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundScript(extension, profile()));
}

// A test fixture which mocks the Time::Now() function to ensure that the
// default clock returns monotonically increasing time.
class ExtensionWebRequestMockedClockTest
    : public ExtensionWebRequestApiTestWithContextType {
 public:
  ExtensionWebRequestMockedClockTest()
      : scoped_time_clock_override_(&ExtensionWebRequestMockedClockTest::Now,
                                    nullptr,
                                    nullptr) {}

  ExtensionWebRequestMockedClockTest(
      const ExtensionWebRequestMockedClockTest&) = delete;
  ExtensionWebRequestMockedClockTest& operator=(
      const ExtensionWebRequestMockedClockTest&) = delete;

 private:
  static base::Time Now() {
    static base::Time now_time = base::Time::UnixEpoch();
    now_time += base::Milliseconds(1);
    return now_time;
  }

  base::subtle::ScopedTimeClockOverrides scoped_time_clock_override_;
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ExtensionWebRequestMockedClockTest,
    ::testing::Values(
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ExtensionWebRequestMockedClockTest,
    ::testing::Values(
        std::make_pair(
            ContextType::kServiceWorkerMV2,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kServiceWorkerMV2,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

// Tests that we correctly dispatch the OnActionIgnored event on an extension
// if the extension's proposed redirect is ignored.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestMockedClockTest,
                       OnActionIgnored_Redirect) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load the two extensions. They redirect "google.com" main-frame urls to
  // the corresponding "example.com and "foo.com" urls.
  base::FilePath test_dir =
      test_data_dir_.AppendASCII("webrequest/on_action_ignored");

  // Load the first extension.
  ExtensionTestMessageListener ready_1_listener("ready_1");
  const Extension* extension_1 =
      LoadExtension(test_dir.AppendASCII("extension_1"));
  ASSERT_TRUE(extension_1);
  ASSERT_TRUE(ready_1_listener.WaitUntilSatisfied());
  const ExtensionId extension_id_1 = extension_1->id();

  // Load the second extension.
  ExtensionTestMessageListener ready_2_listener("ready_2");
  const Extension* extension_2 =
      LoadExtension(test_dir.AppendASCII("extension_2"));
  ASSERT_TRUE(extension_2);
  ASSERT_TRUE(ready_2_listener.WaitUntilSatisfied());
  const ExtensionId extension_id_2 = extension_2->id();

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_LT(prefs->GetLastUpdateTime(extension_id_1),
            prefs->GetLastUpdateTime(extension_id_2));

  // The extensions will notify the browser if their proposed redirect was
  // successful or not.
  ExtensionTestMessageListener redirect_ignored_listener("redirect_ignored");
  ExtensionTestMessageListener redirect_successful_listener(
      "redirect_successful");

  const GURL url = embedded_test_server()->GetURL("google.com", "/simple.html");
  const GURL expected_redirect_url_1 =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  const GURL expected_redirect_url_2 =
      embedded_test_server()->GetURL("foo.com", "/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // The second extension is the latest installed, hence it's redirect url
  // should take precedence.
  EXPECT_EQ(expected_redirect_url_2, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(redirect_ignored_listener.WaitUntilSatisfied());
  EXPECT_EQ(extension_id_1,
            redirect_ignored_listener.extension_id_for_message());
  EXPECT_TRUE(redirect_successful_listener.WaitUntilSatisfied());
  EXPECT_EQ(extension_id_2,
            redirect_successful_listener.extension_id_for_message());

  // Now let |extension_1| be installed after |extension_2|. For an unpacked
  // extension, reloading is equivalent to a reinstall.
  ready_1_listener.Reset();
  ReloadExtension(extension_id_1);
  ASSERT_TRUE(ready_1_listener.WaitUntilSatisfied());

  EXPECT_LT(prefs->GetLastUpdateTime(extension_id_2),
            prefs->GetLastUpdateTime(extension_id_1));

  redirect_ignored_listener.Reset();
  redirect_successful_listener.Reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // The first extension is the latest installed, hence it's redirect url
  // should take precedence.
  EXPECT_EQ(expected_redirect_url_1, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(redirect_ignored_listener.WaitUntilSatisfied());
  EXPECT_EQ(extension_id_2,
            redirect_ignored_listener.extension_id_for_message());
  EXPECT_TRUE(redirect_successful_listener.WaitUntilSatisfied());
  EXPECT_EQ(extension_id_1,
            redirect_successful_listener.extension_id_for_message());
}

// Regression test for http://crbug.com/1074282.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       StaleHeadersAfterRedirect) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Stale Headers Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        self.locationCount = 0;
        self.requestCount = 0;
        chrome.test.sendMessage('ready', function(reply) {
          chrome.webRequest.onResponseStarted.addListener(function(details) {
              self.requestCount++;
              var headers = details.responseHeaders;
              for (var i = 0; i < headers.length; i++) {
                if (headers[i].name === 'Location') {
                  self.locationCount++;
                }
              }
            },
            {urls: ['<all_urls>'], types: ['xmlhttprequest']},
            ['responseHeaders', 'extraHeaders']
          );
        });
      )");

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/redirect-and-wait")
          return nullptr;

        // Wait for the listener to be installed before proceeding.
        base::WaitableEvent unblock(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED);
        // Post a task to the UI thread since the request handler runs on a
        // background thread.
        task_runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                                listener.Reply("");
                                WaitForExtraHeadersListener(
                                    &unblock, browser()->profile());
                              }));
        unblock.Wait();

        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
        http_response->AddCustomHeader(
            "Location", embedded_test_server()->GetURL("/echo").spec());
        return http_response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a basic page so XHR requests work.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echo")));

  // Make a XHR request which redirects. The final response should not include
  // the Location header.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  PerformXhrInFrame(web_contents->GetPrimaryMainFrame(),
                    embedded_test_server()->host_port_pair().host(),
                    embedded_test_server()->port(), "redirect-and-wait");
  EXPECT_EQ(
      GetCountFromBackgroundScript(extension, profile(), "self.requestCount"),
      1);
  EXPECT_EQ(
      GetCountFromBackgroundScript(extension, profile(), "self.locationCount"),
      0);
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       ChangeHeaderUMAs) {
  using RequestHeaderType =
      extension_web_request_api_helpers::RequestHeaderType;
  using ResponseHeaderType =
      extension_web_request_api_helpers::ResponseHeaderType;

  ASSERT_TRUE(embedded_test_server()->Start());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request UMA Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        chrome.webRequest.onBeforeSendHeaders.addListener(function(details) {
          var headers = details.requestHeaders;
          for (var i = 0; i < headers.length; i++) {
            if (headers[i].name.toLowerCase() == 'user-agent') {
              headers[i].value = 'foo';
              break;
            }
          }
          headers.push({name: 'Foo1', value: 'Bar1'});
          headers.push({name: 'Foo2', value: 'Bar2'});
          headers.push({name: 'DNT', value: '0'});
          return {requestHeaders: headers};
        }, {urls: ['*://*/set-cookie*']},
        ['blocking', 'requestHeaders', 'extraHeaders']);

        chrome.webRequest.onHeadersReceived.addListener(function(details) {
          var headers = details.responseHeaders;
          for (var i = 0; i < headers.length; i++) {
            if (headers[i].name.toLowerCase() == 'set-cookie' &&
                headers[i].value == 'key1=val1') {
              headers.splice(i, 1);
              i--;
            } else if (headers[i].name == 'Content-Length') {
              headers[i].value = '0';
            }
          }
          headers.push({name: 'Foo3', value: 'Bar3'});
          headers.push({name: 'Foo4', value: 'Bar4'});
          return {responseHeaders: headers};
        }, {urls: ['*://*/set-cookie*']},
        ['blocking', 'responseHeaders', 'extraHeaders']);

        chrome.test.sendMessage('ready');
      )");

  ExtensionTestMessageListener listener("ready");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/set-cookie?key1=val1&key2=val2")));

  // Changed histograms should record kUserAgent request header along with
  // kSetCookie and kContentLength response headers.
  tester.ExpectUniqueSample("Extensions.WebRequest.RequestHeaderChanged",
                            RequestHeaderType::kUserAgent, 1);
  EXPECT_THAT(
      tester.GetAllSamples("Extensions.WebRequest.ResponseHeaderChanged"),
      ::testing::UnorderedElementsAre(
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ResponseHeaderType::kSetCookie),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ResponseHeaderType::kContentLength),
                       1)));

  // Added request header histogram should record kOther and kDNT.
  EXPECT_THAT(tester.GetAllSamples("Extensions.WebRequest.RequestHeaderAdded"),
              ::testing::UnorderedElementsAre(
                  base::Bucket(static_cast<base::HistogramBase::Sample>(
                                   RequestHeaderType::kDnt),
                               1),
                  base::Bucket(static_cast<base::HistogramBase::Sample>(
                                   RequestHeaderType::kOther),
                               2)));

  // Added response header histogram should record kOther.
  tester.ExpectUniqueSample("Extensions.WebRequest.ResponseHeaderAdded",
                            ResponseHeaderType::kOther, 2);

  // Histograms for removed headers should record kNone.
  tester.ExpectUniqueSample("Extensions.WebRequest.RequestHeaderRemoved",
                            RequestHeaderType::kNone, 1);
  tester.ExpectUniqueSample("Extensions.WebRequest.ResponseHeaderRemoved",
                            ResponseHeaderType::kNone, 1);
}

IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       RemoveHeaderUMAs) {
  using RequestHeaderType =
      extension_web_request_api_helpers::RequestHeaderType;
  using ResponseHeaderType =
      extension_web_request_api_helpers::ResponseHeaderType;

  ASSERT_TRUE(embedded_test_server()->Start());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request UMA Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        chrome.webRequest.onBeforeSendHeaders.addListener(function(details) {
          var headers = details.requestHeaders;
          for (var i = 0; i < headers.length; i++) {
            if (headers[i].name.toLowerCase() == 'user-agent') {
              headers.splice(i, 1);
              break;
            }
          }
          return {requestHeaders: headers};
        }, {urls: ['*://*/set-cookie*']},
        ['blocking', 'requestHeaders', 'extraHeaders']);

        chrome.webRequest.onHeadersReceived.addListener(function(details) {
          var headers = details.responseHeaders;
          for (var i = 0; i < headers.length; i++) {
            if (headers[i].name.toLowerCase() == 'set-cookie') {
              headers.splice(i, 1);
              break;
            }
          }
          return {responseHeaders: headers};
        }, {urls: ['*://*/set-cookie*']},
        ['blocking', 'responseHeaders', 'extraHeaders']);

        chrome.test.sendMessage('ready');
      )");

  ExtensionTestMessageListener listener("ready");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  base::HistogramTester tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/set-cookie?Foo=Bar")));

  // Histograms for removed headers should record kUserAgent and kSetCookie.
  tester.ExpectUniqueSample("Extensions.WebRequest.RequestHeaderRemoved",
                            RequestHeaderType::kUserAgent, 1);
  tester.ExpectUniqueSample("Extensions.WebRequest.ResponseHeaderRemoved",
                            ResponseHeaderType::kSetCookie, 1);

  // Histograms for changed headers should record kNone.
  tester.ExpectUniqueSample("Extensions.WebRequest.RequestHeaderChanged",
                            RequestHeaderType::kNone, 1);
  tester.ExpectUniqueSample("Extensions.WebRequest.ResponseHeaderChanged",
                            ResponseHeaderType::kNone, 1);

  // Histograms for added headers should record kNone.
  tester.ExpectUniqueSample("Extensions.WebRequest.RequestHeaderAdded",
                            RequestHeaderType::kNone, 1);
  tester.ExpectUniqueSample("Extensions.WebRequest.ResponseHeaderAdded",
                            ResponseHeaderType::kNone, 1);
}

struct SWTestParams {
  // This parameter is for opt_extraInfoSpec passed to addEventListener.
  // 'blocking' and 'requestHeaders' if it's false, and 'extraHeaders' in
  // addition to them if it's true.
  bool extra_info_spec;
  ContextType context_type;
};

class ServiceWorkerWebRequestApiTest
    : public testing::WithParamInterface<SWTestParams>,
      public ExtensionApiTest {
 public:
  ServiceWorkerWebRequestApiTest()
      : ExtensionApiTest(GetParam().context_type) {}
  ~ServiceWorkerWebRequestApiTest() override = default;
  ServiceWorkerWebRequestApiTest(const ServiceWorkerWebRequestApiTest&) =
      delete;
  ServiceWorkerWebRequestApiTest& operator=(
      const ServiceWorkerWebRequestApiTest&) = delete;

  // The options passed as opt_extraInfoSpec to addEventListener.
  enum class ExtraInfoSpec {
    // 'blocking', 'requestHeaders'
    kDefault,
    // kDefault + 'extraHeaders'
    kExtraHeaders
  };

  static ExtraInfoSpec GetExtraInfoSpec() {
    return GetParam().extra_info_spec ? ExtraInfoSpec::kExtraHeaders
                                      : ExtraInfoSpec::kDefault;
  }

  void InstallRequestHeaderModifyingExtension() {
    TestExtensionDir test_dir;
    test_dir.WriteManifest(R"({
        "name": "Web Request Header Modifying Extension",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");

    const char kBackgroundScript[] = R"(
        chrome.webRequest.onBeforeSendHeaders.addListener(function(details) {
              details.requestHeaders.push({name: 'foo', value: 'bar'});
              details.requestHeaders.push({
                name: 'frameId',
                value: details.frameId.toString()
              });
              details.requestHeaders.push({
                name: 'resourceType',
                value: details.type
              });
              return {requestHeaders: details.requestHeaders};
            },
            {urls: ['*://*/echoheader*']},
            [%s]);

        chrome.test.sendMessage('ready');
      )";
    std::string opt_extra_info_spec = "'blocking', 'requestHeaders'";
    if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
      opt_extra_info_spec += ", 'extraHeaders'";
    test_dir.WriteFile(
        FILE_PATH_LITERAL("background.js"),
        base::StringPrintf(kBackgroundScript, opt_extra_info_spec.c_str()));

    ExtensionTestMessageListener listener("ready");
    ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  void RegisterServiceWorker(const std::string& worker_path,
                             const std::optional<std::string>& scope) {
    GURL url = embedded_test_server()->GetURL(
        "/service_worker/create_service_worker.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    std::string script = content::JsReplace("register($1, $2);", worker_path,
                                            scope ? *scope : std::string());
    EXPECT_EQ(
        "DONE",
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), script));
  }

  // Ensures requests made by the |worker_script_name| service worker can be
  // intercepted by extensions.
  void RunServiceWorkerFetchTest(const std::string& worker_script_name) {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Install the test extension.
    InstallRequestHeaderModifyingExtension();

    // Register a service worker and navigate to a page it controls.
    RegisterServiceWorker(worker_script_name, std::nullopt);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/service_worker/fetch_from_page.html")));

    // Make a fetch from the controlled page. Depending on the worker script,
    // the fetch might go to the service worker and be re-issued, or might
    // fallback to network, or skip the worker, etc. In any case, this function
    // expects a network request to happen, and that the extension modify the
    // headers of the request before it goes to network. Verify that it was able
    // to inject a header of "foo=bar".
    EXPECT_EQ("bar",
              EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "fetch_from_page('/echoheader?foo');"));
  }
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackgroundWithExtraHeaders,
    ServiceWorkerWebRequestApiTest,
    ::testing::Values(SWTestParams(true, ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ServiceWorkerWebRequestApiTest,
    ::testing::Values(SWTestParams(false, ContextType::kPersistentBackground)));

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorkerWithExtraHeaders,
    ServiceWorkerWebRequestApiTest,
    ::testing::Values(SWTestParams(true, ContextType::kServiceWorkerMV2)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ServiceWorkerWebRequestApiTest,
    ::testing::Values(SWTestParams(false, ContextType::kServiceWorkerMV2)));

IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest, ServiceWorkerFetch) {
  RunServiceWorkerFetchTest("fetch_event_respond_with_fetch.js");
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest, ServiceWorkerFallback) {
  RunServiceWorkerFetchTest("fetch_event_pass_through.js");
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest,
                       ServiceWorkerNoFetchHandler) {
  RunServiceWorkerFetchTest("empty.js");
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest,
                       ServiceWorkerFallbackAfterRedirect) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallRequestHeaderModifyingExtension();

  RegisterServiceWorker("/fetch_event_passthrough.js", "/echoheader");

  // Make sure the request is intercepted with no redirect.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echoheader?foo")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("bar", EvalJs(web_contents, "document.body.textContent;"));

  // Make sure the request is intercepted with a redirect.
  GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" +
      embedded_test_server()->GetURL("/echoheader?foo").spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url));
  EXPECT_EQ("bar", EvalJs(web_contents, "document.body.textContent;"));
}

// An extension should be able to modify the request header for service worker
// script by using WebRequest API.
IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest, ServiceWorkerScript) {
  // The extension to be used in this test adds foo=bar request header.
  const char kScriptPath[] = "/echoheader_service_worker.js";
  // The request handler below will run on the EmbeddedTestServer's IO thread.
  // Hence guard access to |served_service_worker_count| and |foo_header_value|
  // using a lock.
  base::Lock lock;
  int served_service_worker_count = 0;
  std::string foo_header_value;

  // Capture the value of a request header foo, which should be added if
  // extension modifies the request header.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kScriptPath)
          return nullptr;

        base::AutoLock auto_lock(lock);
        ++served_service_worker_count;
        foo_header_value.clear();
        if (request.headers.find("foo") != request.headers.end())
          foo_header_value = request.headers.at("foo");

        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/javascript");
        response->AddCustomHeader("Cache-Control", "no-cache");
        response->set_content("// empty");
        return response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallRequestHeaderModifyingExtension();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Register a service worker. The worker script should have "foo: bar" request
  // header added by the extension.
  std::string script =
      content::JsReplace("register($1, './in-scope');", kScriptPath);
  EXPECT_EQ("DONE", EvalJs(web_contents, script));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(1, served_service_worker_count);
    EXPECT_EQ("bar", foo_header_value);
  }

  // Update the worker. The worker should have "foo: bar" request header in the
  // request for update checking.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("DONE", EvalJs(web_contents, "update('./in-scope');"));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(2, served_service_worker_count);
    EXPECT_EQ("bar", foo_header_value);
  }
}

// An extension should be able to modify the request header for module service
// worker script by using WebRequest API.
IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest,
                       ModuleServiceWorkerScript) {
  // The extension to be used in this test adds foo=bar request header.
  constexpr char kScriptPath[] = "/echoheader_service_worker.js";
  // The request handler below will run on the EmbeddedTestServer's IO thread.
  // Hence guard access to |served_service_worker_count| and |foo_header_value|
  // using a lock.
  base::Lock lock;
  int served_service_worker_count = 0;
  std::string foo_header_value;

  // Capture the value of a request header foo, which should be added if
  // extension modifies the request header.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != kScriptPath)
          return nullptr;

        base::AutoLock auto_lock(lock);
        ++served_service_worker_count;
        foo_header_value.clear();
        if (base::Contains(request.headers, "foo"))
          foo_header_value = request.headers.at("foo");

        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/javascript");
        response->AddCustomHeader("Cache-Control", "no-cache");
        response->set_content("// empty");
        return response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallRequestHeaderModifyingExtension();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Register a service worker. `EvalJs` is blocked until the request handler
  // serves the worker script. The worker script should have "foo: bar" request
  // header added by the extension.
  std::string script =
      content::JsReplace("register($1, './in-scope', 'module');", kScriptPath);
  EXPECT_EQ("DONE", EvalJs(web_contents, script));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(1, served_service_worker_count);
    EXPECT_EQ("bar", foo_header_value);
  }

  // Update the worker. `EvalJs` is blocked until the request handler serves the
  // worker script. The worker should have "foo: bar" request header in the
  // request for update checking.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("DONE", EvalJs(web_contents, "update('./in-scope');"));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(2, served_service_worker_count);
    EXPECT_EQ("bar", foo_header_value);
  }
}

// An extension should be able to modify the request header for module service
// worker script with static import by using WebRequest API.
IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest,
                       ModuleServiceWorkerScriptWithStaticImport) {
  // The extension to be used in this test adds foo=bar request header.
  constexpr char kScriptPath[] = "/static-import-worker.js";
  constexpr char kImportedScriptPath[] = "/echoheader_service_worker.js";
  // The request handler below will run on the EmbeddedTestServer's IO thread.
  // Hence guard access to |served_service_worker_count| and |foo_header_value|
  // using a lock.
  base::Lock lock;
  int served_service_worker_count = 0;
  std::string foo_header_value;

  // Capture the value of a request header foo, which should be added if
  // extension modifies the request header.
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Handle the top-level worker script.
        if (request.relative_url == kScriptPath) {
          base::AutoLock auto_lock(lock);
          ++served_service_worker_count;
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/javascript");
          response->AddCustomHeader("Cache-Control", "no-cache");
          response->set_content("import './echoheader_service_worker.js';");
          return response;
        }
        // Handle the static-imported script.
        if (request.relative_url == kImportedScriptPath) {
          base::AutoLock auto_lock(lock);
          ++served_service_worker_count;
          foo_header_value.clear();
          if (base::Contains(request.headers, "foo"))
            foo_header_value = request.headers.at("foo");
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/javascript");
          response->AddCustomHeader("Cache-Control", "no-cache");
          response->set_content("// empty");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  InstallRequestHeaderModifyingExtension();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Register a service worker. The worker script should have "foo: bar" request
  // header added by the extension.
  std::string script =
      content::JsReplace("register($1, './in-scope', 'module');", kScriptPath);
  EXPECT_EQ("DONE", EvalJs(web_contents, script));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(2, served_service_worker_count);
    EXPECT_EQ("bar", foo_header_value);
  }

  // Update the worker. The worker should have "foo: bar" request header in the
  // request for update checking.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("DONE", EvalJs(web_contents, "update('./in-scope');"));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(4, served_service_worker_count);
    EXPECT_EQ("bar", foo_header_value);
  }
}

// Ensure that extensions can intercept service worker navigation preload
// requests.
IN_PROC_BROWSER_TEST_P(ServiceWorkerWebRequestApiTest,
                       ServiceWorkerNavigationPreload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install the test extension.
  InstallRequestHeaderModifyingExtension();

  // Register a service worker that uses navigation preload.
  RegisterServiceWorker("/service_worker/navigation_preload_worker.js",
                        "/echoheader");

  // Navigate to "/echoheader". The browser will detect that the service worker
  // above is registered with this scope and has navigation preload enabled.
  // So it will send the navigation preload request to network while at the same
  // time starting up the service worker. The service worker will get the
  // response for the navigation preload request, and respond with it to create
  // the page.
  GURL url = embedded_test_server()->GetURL(
      "/echoheader?frameId&resourceType&service-worker-navigation-preload");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Since the request was to "/echoheader", the response describes the request
  // headers.
  //
  // The expectation is "0\nmain_frame\ntrue" because...
  //
  // 1) The extension is expected to add a "frameId: {id}" header, where {id} is
  //    details.frameId. This id is 0 for the main frame.
  // 2) The extension is similarly expected to add a "resourceType: {type}"
  //    header, where {type} is details.type.
  // 3) The browser adds a "service-worker-navigation-preload: true" header for
  //    navigation preload requests, so also sanity check that header to prove
  //    that this test is really testing the navigation preload request.
  EXPECT_EQ("0\nmain_frame\ntrue",
            EvalJs(web_contents, "document.body.textContent;"));

  // Repeat the test from an iframe, to test that details.frameId and resource
  // type is populated correctly.
  const char kAddIframe[] = R"(
    (async () => {
      const iframe = document.createElement('iframe');
      await new Promise(resolve => {
        iframe.src = $1;
        iframe.onload = resolve;
        document.body.appendChild(iframe);
      });
      const result = iframe.contentWindow.document.body.textContent;

      // Expect "{frameId}\nsub_frame\ntrue" where {frameId} is a positive
      // integer.
      const split = result.split('\n');
      if (parseInt(split[0]) > 0 && split[1] == 'sub_frame' &&
          split[2] == 'true') {
          return 'ok';
      }
      return 'bad result: ' + result;
    })();
  )";

  EXPECT_EQ("ok", EvalJs(web_contents, content::JsReplace(kAddIframe, url)));
}

// Ensure we don't strip off initiator incorrectly in web request events when
// both the normal and incognito contexts are active. Regression test for
// crbug.com/934398.
// TODO(crbug.com/41493389): enable this flaky test
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_Initiator_SpanningIncognito DISABLED_Initiator_SpanningIncognito
#else
#define MAYBE_Initiator_SpanningIncognito Initiator_SpanningIncognito
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_Initiator_SpanningIncognito) {
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest")
                        .AppendASCII("initiator_spanning"));
  ASSERT_TRUE(extension);
  // Save the ID because enabling the extension for incognito mode will
  // invalidate |extension|.
  const ExtensionId extension_id = extension->id();
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);

  // iframe.html loads an iframe to title1.html. The extension listens for the
  // request to title1.html and records the request initiator.
  GURL url = embedded_test_server()->GetURL("google.com", "/iframe.html");
  const std::string url_origin = url::Origin::Create(url).Serialize();

  static constexpr char kScript[] = R"(
    chrome.test.sendScriptResult(JSON.stringify(self.initiators));
    self.initiators = [];
  )";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::optional<std::string> result =
      ExecuteScriptAndReturnString(extension_id, profile(), kScript);
  ASSERT_TRUE(result);
  EXPECT_EQ(base::StringPrintf("[\"%s\"]", url_origin.c_str()), *result);

  // The extension isn't enabled in incognito. Se we shouldn't intercept the
  // request to |url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
  result = ExecuteScriptAndReturnString(extension_id, profile(), kScript);
  ASSERT_TRUE(result);
  EXPECT_EQ("[]", *result);

  // Now enable the extension in incognito.
  extension = nullptr;
  ready_listener.Reset();
  extensions::util::SetIsIncognitoEnabled(extension_id, profile(),
                                          true /* enabled */);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  // Now we should be able to intercept the incognito request.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url));
  result = ExecuteScriptAndReturnString(extension_id, profile(), kScript);
  ASSERT_TRUE(result);
  EXPECT_EQ(base::StringPrintf("[\"%s\"]", url_origin.c_str()), *result);
}

// Ensure we don't strip off initiator incorrectly in web request events when
// both the normal and incognito contexts are active. Regression test for
// crbug.com/934398.
// Flaky on Linux. See http://crbug.com/1423252
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Initiator_SplitIncognito DISABLED_Initiator_SplitIncognito
#else
#define MAYBE_Initiator_SplitIncognito Initiator_SplitIncognito
#endif
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       MAYBE_Initiator_SplitIncognito) {
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  ExtensionTestMessageListener ready_listener("ready");
  ExtensionTestMessageListener incognito_ready_listener("incognito ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrequest").AppendASCII("initiator_split"),
      {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);
  EXPECT_TRUE(incognito_ready_listener.WaitUntilSatisfied());

  // iframe.html loads an iframe to title1.html. The extension listens for the
  // request to title1.html and records the request initiator.
  GURL url_normal =
      embedded_test_server()->GetURL("google.com", "/iframe.html");
  GURL url_incognito =
      embedded_test_server()->GetURL("example.com", "/iframe.html");
  const std::string origin_normal = url::Origin::Create(url_normal).Serialize();
  const std::string origin_incognito =
      url::Origin::Create(url_incognito).Serialize();

  static constexpr char kScript[] = R"(
    chrome.test.sendScriptResult(JSON.stringify(self.initiators));
    self.initiators = [];
  )";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_normal));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, url_incognito));
  std::optional<std::string> result =
      ExecuteScriptAndReturnString(extension->id(), profile(), kScript);
  ASSERT_TRUE(result);
  EXPECT_EQ(base::StringPrintf("[\"%s\"]", origin_normal.c_str()), *result);
  result = ExecuteScriptAndReturnString(
      extension->id(),
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), kScript);
  ASSERT_TRUE(result);
  EXPECT_EQ(base::StringPrintf("[\"%s\"]", origin_incognito.c_str()), *result);
}

// A request handler that sets the Access-Control-Allow-Origin header.
std::unique_ptr<net::test_server::HttpResponse> HandleXHRRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  return http_response;
}

// Regression test for http://crbug.com/971206. The responseHeaders should still
// be present in onBeforeRedirect even for HSTS upgrade.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       ExtraHeadersWithHSTSUpgrade) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.RegisterRequestHandler(
      base::BindRepeating(&HandleXHRRequest));
  ASSERT_TRUE(https_test_server.Start());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request HSTS Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        chrome.webRequest.onBeforeRedirect.addListener(function(details) {
          self.headerCount = details.responseHeaders.length;
        }, {urls: ['<all_urls>']},
        ['responseHeaders', 'extraHeaders']);

        chrome.test.sendMessage('ready');
      )");

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           https_test_server.GetURL("/echo")));

  content::StoragePartition* partition =
      profile()->GetDefaultStoragePartition();
  base::RunLoop run_loop;
  partition->GetNetworkContext()->AddHSTS(
      https_test_server.host_port_pair().host(),
      base::Time::Now() + base::Days(100), true, run_loop.QuitClosure());
  run_loop.Run();

  PerformXhrInFrame(browser()
                        ->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetPrimaryMainFrame(),
                    https_test_server.host_port_pair().host(),
                    https_test_server.port(), "echo");
  EXPECT_GT(
      GetCountFromBackgroundScript(extension, profile(), "self.headerCount"),
      0);
}

// Ensure that when an extension blocks a main-frame request, the resultant
// error page attributes this to an extension.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       ErrorPageForBlockedMainFrameNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest(
      "webrequest", {.extension_url = "test_simple_cancel_navigation.html"}))
      << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  std::string body =
      content::EvalJs(tab, "document.body.textContent").ExtractString();

  EXPECT_TRUE(
      base::Contains(body, "This page has been blocked by an extension"));
  EXPECT_TRUE(base::Contains(body, "Try disabling your extensions."));
}

// Regression test for https://crbug.com/1019614.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       HSTSUpgradeAfterRedirect) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(https_test_server.Start());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Web Request HSTS Test",
    "manifest_version": 2,
    "version": "0.1",
    "background": { "scripts": ["background.js"], "persistent": true },
    "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.webRequest.onBeforeRedirect.addListener(() => {}, {
      urls: [ '<all_urls>' ]
    }, [ 'responseHeaders', 'extraHeaders' ]);

    chrome.test.sendMessage('ready');
  )");

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::StoragePartition* partition =
      profile()->GetDefaultStoragePartition();
  base::test::TestFuture<void> hsts_added;
  partition->GetNetworkContext()->AddHSTS("hsts.com",
                                          base::Time::Now() + base::Days(100),
                                          true, hsts_added.GetCallback());
  ASSERT_TRUE(hsts_added.Wait());

  GURL final_url = https_test_server.GetURL("hsts.com", "/echo");
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");
  GURL http_url = final_url.ReplaceComponents(replace_scheme);
  GURL redirect_url = embedded_test_server()->GetURL(
      "test.com", "/server-redirect?" + http_url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url));
  EXPECT_EQ(final_url, browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

// Regression test for https://crbug.com/40864513. This test passes if it
// doesn't crash.
// This is a copy of HSTSUpgradeAfterRedirect, but the redirect contains a CSP
// header.
// TODO(https://crbug.com/40864513) Enable this test.
IN_PROC_BROWSER_TEST_P(ExtensionWebRequestApiTestWithContextType,
                       DISABLED_HSTSUpgradeAfterRedirectWithCSP) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url.starts_with("/server-redirect-with-csp")) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->AddCustomHeader("Location", request.GetURL().query_piece());
          response->AddCustomHeader("Content-Security-Policy",
                                    "frame-ancestors 'none'");
          response->set_code(net::HTTP_MOVED_PERMANENTLY);
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(https_test_server.Start());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Web Request HSTS Test",
    "manifest_version": 2,
    "version": "0.1",
    "background": { "scripts": ["background.js"], "persistent": true },
    "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.webRequest.onBeforeRedirect.addListener(() => {}, {
      urls: [
        '<all_urls>',
      ]
    }, [
      'responseHeaders',
      'extraHeaders',
    ]);

    chrome.test.sendMessage('ready');
  )");

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::StoragePartition* partition =
      profile()->GetDefaultStoragePartition();
  base::test::TestFuture<void> hsts_added;
  partition->GetNetworkContext()->AddHSTS("hsts.com",
                                          base::Time::Now() + base::Days(100),
                                          true, hsts_added.GetCallback());
  ASSERT_TRUE(hsts_added.Wait());

  GURL final_url = https_test_server.GetURL("hsts.com", "/echo");
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");
  GURL http_url = final_url.ReplaceComponents(replace_scheme);
  GURL redirect_url = embedded_test_server()->GetURL(
      "test.com", "/server-redirect-with-csp?" + http_url.spec());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url));
  EXPECT_EQ(final_url, browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

// Tests registering webRequest events in multiple contexts in the same
// extension (which will thus be in the same process). Regression test for
// https://crbug.com/1297276.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       ListenersInMultipleContexts) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an extension that has a page with two iframes. Each iframe registers
  // a listener for the same event.
  static constexpr char kManifest[] =
      R"({
           "name": "ext",
           "manifest_version": 3,
           "version": "1",
           "permissions": ["webRequest"],
           "host_permissions": ["http://example.com/*"]
         })";
  static constexpr char kParentHtml[] =
      R"(<!doctype html>
         <html>
           Hello world
           <iframe src="iframe.html" name="iframe1"></iframe>
           <iframe src="iframe.html" name="iframe2"></iframe>
         </html>)";
  static constexpr char kIframeHtml[] =
      R"(<!doctype html>
         <html>
           Iframe
           <script src="iframe.js"></script>
         </html>)";
  static constexpr char kIframeJs[] =
      R"(const frameName = window.name;
         chrome.webRequest.onBeforeRequest.addListener(
             (details) => {
               chrome.test.sendMessage(frameName + ' event');
             },
             {urls: ['http://example.com/*'], types: ['main_frame']});
         chrome.test.sendMessage(frameName + ' ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("parent.html"), kParentHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("iframe.html"), kIframeHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("iframe.js"), kIframeJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto* router = WebRequestEventRouter::Get(profile());
  ASSERT_TRUE(router);

  static constexpr char kEventName[] = "webRequest.onBeforeRequest";
  EXPECT_EQ(0u, router->GetListenerCountForTesting(profile(), kEventName));

  // Load the extension page and wait for it to register its listeners.
  {
    ExtensionTestMessageListener listener1("iframe1 ready");
    ExtensionTestMessageListener listener2("iframe2 ready");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL("parent.html")));
    ASSERT_TRUE(listener1.WaitUntilSatisfied());
    ASSERT_TRUE(listener2.WaitUntilSatisfied());
  }

  // Two different listeners should be registered.
  EXPECT_EQ(2u, router->GetListenerCountForTesting(profile(), kEventName));

  // Trigger an event. Both listeners should fire.
  {
    ExtensionTestMessageListener listener1("iframe1 event");
    ExtensionTestMessageListener listener2("iframe2 event");
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        embedded_test_server()->GetURL("example.com", "/title1.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    EXPECT_TRUE(listener1.WaitUntilSatisfied());
    EXPECT_TRUE(listener2.WaitUntilSatisfied());
  }
}

struct SWBTestParams {
  // The parameter is for opt_extraInfoSpec passed to addEventListener.
  // 'blocking' if it's false, and 'extraHeaders' in addition to them
  // if it's true.
  bool extra_info_spec;
  ContextType context_type;
};

class SubresourceWebBundlesWebRequestApiTest
    : public testing::WithParamInterface<SWBTestParams>,
      public ExtensionApiTest {
 public:
  SubresourceWebBundlesWebRequestApiTest()
      : ExtensionApiTest(GetParam().context_type) {}
  ~SubresourceWebBundlesWebRequestApiTest() override = default;
  SubresourceWebBundlesWebRequestApiTest(
      const SubresourceWebBundlesWebRequestApiTest&) = delete;
  SubresourceWebBundlesWebRequestApiTest& operator=(
      const SubresourceWebBundlesWebRequestApiTest&) = delete;

 protected:
  // Whether 'extraHeaders' is set in opt_extraInfoSpec of addEventListener.
  enum class ExtraInfoSpec {
    // No 'extraHeaders'
    kDefault,
    // with 'extraHeaders'
    kExtraHeaders
  };

  static ExtraInfoSpec GetExtraInfoSpec() {
    return GetParam().extra_info_spec ? ExtraInfoSpec::kExtraHeaders
                                      : ExtraInfoSpec::kDefault;
  }

  bool TryLoadScript(const std::string& script_src) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string script = base::StringPrintf(R"(
          (() => {
            const script = document.createElement('script');
            return new Promise(resolve => {
              script.addEventListener('load', () => {
                resolve(true);
              });
              script.addEventListener('error', () => {
                resolve(false);
              });
              script.src = '%s';
              document.body.appendChild(script);
            });
          })();
        )",
                                            script_src.c_str());
    return EvalJs(web_contents->GetPrimaryMainFrame(), script).ExtractBool();
  }

  bool TryLoadBundle(const std::string& href, const std::string& resources) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string script = base::StringPrintf(R"(
          (() => {
            const script = document.createElement('script');
            script.type = 'webbundle';
            return new Promise(resolve => {
              script.addEventListener('load', () => {
                resolve(true);
              });
              script.addEventListener('error', () => {
                resolve(false);
              });
              script.textContent = JSON.stringify({
                'source': '%s',
                'resources': ['%s']
              });
              document.body.appendChild(script);
            });
          })();
        )",
                                            href.c_str(), resources.c_str());
    return EvalJs(web_contents->GetPrimaryMainFrame(), script).ExtractBool();
  }

  // Registers a request handler for static content.
  void RegisterRequestHandler(const std::string& relative_url,
                              const std::string& content_type,
                              const std::string& content) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [relative_url, content_type,
         content](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == relative_url) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type(content_type);
            response->set_content(content);
            return std::move(response);
          }
          return nullptr;
        }));
  }

  // Registers a request handler for web bundle. This method takes a pointer of
  // the content of the web bundle, because we can't create the content of the
  // web bundle before starting the server since we need to know the port number
  // of the test server due to the same-origin restriction (the origin of
  // subresource which is written in the web bundle must be same as the origin
  // of the web bundle), and we can't call
  // EmbeddedTestServer::RegisterRequestHandler() after starting the server.
  void RegisterWebBundleRequestHandler(const std::string& relative_url,
                                       const std::string* web_bundle) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [relative_url, web_bundle](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == relative_url) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("application/webbundle");
            response->AddCustomHeader("X-Content-Type-Options", "nosniff");
            response->set_content(*web_bundle);
            return std::move(response);
          }
          return nullptr;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackgroundWithExtraHeaders,
    SubresourceWebBundlesWebRequestApiTest,
    ::testing::Values(SWBTestParams(true, ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    SubresourceWebBundlesWebRequestApiTest,
    ::testing::Values(SWBTestParams(false,
                                    ContextType::kPersistentBackground)));

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorkerWithExtraHeaders,
    SubresourceWebBundlesWebRequestApiTest,
    ::testing::Values(SWBTestParams(true, ContextType::kServiceWorkerMV2)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    SubresourceWebBundlesWebRequestApiTest,
    ::testing::Values(SWBTestParams(false, ContextType::kServiceWorkerMV2)));

// Ensure web request listeners can intercept requests for a web bundle and its
// subresources.
// TODO(crbug.com/40801096): Fix flane and re-enable test.
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest,
                       DISABLED_RequestIntercepted) {
  const std::string uuid_in_package_script_url =
      "uuid-in-package:6a059ece-62f4-4c79-a9e2-1c641cbdbaaf";
  // Create an extension that intercepts requests.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest"]
      })");

  std::string opt_extra_info_spec = "";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
    opt_extra_info_spec += "'extraHeaders'";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        self.numMainResourceRequests = 0;
        self.numWebBundleRequests = 0;
        self.numScriptRequests = 0;
        self.numUUIDInPackageScriptRequests = 0;
        chrome.webRequest.onBeforeRequest.addListener(function(details) {
          if (details.url.includes('test.html'))
            self.numMainResourceRequests++;
          else if (details.url.includes('web_bundle.wbn'))
            self.numWebBundleRequests++;
          else if (details.url.includes('test.js'))
            self.numScriptRequests++;
          else if (details.url === '%s')
            self.numUUIDInPackageScriptRequests++;
        }, {urls: ['<all_urls>']}, [%s]);

        chrome.test.sendMessage('ready');
      )",
                                        uuid_in_package_script_url.c_str(),
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  const std::string page_html = base::StringPrintf(
      R"(
        <title>Loaded</title>
        <body>
        <script>
          (() => {
            const wbn_url =
                new URL('./web_bundle.wbn', location.href).toString();
            const script_url = new URL('./test.js', location.href).toString();
            const uuid_in_package_script_url = '%s';

            const script_web_bundle = document.createElement('script');
            script_web_bundle.type = 'webbundle';
            script_web_bundle.textContent = JSON.stringify({
              'source': wbn_url,
              'resources': [script_url, uuid_in_package_script_url]
            });
            document.body.appendChild(script);
            const script = document.createElement('script');
            script.src = script_url;
            script.addEventListener('load', () => {
              const uuid_in_package_script = document.createElement('script');
              uuid_in_package_script.src = uuid_in_package_script_url;
              document.body.appendChild(uuid_in_package_script);
            });
            document.body.appendChild(script);
          })();
        </script>
        </body>
      )",
      uuid_in_package_script_url.c_str());
  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", page_html);
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  GURL script_url = embedded_test_server()->GetURL("/test.js");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      script_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'ScriptDone';");
  builder.AddExchange(
      uuid_in_package_script_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title += ':UUIDInPackageScriptDone';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::u16string expected_title = u"ScriptDone:UUIDInPackageScriptDone";
  content::TitleWatcher title_watcher(web_contents, expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());
  // Check that the scripts in the web bundle are correctly loaded even when the
  // extension intercepted the request.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numMainResourceRequests"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numWebBundleRequests"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numScriptRequests"));
  EXPECT_EQ(
      1, GetCountFromBackgroundScript(extension, profile(),
                                      "self.numUUIDInPackageScriptRequests"));
}

// Ensure web request API can block the requests for the subresources inside the
// web bundle.
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest,
                       RequestCanceled) {
  // Create an extension that blocks a bundle subresource request.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  std::string pass_uuid_in_package_js_url =
      "uuid-in-package:bf50ad1f-899e-42ca-95ac-ca592aa2ecb5";
  std::string cancel_uuid_in_package_js_url =
      "uuid-in-package:9cc02e52-05b6-466a-8c0e-f22ee86825a8";
  std::string opt_extra_info_spec = "'blocking'";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
    opt_extra_info_spec += ", 'extraHeaders'";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        self.numPassScriptRequests = 0;
        self.numCancelScriptRequests = 0;
        self.numUUIDInPackagePassScriptRequests = 0;
        self.numUUIDInPackageCancelScriptRequests = 0;
        chrome.webRequest.onBeforeRequest.addListener(function(details) {
          if (details.url.includes('pass.js')) {
            self.numPassScriptRequests++;
            return {cancel: false};
          } else if (details.url.includes('cancel.js')) {
            self.numCancelScriptRequests++;
            return {cancel: true};
          } else if (details.url === '%s') {
            self.numUUIDInPackagePassScriptRequests++;
            return {cancel: false};
          } else if (details.url === '%s') {
            self.numUUIDInPackageCancelScriptRequests++;
            return {cancel: true};
          }
        }, {urls: ['<all_urls>']}, [%s]);

        chrome.test.sendMessage('ready');
      )",
                                        pass_uuid_in_package_js_url.c_str(),
                                        cancel_uuid_in_package_js_url.c_str(),
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }
  std::string page_html = base::StringPrintf(
      R"(
        <title>Loaded</title>
        <body>
        <script>
        (() => {
          const wbn_url = new URL('./web_bundle.wbn', location.href).toString();
          const pass_js_url = new URL('./pass.js', location.href).toString();
          const cancel_js_url =
              new URL('./cancel.js', location.href).toString();
          const pass_uuid_in_package_js_url = '%s';
          const cancel_uuid_in_package_js_url = '%s';
          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({
            'source': wbn_url,
            'resources': [pass_js_url, cancel_js_url,
                          pass_uuid_in_package_js_url,
                          cancel_uuid_in_package_js_url]
          });
          document.body.appendChild(script);
        })();
        </script>
        </body>
      )",
      pass_uuid_in_package_js_url.c_str(),
      cancel_uuid_in_package_js_url.c_str());

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", page_html);
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  GURL pass_js_url = embedded_test_server()->GetURL("/pass.js");
  GURL cancel_js_url = embedded_test_server()->GetURL("/cancel.js");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      pass_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'script loaded';");
  builder.AddExchange(
      cancel_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}}, "");
  builder.AddExchange(
      pass_uuid_in_package_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'uuid-in-package script loaded';");
  builder.AddExchange(
      cancel_uuid_in_package_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}}, "");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());

  std::u16string expected_title1 = u"script loaded";
  content::TitleWatcher title_watcher1(web_contents, expected_title1);
  EXPECT_TRUE(TryLoadScript("pass.js"));
  // Check that the script in the web bundle is correctly loaded even when the
  // extension with blocking handler intercepted the request.
  EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());

  EXPECT_FALSE(TryLoadScript("cancel.js"));

  std::u16string expected_title2 = u"uuid-in-package script loaded";
  content::TitleWatcher title_watcher2(web_contents, expected_title2);
  EXPECT_TRUE(TryLoadScript(pass_uuid_in_package_js_url));
  // Check that the uuid-in-package script in the web bundle is correctly loaded
  // even when the extension with blocking handler intercepted the request.
  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());

  EXPECT_FALSE(TryLoadScript(cancel_uuid_in_package_js_url));

  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numPassScriptRequests"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numCancelScriptRequests"));
  EXPECT_EQ(
      1, GetCountFromBackgroundScript(
             extension, profile(), "self.numUUIDInPackagePassScriptRequests"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(
                   extension, profile(),
                   "self.numUUIDInPackageCancelScriptRequests"));
}

// Ensure web request API can change the headers of the subresources inside the
// web bundle.
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest, ChangeHeader) {
  // Create an extension that changes the header of the subresources inside the
  // web bundle.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  std::string opt_extra_info_spec = "'blocking', 'responseHeaders'";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
    opt_extra_info_spec += ", 'extraHeaders'";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        chrome.webRequest.onHeadersReceived.addListener(function(details) {
          if (!details.url.includes('target.txt')) {
            return {cancel: false};
          }
          const headers = details.responseHeaders;
          for (let i = 0; i < headers.length; i++) {
            if (headers[i].name.toLowerCase() == 'foo') {
              headers[i].value += '-changed';
            }
          }
          headers.push({name: 'foo', value:'inserted'});
          return {responseHeaders: headers};
        }, {urls: ['<all_urls>']}, [%s]);

        chrome.test.sendMessage('ready');
      )",
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }
  const char kPageHtml[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (async () => {
          const wbn_url = new URL('./web_bundle.wbn', location.href).toString();
          const target_url = new URL('./target.txt', location.href).toString();
          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({
            'source': wbn_url,
            'resources': [target_url]
          });
          document.body.appendChild(script);
          const res = await fetch(target_url);
          document.title = res.status + ':' + res.headers.get('foo');
        })();
        </script>
        </body>
      )";

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", kPageHtml);
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  GURL target_txt_url = embedded_test_server()->GetURL("/target.txt");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      target_txt_url,
      {{":status", "200"}, {"content-type", "text/plain"}, {"foo", "bar"}},
      "Hello world");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::u16string expected_title = u"200:bar-changed, inserted";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Ensure web request API can change the response headers of uuid-in-package:
// URL subresources inside the web bundle.
// Note: Currently we can't directly check the response headers of
// uuid-in-package: URL resources, because CORS requests are not allowed for
// such URLs. So we change the content-type header of a JavaScript file and
// monitor the error handler. Subresources in web bundles should be treated as
// if an artificial `X-Content-Type-Options: nosniff` header field is set. So
// when the content-type is not suitable for script execution, the script
// should fail to load.
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest,
                       ChangeHeaderUuidInPackageUrlResource) {
  std::string uuid_url = "uuid-in-package:71940cde-d20b-4fb5-b920-38a58a92c516";
  // Create an extension that changes the header of the subresources inside
  // the web bundle.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  std::string opt_extra_info_spec = "'blocking', 'responseHeaders'";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
    opt_extra_info_spec += ", 'extraHeaders'";
  static constexpr char kJsTemplate[] = R"(
        chrome.webRequest.onHeadersReceived.addListener(function(details) {
          if (details.url != '%s') {
            return;
          }
          const headers = details.responseHeaders;
          for (let i = 0; i < headers.length; i++) {
            if (headers[i].name.toLowerCase() == 'content-type') {
              headers[i].value = 'unknown/type';
            }
          }
          return {responseHeaders: headers};
        }, {urls: ['<all_urls>']}, [%s]);

        chrome.test.sendMessage('ready');
      )";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(kJsTemplate, uuid_url.c_str(),
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }
  static constexpr char kHtmlTemplate[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (async () => {
          const wbn_url = new URL('./web_bundle.wbn', location.href).toString();
          const uuid_url = '%s';
          const script_web_bundle = document.createElement('script');
          script_web_bundle.type = 'webbundle';
          script_web_bundle.textContent = JSON.stringify({
            'source': wbn_url,
            'resources': [uuid_url]
          });
          const script = document.createElement('script');
          script.src = uuid_url;
          script.addEventListener('error', () => {
            document.title = 'failed to load';
          });
          document.body.appendChild(script);
        })();
        </script>
        </body>
      )";
  std::string page_html = base::StringPrintf(kHtmlTemplate, uuid_url.c_str());

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", page_html);
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      uuid_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'loaded';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::u16string expected_title = u"failed to load";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Ensure web request API can redirect the requests for the subresources inside
// the web bundle.
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest,
                       RequestRedirected) {
  // Create an extension that redirects a bundle subresource request.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  std::string opt_extra_info_spec = "'blocking'";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
    opt_extra_info_spec += ", 'extraHeaders'";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        chrome.webRequest.onBeforeRequest.addListener(function(details) {
          if (details.url.includes('redirect.js')) {
            const redirectUrl =
                details.url.replace('redirect.js', 'redirected.js');
            return {redirectUrl: redirectUrl};
          } else if (details.url.includes('redirect_to_unlisted.js')) {
            const redirectUrl =
                details.url.replace('redirect_to_unlisted.js',
                                    'redirected_to_unlisted.js');
            return {redirectUrl: redirectUrl};
          } else if (details.url.includes('redirect_to_server.js')) {
            const redirectUrl =
                details.url.replace('redirect_to_server.js',
                                    'redirected_to_server.js');
            return {redirectUrl: redirectUrl};
          }
        }, {urls: ['<all_urls>']}, [%s]);

        chrome.test.sendMessage('ready');
      )",
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }
  const char kPageHtml[] = R"(
        <title>Loaded</title>
        <body>
        <script>
        (() => {
          const wbn_url = new URL('./web_bundle.wbn', location.href).toString();
          const redirect_js_url =
              new URL('./redirect.js', location.href).toString();
          const redirected_js_url =
              new URL('./redirected.js', location.href).toString();
          const redirect_to_unlisted_js_url =
              new URL('./redirect_to_unlisted.js', location.href).toString();
          const redirect_to_server =
              new URL('./redirect_to_server.js', location.href).toString();
          const script = document.createElement('script');
          script.type = 'webbundle';
          script.textContent = JSON.stringify({
            'source': wbn_url,
            'resources': [redirect_js_url, redirected_js_url,
                          redirect_to_unlisted_js_url, redirect_to_server]
          });
          document.body.appendChild(script);

        })();
        </script>
        </body>
      )";

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);
  RegisterRequestHandler("/test.html", "text/html", kPageHtml);
  RegisterRequestHandler("/redirect_to_server.js", "application/javascript",
                         "document.title = 'redirect_to_server';");
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  GURL redirect_js_url = embedded_test_server()->GetURL("/redirect.js");
  GURL redirected_js_url = embedded_test_server()->GetURL("/redirected.js");
  GURL redirect_to_unlisted_js_url =
      embedded_test_server()->GetURL("/redirect_to_unlisted.js");
  GURL redirected_to_unlisted_js_url =
      embedded_test_server()->GetURL("/redirected_to_unlisted.js");
  GURL redirect_to_server_js_url =
      embedded_test_server()->GetURL("/redirect_to_server.js");
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      redirect_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirect';");
  builder.AddExchange(
      redirected_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirected';");
  builder.AddExchange(
      redirect_to_unlisted_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirect_to_unlisted';");
  builder.AddExchange(
      redirected_to_unlisted_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirected_to_unlisted';");
  builder.AddExchange(
      redirect_to_server_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'redirect_to_server';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());
  {
    std::u16string expected_title = u"redirected";
    content::TitleWatcher title_watcher(web_contents, expected_title);
    EXPECT_TRUE(TryLoadScript("redirect.js"));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  {
    // In the current implementation, extensions can redirect the request to
    // the other resource in the web bundle even if the resource is not listed
    // in the resources attribute.
    std::u16string expected_title = u"redirected_to_unlisted";
    content::TitleWatcher title_watcher(web_contents, expected_title);
    EXPECT_TRUE(TryLoadScript("redirect_to_unlisted.js"));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
  // In the current implementation, extensions can't redirect the request to
  // outside the web bundle.
  EXPECT_FALSE(TryLoadScript("redirect_to_server.js"));
}

// Ensure that request to Subresource WebBundle fails if it is redirected by web
// request API.
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest,
                       WebBundleRequestRedirected) {
  // Create an extension that redirects "redirect.wbn" to "redirected.wbn".
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  std::string opt_extra_info_spec = "'blocking'";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders)
    opt_extra_info_spec += ", 'extraHeaders'";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        chrome.webRequest.onBeforeRequest.addListener(function(details) {
          if (!details.url.includes('redirect.wbn'))
            return;
          const redirectUrl =
              details.url.replace('redirect.wbn', 'redirected.wbn');
          return {redirectUrl};
        }, {urls: ['<all_urls>']}, [%s]);

        chrome.test.sendMessage('ready');
      )",
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/redirect.wbn", &web_bundle);
  RegisterWebBundleRequestHandler("/redirected.wbn", &web_bundle);
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  std::string js_url_str = embedded_test_server()->GetURL("/script.js").spec();
  web_package::WebBundleBuilder builder;
  builder.AddExchange(
      js_url_str,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'script loaded';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // In the current implementation, extensions can't redirect requests to
  // Subresource WebBundles.
  EXPECT_FALSE(TryLoadBundle("redirect.wbn", js_url_str));

  // Without redirection, bundle load should succeed.
  EXPECT_TRUE(TryLoadBundle("redirected.wbn", js_url_str));
}

// Ensure web request listener can intercept requests for web bundles with the
// resource type "webbundle".
IN_PROC_BROWSER_TEST_P(SubresourceWebBundlesWebRequestApiTest,
                       WebBundleRequestCanceledWithResourceType) {
  // Create an extension that cancels 'webbundle' resource type request in
  // chrome.webRequest.onBeforeRequest listener.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "Web Request Subresource Web Bundles Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  std::string opt_extra_info_spec = "'blocking'";
  if (GetExtraInfoSpec() == ExtraInfoSpec::kExtraHeaders) {
    opt_extra_info_spec += ", 'extraHeaders'";
  }
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        self.numOnBeforeRequestCalled = 0;
        self.unexpectedRequests = [];
        chrome.webRequest.onBeforeRequest.addListener(function(details) {
          self.numOnBeforeRequestCalled++;
          if (details.type != 'webbundle') {
            self.unexpectedRequests.push(details);
          }
          return {cancel: true};
        }, {urls: ['<all_urls>'], types: ['webbundle']}, [%s]);

        chrome.test.sendMessage('ready');
      )",
                                        opt_extra_info_spec.c_str()));
  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("ready");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  std::string web_bundle;
  RegisterWebBundleRequestHandler("/web_bundle.wbn", &web_bundle);

  std::string page_html = R"(
        <title>Page loaded</title>
        <body>
        <script>
          (() => {
            const script = document.createElement('script');
            script.type = 'webbundle';
            script.textContent = JSON.stringify({
              'source': 'web_bundle.wbn',
              'resources': ['cancel.js']
            });
            script.addEventListener('error', () => {
              document.title = 'web_bundle.wbn loading canceled';
            });
            script.addEventListener('load', () => {
              document.title = 'web_bundle.wbn loaded';
            });
            document.body.appendChild(script);
          })();
        </script>
        </body>
      )";
  RegisterRequestHandler("/test.html", "text/html", page_html);

  std::string pass_js = "document.title = 'pass.js loaded';";
  RegisterRequestHandler("/pass.js", "application/javascript", pass_js);

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a web bundle.
  web_package::WebBundleBuilder builder;
  GURL cancel_js_url = embedded_test_server()->GetURL("/cancel.js");
  builder.AddExchange(
      cancel_js_url,
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'cancel.js loaded';");
  std::vector<uint8_t> bundle = builder.CreateBundle();
  web_bundle = std::string(bundle.begin(), bundle.end());

  GURL page_url = embedded_test_server()->GetURL("/test.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(page_url, web_contents->GetLastCommittedURL());

  std::u16string expected_title1 = u"web_bundle.wbn loading canceled";
  content::TitleWatcher title_watcher1(web_contents, expected_title1);
  title_watcher1.AlsoWaitForTitle(u"web_bundle.wbn loaded");
  // Check that the request for web_bundle.wbn was correctly canceled.
  EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());

  // Check that the onBeforeRequest listener is called for the 'webbundle'
  // resource type request.
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numOnBeforeRequestCalled"));
  static constexpr char kScript[] =
      "chrome.test.sendScriptResult(JSON.stringify(self.unexpectedRequests));";
  EXPECT_EQ("[]",
            ExecuteScriptAndReturnString(extension->id(), profile(), kScript));

  // Try 'script' resource type request to check that the onBeforeRequest
  // listener is invoked only for a 'webbundle' resource type request.
  std::u16string expected_title2 = u"pass.js loaded";
  content::TitleWatcher title_watcher2(web_contents, expected_title2);
  EXPECT_TRUE(TryLoadScript("pass.js"));
  // Check that the pass.js was correctly loaded.
  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());

  // Check that the onBeforeRequest listener is not called for the 'script'
  // resource type request.
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "self.numOnBeforeRequestCalled"));
  EXPECT_EQ("[]",
            ExecuteScriptAndReturnString(extension->id(), profile(), kScript));
}

// TODO(crbug.com/40130781) When we implement variant matching of subresource
// web bundles, we should add test for request header modification.

enum class RedirectType {
  kOnBeforeRequest,
  kOnHeadersReceived,
};

struct RITestParams {
  RedirectType redirect_type;
  ContextType context_type;
};

class RedirectInfoWebRequestApiTest
    : public testing::WithParamInterface<RITestParams>,
      public ExtensionApiTest {
 public:
  RedirectInfoWebRequestApiTest() : ExtensionApiTest(GetParam().context_type) {
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }

  ~RedirectInfoWebRequestApiTest() override = default;
  RedirectInfoWebRequestApiTest(const RedirectInfoWebRequestApiTest&) = delete;
  RedirectInfoWebRequestApiTest& operator=(
      const RedirectInfoWebRequestApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  void InstallRequestRedirectingExtension(const std::string& resource_type) {
    TestExtensionDir test_dir;
    test_dir.WriteManifest(R"({
          "name": "Simple Redirect",
          "manifest_version": 2,
          "version": "0.1",
          "background": { "scripts": ["background.js"], "persistent": true },
          "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
        })");
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                       base::StringPrintf(R"(
          chrome.webRequest.%s.addListener(function(details) {
            if (details.type == '%s' &&
                details.url.includes('hello.html')) {
              var redirectUrl =
                  details.url.replace('original.test', 'redirected.test');
              return {redirectUrl: redirectUrl};
            }
          }, {urls: ['*://original.test/*']}, ['blocking']);
          chrome.test.sendMessage('ready');
        )",
                                          GetParam().redirect_type ==
                                                  RedirectType::kOnBeforeRequest
                                              ? "onBeforeRequest"
                                              : "onHeadersReceived",
                                          resource_type.c_str()));
    ExtensionTestMessageListener listener("ready");
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

 private:
  TestExtensionDir test_dir_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    PersistentBackgroundOnBeforeRequest,
    RedirectInfoWebRequestApiTest,
    ::testing::Values(RITestParams(RedirectType::kOnBeforeRequest,
                                   ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    PersistentBackgroundOnHeadersReceived,
    RedirectInfoWebRequestApiTest,
    ::testing::Values(RITestParams(RedirectType::kOnHeadersReceived,
                                   ContextType::kPersistentBackground)));

// These tests use webRequestBlocking and/or declarativeWebRequest.
// See crbug.com/332512510.
INSTANTIATE_TEST_SUITE_P(
    ServiceWorkerOnBeforeRequest,
    RedirectInfoWebRequestApiTest,
    ::testing::Values(RITestParams(RedirectType::kOnBeforeRequest,
                                   ContextType::kServiceWorkerMV2)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorkerOnHeadersReceived,
    RedirectInfoWebRequestApiTest,
    ::testing::Values(RITestParams(RedirectType::kOnHeadersReceived,
                                   ContextType::kServiceWorkerMV2)));

// Test that a main frame request redirected by an extension has the correct
// site_for_cookies and network_isolation_key parameters.
IN_PROC_BROWSER_TEST_P(RedirectInfoWebRequestApiTest,
                       VerifyRedirectInfoMainFrame) {
  InstallRequestRedirectingExtension("main_frame");

  content::URLLoaderMonitor monitor;

  // Navigate to the URL that should be redirected, and check that the extension
  // redirects it.
  GURL url = embedded_test_server()->GetURL("original.test", "/hello.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  GURL redirected_url =
      embedded_test_server()->GetURL("redirected.test", "/hello.html");
  EXPECT_EQ(redirected_url, web_contents->GetLastCommittedURL());

  // Check the parameters passed to the URLLoaderFactory.
  std::optional<network::ResourceRequest> resource_request =
      monitor.GetRequestInfo(redirected_url);
  ASSERT_TRUE(resource_request.has_value());
  EXPECT_TRUE(resource_request->site_for_cookies.IsFirstParty(redirected_url));
  ASSERT_TRUE(resource_request->trusted_params);
  url::Origin redirected_origin = url::Origin::Create(redirected_url);
  EXPECT_TRUE(
      resource_request->trusted_params->isolation_info.IsEqualForTesting(
          net::IsolationInfo::Create(
              net::IsolationInfo::RequestType::kMainFrame, redirected_origin,
              redirected_origin,
              net::SiteForCookies::FromOrigin(redirected_origin))));
}

// Test that a sub frame request redirected by an extension has the correct
// site_for_cookies and network_isolation_key parameters.
IN_PROC_BROWSER_TEST_P(RedirectInfoWebRequestApiTest,
                       VerifyBeforeRequestRedirectInfoSubFrame) {
  InstallRequestRedirectingExtension("sub_frame");

  content::URLLoaderMonitor monitor;

  // Navigate to page with an iframe that should be redirected, and check that
  // the extension redirects it.
  GURL original_iframed_url =
      embedded_test_server()->GetURL("original.test", "/hello.html");
  GURL page_with_iframe_url = embedded_test_server()->GetURL(
      "somewhere-else.test",
      net::test_server::GetFilePathWithReplacements(
          "/page_with_iframe.html",
          base::StringPairs{
              {"title1.html", original_iframed_url.spec().c_str()}}));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), page_with_iframe_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(page_with_iframe_url, web_contents->GetLastCommittedURL());

  content::RenderFrameHostWrapper child_frame(
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0));
  ASSERT_TRUE(child_frame);

  GURL redirected_url =
      embedded_test_server()->GetURL("redirected.test", "/hello.html");
  ASSERT_EQ(redirected_url, child_frame->GetLastCommittedURL());

  // Check the parameters passed to the URLLoaderFactory.
  std::optional<network::ResourceRequest> resource_request =
      monitor.GetRequestInfo(redirected_url);
  ASSERT_TRUE(resource_request.has_value());
  EXPECT_TRUE(
      resource_request->site_for_cookies.IsFirstParty(page_with_iframe_url));
  EXPECT_FALSE(resource_request->site_for_cookies.IsFirstParty(redirected_url));
  ASSERT_TRUE(resource_request->trusted_params);
  url::Origin top_level_origin = url::Origin::Create(page_with_iframe_url);
  url::Origin redirected_origin = url::Origin::Create(redirected_url);
  EXPECT_TRUE(
      resource_request->trusted_params->isolation_info.IsEqualForTesting(
          net::IsolationInfo::Create(
              net::IsolationInfo::RequestType::kSubFrame, top_level_origin,
              redirected_origin,
              net::SiteForCookies::FromOrigin(top_level_origin))));
}

// Regression test for crbug.com/1510422 to validate that redirection to an
// invalid URL by extension does not crash the browser.
IN_PROC_BROWSER_TEST_P(RedirectInfoWebRequestApiTest,
                       VerifyInvalidUrlRedirection) {
  TestExtensionDir test_dir;
  static constexpr char kInvalidUrl[] = "https://www.invalid.[ss]com/";
  test_dir.WriteManifest(R"({
        "name": "Simple Redirect",
          "manifest_version": 2,
          "version": "0.1",
          "background": { "scripts": ["background.js"], "persistent": true },
        "permissions": ["<all_urls>", "webRequest", "webRequestBlocking"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
        chrome.webRequest.onBeforeRequest.addListener(function(details) {
          if (details.url.includes('hello.html')) {
            var redirectUrl = '%s';
            return {redirectUrl: redirectUrl};
          }
        }, {urls: ['*://redirect.test/*']}, ['blocking']);
        chrome.test.sendMessage('ready');
      )",
                                        kInvalidUrl));

  // Since we can't catch the error in the extension's JS, we instead listen to
  // the error come into the error console.
  ErrorConsoleTestObserver error_observer(2u, profile());
  error_observer.EnableErrorCollection();

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to the URL that should be redirected, and check that the extension
  // navigation happens successfully.
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL url = embedded_test_server()->GetURL("redirect.test", "/hello.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  error_observer.WaitForErrors();
  const ErrorList& errors =
      ErrorConsole::Get(profile())->GetErrorsForExtension(extension->id());
  ASSERT_EQ(2u, errors.size());
  EXPECT_EQ(
      base::ASCIIToUTF16(base::StringPrintf(
          "Unchecked runtime.lastError: redirectUrl '%s' is not a valid URL.",
          kInvalidUrl)),
      errors[1]->message());
}

class ProxyCORSWebRequestApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ProxyCORSWebRequestApiTest() : ExtensionApiTest(GetParam()) {}
  ~ProxyCORSWebRequestApiTest() override = default;
  ProxyCORSWebRequestApiTest(const ProxyCORSWebRequestApiTest&) = delete;
  ProxyCORSWebRequestApiTest& operator=(const ProxyCORSWebRequestApiTest&) =
      delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(StartEmbeddedTestServer());
    proxy_cors_server_.RegisterRequestHandler(base::BindRepeating(
        &ProxyCORSWebRequestApiTest::HandleProxiedCORSRequest));
    ASSERT_TRUE(proxy_cors_server_.Start());

    PrefService* pref_service = browser()->profile()->GetPrefs();
    pref_service->SetDict(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateFixedServers(
                              proxy_cors_server_.host_port_pair().ToString(),
                              "accounts.google.com"));

    // Flush the proxy configuration change to avoid any races.
    ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
        ->FlushProxyConfigMonitorForTesting();
    profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleProxiedCORSRequest(const net::test_server::HttpRequest& request) {
    std::string request_url;
    // Request url with be replaced by host:port pair of embedded proxy server
    // in HttpRequest, extract requested url from request line instead.
    std::vector<std::string> request_lines =
        base::SplitString(request.all_headers, "\r\n", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (!request_lines.empty()) {
      std::vector<std::string> request_line =
          base::SplitString(request_lines[0], " ", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
      if (request_line.size() > 1) {
        request_url = request_line[1];
      }
    }
    if (request_url != kCORSUrl) {
      return nullptr;
    }

    // Handle request as proxy server.
    const auto proxy_auth = request.headers.find("Proxy-Authorization");
    std::string auth;
    if (proxy_auth != request.headers.end()) {
      auth = proxy_auth->second;
      const std::string auth_method_prefix = "Basic ";
      const auto prefix_pos = auth.find(auth_method_prefix);
      EXPECT_EQ(0U, prefix_pos);
      EXPECT_GT(auth.size(), auth_method_prefix.size());
      if (prefix_pos == 0U && auth.size() > auth_method_prefix.size()) {
        auth = auth.substr(auth_method_prefix.size());
        EXPECT_TRUE(base::Base64Decode(auth, &auth));
      } else {
        auth.clear();
      }
    }
    if (auth != base::StringPrintf("%s:%s", kCORSProxyUser, kCORSProxyPass)) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->AddCustomHeader("Proxy-Authenticate",
                                "Basic realm=\"TestRealm\"");
      response->set_code(net::HTTP_PROXY_AUTHENTICATION_REQUIRED);
      return response;
    }

    // Handle request as cors server.
    if (request.method == net::test_server::METHOD_OPTIONS) {
      const auto preflight_method =
          request.headers.find("Access-Control-Request-Method");
      const auto preflight_header =
          request.headers.find("Access-Control-Request-Headers");
      if (preflight_method == request.headers.end() ||
          preflight_header == request.headers.end()) {
        ADD_FAILURE() << "Expected Access-Control-Request-* headers were not "
                         "found in preflight request";
        std::unique_ptr<net::test_server::BasicHttpResponse> response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_BAD_REQUEST);
        return response;
      }
      EXPECT_EQ("GET", preflight_method->second);
      EXPECT_EQ(kCustomPreflightHeader, preflight_header->second);

      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      response->AddCustomHeader("Access-Control-Allow-Methods", "GET");
      response->AddCustomHeader("Access-Control-Allow-Headers",
                                kCustomPreflightHeader);
      response->set_code(net::HTTP_NO_CONTENT);
      return response;
    }
    EXPECT_EQ(net::test_server::METHOD_GET, request.method);

    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content_type("text/plain");
    response->set_content("PASS");
    response->set_code(net::HTTP_OK);
    return response;
  }

  net::EmbeddedTestServer proxy_cors_server_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ProxyCORSWebRequestApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ProxyCORSWebRequestApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Regression test for crbug.com/1212625
// Test that CORS preflight request which requires proxy auth completes
// successfully instead of being cancelled after proxy auth required response.
IN_PROC_BROWSER_TEST_P(ProxyCORSWebRequestApiTest,
                       PreflightCompletesSuccessfully) {
  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_cors_preflight"));
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionTestMessageListener preflight_listener("cors-preflight-succeeded");
  static constexpr char kCORSPreflightedRequest[] = R"(
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '%s');
      xhr.setRequestHeader('%s', 'testvalue');
      new Promise(resolve => {
        xhr.onload = () => {resolve(true);};
        xhr.onerror = () => {resolve(false);};
        xhr.send();
      });
      )";

  ExecuteScriptAsyncWithoutUserGesture(
      web_contents->GetPrimaryMainFrame(),
      base::StringPrintf(kCORSPreflightedRequest, kCORSUrl,
                         kCustomPreflightHeader));

  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));
  LoginHandler::GetAllLoginHandlersForTest().front()->SetAuth(
      base::ASCIIToUTF16(std::string(kCORSProxyUser)),
      base::ASCIIToUTF16(std::string(kCORSProxyPass)));

  EXPECT_TRUE(preflight_listener.WaitUntilSatisfied());
  EXPECT_EQ(1, GetCountFromBackgroundScript(
                   extension, profile(), "self.preflightHeadersReceivedCount"));
  EXPECT_EQ(
      1, GetCountFromBackgroundScript(extension, profile(),
                                      "self.preflightProxyAuthRequiredCount"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(
                   extension, profile(), "self.preflightResponseStartedCount"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(
                   extension, profile(),
                   "self.preflightResponseStartedSuccessfullyCount"));
}

class ExtensionWebRequestApiFencedFrameTest
    : public ExtensionWebRequestApiTest {
 protected:
  ExtensionWebRequestApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
    // Fenced frames are only allowed in secure contexts.
    UseHttpsTestServer();
  }
  ~ExtensionWebRequestApiFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiFencedFrameTest, Load) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest",
                               {.extension_url = "test_fenced_frames.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiFencedFrameTest,
                       DeclarativeSendMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest(
      "webrequest", {.extension_url = "test_fenced_frames_send_message.html"}))
      << message_;
}

class ExtensionWebRequestApiPrerenderingTest
    : public ExtensionWebRequestApiTest {
 protected:
  ExtensionWebRequestApiPrerenderingTest() = default;
  ~ExtensionWebRequestApiPrerenderingTest() override = default;

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiPrerenderingTest, Load) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest",
                               {.extension_url = "test_prerendering.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiPrerenderingTest, LoadIntoNewTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest(
      "webrequest", {.extension_url = "test_prerendering_into_new_tab.html"}))
      << message_;
}

// A clunky test suite class to allow for waiting for a message to be sent from
// the extension's background context when it starts up. We need this because
// we don't currently have a good way of waiting for a service worker context to
// be fully initialized.
class WebRequestPersistentListenersTest
    : public ExtensionWebRequestApiTestWithContextType {
 public:
  WebRequestPersistentListenersTest()
      // Note: Set the listener before triggering the parent
      // SetUpOnMainThread to ensure it happens before extensions start
      // loading.
      : test_listener_(
            std::make_unique<ExtensionTestMessageListener>("ready")) {}
  ~WebRequestPersistentListenersTest() override = default;

  void TearDownOnMainThread() override {
    test_listener_.reset();
    ExtensionWebRequestApiTestWithContextType::TearDownOnMainThread();
  }

  void WaitForReadyMessage() {
    EXPECT_TRUE(test_listener_->WaitUntilSatisfied());
  }

 private:
  std::unique_ptr<ExtensionTestMessageListener> test_listener_;
};

namespace {

constexpr char kGetNumRequests[] =
    R"((async function() {
         // Wait for any pending storage writes to complete.
         await flushStorage();
         chrome.storage.local.get(
             {requestCount: -1},
             (result) => {
               chrome.test.sendScriptResult(result.requestCount);
             });
       })();)";

}  // namespace

// Tests that webRequest listeners are persistent across browser restarts.
IN_PROC_BROWSER_TEST_P(WebRequestPersistentListenersTest,
                       PRE_TestListenersArePersistent) {
  // Load an extension that listens for webRequest events.
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_persistent"));
  ASSERT_TRUE(extension);

  // Navigate to example.com (a site the extension has access to).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  // Validate that we have a single request seen by the extension.
  base::Value request_count = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), kGetNumRequests,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  ASSERT_TRUE(request_count.is_int());
  EXPECT_EQ(1, request_count.GetInt());
}

IN_PROC_BROWSER_TEST_P(WebRequestPersistentListenersTest,
                       TestListenersArePersistent) {
  // Find the installed extension and wait for it to fully load.
  ASSERT_TRUE(StartEmbeddedTestServer());
  const Extension* extension = nullptr;
  for (const auto& candidate : extension_registry()->enabled_extensions()) {
    if (candidate->name() == "Web Request Persistence") {
      extension = candidate.get();
      break;
    }
  }
  ASSERT_TRUE(extension);
  WaitForExtensionViewsToLoad();
  WaitForReadyMessage();

  // Navigate once more to example.com.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  // We should now have two records seen by the extension.
  base::Value request_count = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), kGetNumRequests,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  ASSERT_TRUE(request_count.is_int());
  EXPECT_EQ(2, request_count.GetInt());
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    WebRequestPersistentListenersTest,
    ::testing::Values(
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kPersistentBackground,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());
INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    WebRequestPersistentListenersTest,
    ::testing::Values(
        std::make_pair(
            ContextType::kServiceWorker,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchEnabled),
        std::make_pair(
            ContextType::kServiceWorker,
            BackgroundResourceFetchTestCase::kBackgroundResourceFetchDisabled)),
    ExtensionWebRequestApiTestWithContextType::PrintToStringParamName());

class ManifestV3WebRequestApiTest : public ExtensionWebRequestApiTest {
 public:
  ManifestV3WebRequestApiTest() = default;
  ~ManifestV3WebRequestApiTest() override = default;

  // Loads an extension contained within `test_dir` as a policy-installed
  // extension. This is useful because webRequestBlocking is restricted to
  // policy-installed extensions in Manifest V3.
  // This assumes the extension script will send a "ready" message once it's
  // done setting up.
  const Extension* LoadPolicyExtension(TestExtensionDir& test_dir) {
    // We need a "ready"-style listener here because `InstallExtension()`
    // doesn't automagically wait for the extension to finish setting up.
    ExtensionTestMessageListener listener("ready");
    // Since we may programmatically stop the worker, we also need to wait for
    // the registration to be fully stored.
    service_worker_test_utils::TestServiceWorkerContextObserver
        registration_observer(profile());
    base::FilePath packed_path = test_dir.Pack();
    const Extension* extension = InstallExtension(
        packed_path, 1, mojom::ManifestLocation::kExternalPolicyDownload);
    EXPECT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    registration_observer.WaitForRegistrationStored();

    return extension;
  }

  WebRequestEventRouter* web_request_router() {
    return WebRequestEventRouter::Get(profile());
  }
};

// Tests a service worker-based extension intercepting requests with
// webRequestBlocking.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest, WebRequestBlocking) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "webRequestBlocking"],
           "host_permissions": [
             "http://block.example/*",
             "http://allow.example/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  // An extension with a listener that cancels any requests that include
  // block.example.
  static constexpr char kBackgroundJs[] =
      R"(chrome.webRequest.onBeforeRequest.addListener(
             (details) => {
               if (details.url.includes('block.example')) {
                 return {cancel: true}
               }
               return {};
             },
             {urls: ['<all_urls>'], types: ['main_frame']},
             ['blocking']);
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadPolicyExtension(test_dir);
  ASSERT_TRUE(extension);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to allow.example. This should succeed.
  {
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("allow.example", "/simple.html")));
    EXPECT_EQ(net::OK, nav_observer.last_net_error_code());
  }

  // Now, navigate to block.example. This navigation should be blocked.
  {
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("block.example", "/simple.html")));
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
  }
}

// Tests a service worker-based extension registering multiple webRequest events
// in multiple contexts. This ensures the subevent name logic for service worker
// extensions doesn't result in any collisions of listener IDs, similar to the
// issue found in https://crbug.com/1297276.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       MultipleListenersAndContexts) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "storage"],
           "host_permissions": [
             "http://first.example/*",
             "http://second.example/*",
             "http://third.example/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  // The extension has two contexts: the background service worker (which
  // registers two listeners) and a separate page (which also registers a
  // listener). This ensures that a) service worker listeners do not conflict
  // with each other and b) service worker listeners do not conflict with
  // listeners registered in other contexts.
  static constexpr char kBackgroundJs[] =
      R"(self.firstCount = 0;
         self.secondCount = 0;
         function firstListener() { ++firstCount; }
         function secondListener() { ++secondCount; }
         chrome.webRequest.onBeforeRequest.addListener(
             firstListener,
             {urls: ['http://first.example/*'], types: ['main_frame']}, []);
         chrome.webRequest.onBeforeRequest.addListener(
             secondListener,
             {urls: ['http://second.example/*'], types: ['main_frame']}, []);)";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
          <html>
            Page
            <script src="page.js"></script>
          </html>)";
  static constexpr char kPageJs[] =
      R"(self.thirdCount = 0;
         function thirdListener() { ++thirdCount; }
         chrome.webRequest.onBeforeRequest.addListener(
             thirdListener,
             {urls: ['http://third.example/*'], types: ['main_frame']}, []);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Load the page with the extension listeners.
  content::RenderFrameHost* page_host = ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html"));
  ASSERT_TRUE(page_host);

  // At this point, 3 listeners should be registered.
  EXPECT_EQ(3u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Convenience lambdas for checking the count received in each listener.
  auto get_first_count = [this, extension]() {
    return GetCountFromBackgroundScript(extension, profile(), "firstCount");
  };
  auto get_second_count = [this, extension]() {
    return GetCountFromBackgroundScript(extension, profile(), "secondCount");
  };
  auto get_third_count = [page_host]() {
    return content::EvalJs(page_host, "window.thirdCount;").ExtractInt();
  };

  // No listeners should have fired yet.
  EXPECT_EQ(0, get_first_count());
  EXPECT_EQ(0, get_second_count());
  EXPECT_EQ(0, get_third_count());

  // Navigate to first.example (this first navigation needs to happen in a new
  // tab so that we don't navigate the extension page).
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL("first.example", "/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // (Only) the first listener should have fired.
  EXPECT_EQ(1, get_first_count());
  EXPECT_EQ(0, get_second_count());
  EXPECT_EQ(0, get_third_count());

  // Navigate to second.example. The second listener should fire.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("second.example", "/title1.html")));
  EXPECT_EQ(1, get_first_count());
  EXPECT_EQ(1, get_second_count());
  EXPECT_EQ(0, get_third_count());

  // Navigate to third.example. The third listener should fire.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("third.example", "/title1.html")));
  EXPECT_EQ(1, get_first_count());
  EXPECT_EQ(1, get_second_count());
  EXPECT_EQ(1, get_third_count());
}

// Tests that a service worker-based extension with webRequestBlocking can
// intercept requests after the service worker stops.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       WebRequestBlocking_AfterWorkerShutdown) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "webRequestBlocking"],
           "host_permissions": [
             "http://block.example/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  // An extension with a listener that cancels any requests that include
  // block.example.
  static constexpr char kBackgroundJs[] =
      R"(chrome.webRequest.onBeforeRequest.addListener(
             (details) => {
               if (details.url.includes('block.example')) {
                 return {cancel: true}
               }
               return {};
             },
             {urls: ['<all_urls>'], types: ['main_frame']},
             ['blocking']);
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadPolicyExtension(test_dir);
  ASSERT_TRUE(extension);

  // A single webRequest listener should be registered.
  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // Note: the task to remove listeners from ExtensionWebRequestEventRouter
  // is async; run to flush the posted task.
  base::RunLoop().RunUntilIdle();

  // The listener should still be registered, but should be counted as an
  // inactive listener.
  EXPECT_EQ(0u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(1u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Navigate to block.example. The request should be blocked by the extension.
  {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("block.example", "/simple.html")));
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
  }
}

// Tests a service worker-based extension using webRequest for observational
// purposes receives events after the worker stops.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       WebRequestObservation_AfterWorkerShutdown) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "storage"],
           "host_permissions": [
             "http://example.com/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  // An extension that stores the number of matched requests in a count in
  // extension storage.
  // This is very similar to the test extension at
  // chrome/test/data/extensions/api_test/webrequest_persistent, but is
  // manifest V3. There's enough changes that our loading auto-conversion code
  // won't quite work (mostly around permissions vs host_permissions), so we
  // need a bit of a duplication here.
  static constexpr char kBackgroundJs[] =
      R"(let storageComplete = undefined;
         let isUsingStorage = false;

         // Waits for any pending load to complete to avoid raciness in the
         // test.
         async function flushStorage() {
           console.assert(!storageComplete);
           if (!isUsingStorage)
             return;
           await new Promise((resolve) => {
             storageComplete = resolve;
           });
           storageComplete = undefined;
         }

         chrome.webRequest.onBeforeRequest.addListener(
             async (details) => {
               isUsingStorage = true;
               let {requestCount} =
                   await chrome.storage.local.get({requestCount: 0});
               requestCount++;
               await chrome.storage.local.set({requestCount});
               isUsingStorage = false;
               if (storageComplete)
                 storageComplete();
               chrome.test.sendMessage('event received');
             },
             {urls: ['<all_urls>'], types: ['main_frame']});)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  // A single listener should be registered.
  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Navigate to a URL. The request should be seen by the extension.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  auto get_request_count = [this, extension]() {
    base::Value request_count = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), kGetNumRequests,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    return request_count.GetInt();
  };

  EXPECT_EQ(1, get_request_count());

  // Stop the extension's service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // Note: the task to remove listeners from ExtensionWebRequestEventRouter
  // is async; run to flush the posted task.
  base::RunLoop().RunUntilIdle();

  // The listener should still be registered, but should be counted as an
  // inactive listener.
  EXPECT_EQ(0u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(1u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  {
    // Navigate again. The request should again be seen by the extension.
    //
    // We need to use a message listener here to ensure we gave the extension
    // enough time to start up and have the event fire. Unlike the blocking
    // scenario, there's no guarantee this happens by the time navigation
    // completes.
    ExtensionTestMessageListener listener("event received");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("example.com", "/simple.html")));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  // The inactive listener should have been reactivated...
  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // ... and the extension should have seen the request.
  EXPECT_EQ(2, get_request_count());
}

// Tests unloading an extension with lazy listeners while the worker is
// inactive. The listeners should be properly cleaned up.
IN_PROC_BROWSER_TEST_F(
    ManifestV3WebRequestApiTest,
    ServiceWorkerWithWebRequest_UnloadExtensionWhileWorkerInactive) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest"],
           "host_permissions": [
             "http://example.com/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.webRequest.onBeforeRequest.addListener(
             async (details) => { },
             {urls: ['<all_urls>'], types: ['main_frame']});)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // Note: the task to remove listeners from ExtensionWebRequestEventRouter
  // is async; run to flush the posted task.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(1u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  DisableExtension(extension->id());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
}

// Tests a service worker adding and then removing a listener.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       ServiceWorkerWithWebRequest_ManuallyRemoveListener) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest"],
           "host_permissions": [
             "http://example.com/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  static constexpr char kBackgroundJs[] =
      R"(self.firstListenerCount = 0;
         self.secondListenerCount = 0;
         self.firstListener = function() { ++firstListenerCount; };
         self.secondListener = function() { ++secondListenerCount; };
         chrome.webRequest.onBeforeRequest.addListener(
             firstListener,
             {urls: ['<all_urls>'], types: ['main_frame']});
         chrome.webRequest.onBeforeRequest.addListener(
             secondListener,
             {urls: ['<all_urls>'], types: ['main_frame']});)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);

  // There should initially be two listeners registered, both active (since
  // the service worker is active).
  EXPECT_EQ(2u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Manually remove one of the listeners. This should result in the listener
  // being fully removed (not deactivated), so there should only be a single
  // listener remaining.
  static constexpr char kRemoveListener[] =
      R"(chrome.webRequest.onBeforeRequest.removeListener(self.firstListener);
         chrome.test.sendScriptResult('');)";
  BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), kRemoveListener,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  // Note: the task to remove listeners from ExtensionWebRequestEventRouter
  // is async; run to flush the posted task.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Navigate to a page and verify that only the second listener fires.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));

  EXPECT_EQ(0, GetCountFromBackgroundScript(extension, profile(),
                                            "firstListenerCount"));
  EXPECT_EQ(1, GetCountFromBackgroundScript(extension, profile(),
                                            "secondListenerCount"));
}

// Tests listeners in multiple contexts with lazy event disptaching.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       ListenersInMultipleContextsWithLazyDispatch) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest"],
           "host_permissions": [ "http://example.com/*" ],
           "background": {"service_worker": "background.js"}
         })";
  // The extension has two contexts: the background service worker and a
  // separate page, each of which register an identical listener. Each should
  // only be invoked once.
  static constexpr char kBackgroundJs[] =
      R"(self.eventCount = 0;
         chrome.webRequest.onBeforeRequest.addListener(
             async function() {
               ++eventCount;
               // Perform a rount trip to ensure any events that are coming our
               // way get dispatched, and then notify the test.
               await chrome.test.waitForRoundTrip('test');
               chrome.test.sendMessage('worker received');
             },
             {urls: ['http://example.com/*'], types: ['main_frame']}, []);)";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
          <html>
            Page
            <script src="page.js"></script>
          </html>)";
  static constexpr char kPageJs[] =
      R"(self.eventCount = 0;
         chrome.webRequest.onBeforeRequest.addListener(
             function() { ++eventCount; },
             {urls: ['http://example.com/*'], types: ['main_frame']}, []);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kPageJs);
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.wait_for_registration_stored = true});

  ASSERT_TRUE(extension);

  // Load the page with the extension listeners.
  content::RenderFrameHost* page_host = ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html"));
  ASSERT_TRUE(page_host);

  // At this point, 2 listeners should be registered.
  EXPECT_EQ(2u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Convenience lambdas for checking the count received in each listener.
  auto get_worker_event_count = [this, extension]() {
    return GetCountFromBackgroundScript(extension, profile(), "eventCount");
  };
  auto get_page_event_count = [page_host]() {
    return content::EvalJs(page_host, "self.eventCount;").ExtractInt();
  };

  // Stop the extension's service worker. The worker listener should now be
  // registered as an inactive listener.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // Note: the task to remove listeners from ExtensionWebRequestEventRouter
  // is async; run to flush the posted task.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(1u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  {
    ExtensionTestMessageListener listener("worker received");
    // Navigate to example.com (this navigation needs to happen in a new tab so
    // that we don't navigate the extension page).
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        embedded_test_server()->GetURL("example.com", "/title1.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  // Each listener should have fired exactly once.
  EXPECT_EQ(1, get_worker_event_count());
  EXPECT_EQ(1, get_page_event_count());
}

// Tests that an MV3 extension can use the `webRequestAuthProvider` permission
// to intercept and handle `onAuthRequired` events coming from a tab.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest, TestOnAuthRequiredTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "webRequestAuthProvider"],
           "host_permissions": [ "http://example.com/*" ],
           "background": {"service_worker": "background.js"}
         })";
  // The extension will asynchronously provide the user credentials for the
  // request.
  static constexpr char kBackgroundJs[] =
      R"(chrome.webRequest.onAuthRequired.addListener(
             (details, callback) => {
               chrome.test.assertEq('mv3authprovider', details.realm);
               chrome.test.assertEq(401, details.statusCode);
               const authCredentials = {username: 'foo', password: 'secret'};
               setTimeout(() => {
                 callback({authCredentials});
                 chrome.test.succeed();
               }, 20);
             },
             {urls: ['<all_urls>']},
             ['asyncBlocking']);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());

  ASSERT_TRUE(extension);

  // Navigate to a special URL that will prompt the user for credentials. The
  // request should succeed (verified by the last navigation status) and the
  // extension should have received the event (verified by the ResultCatcher).
  static constexpr char kRealm[] = "mv3authprovider";
  std::string auth_url_path =
      base::StringPrintf("/auth-basic/%s/subpath?realm=%s", kRealm, kRealm);
  GURL auth_url = embedded_test_server()->GetURL("example.com", auth_url_path);

  ResultCatcher result_catcher;
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  content::RenderFrameHost* frame_host =
      ui_test_utils::NavigateToURL(browser(), auth_url);
  ASSERT_TRUE(result_catcher.GetNextResult());
  EXPECT_EQ(auth_url, frame_host->GetLastCommittedURL());
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
}

class OnAuthRequiredApiTest : public ExtensionApiTest {
 public:
  static constexpr char kTestDomain[] = "a.test";
  OnAuthRequiredApiTest() {
    // Https is required to use service workers.
    // This limits the set of domains with valid certificates. For the purposes
    // of this test we will use kTestDomain.
    UseHttpsTestServer();
  }
  ~OnAuthRequiredApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // Returns a URL which requires username/password
  GURL MakeAuthUrl() {
    static constexpr char kRealm[] = "mv3authprovider";
    std::string auth_url_path =
        base::StringPrintf("/auth-basic/%s/subpath?realm=%s", kRealm, kRealm);
    return embedded_test_server()->GetURL(kTestDomain, auth_url_path);
  }

  // Loads an extension that implements onAuthRequired. `additional_js` will be
  // concatenated to the background.js.
  void LoadExtensionWithAdditionalJs(const std::string& additional_js) {
    static constexpr char kManifest[] =
        R"({
             "name": "MV3 WebRequest",
             "version": "0.1",
             "manifest_version": 3,
             "permissions": ["webRequest", "webRequestAuthProvider"],
             "host_permissions": [ "http://127.0.0.1/*", "https://a.test/*" ],
             "background": {"service_worker": "background.js"}
           })";
    static constexpr char kBackgroundJs[] =
        R"(
            let didInterceptAuth = false;
            chrome.webRequest.onAuthRequired.addListener(
               (details, callback) => {
                 didInterceptAuth = true;
                 chrome.test.assertEq('mv3authprovider', details.realm);
                 chrome.test.assertEq(401, details.statusCode);
                 const authCredentials = {username: 'foo', password: 'secret'};
                 callback({authCredentials});
               },
               {urls: ['<all_urls>']},
               ['asyncBlocking']);
          )";
    std::string background_js(kBackgroundJs);
    background_js += additional_js;

    test_extension_dir_.WriteManifest(kManifest);
    test_extension_dir_.WriteFile(FILE_PATH_LITERAL("background.js"),
                                  background_js);

    const Extension* extension =
        LoadExtension(test_extension_dir_.UnpackedPath());
    ASSERT_TRUE(extension);
  }

 private:
  TestExtensionDir test_extension_dir_;
  base::ScopedTempDir service_worker_dir_;
};

// Tests that an MV3 extension can use the `webRequestAuthProvider` permission
// to intercept and handle `onAuthRequired` events coming from an extension
// service worker. This test does the following:
//   (1) This loads an extension with a service-worker background.js.
//   (2) The extension adds a listener to chrome.webRequest.onAuthRequired.
//   (3) The extension attempts to fetch a resource that requires http auth.
//   (4) This triggers the listener in (3), which supplies credentials
//   (5) Checks that the fetch succeeded.
IN_PROC_BROWSER_TEST_F(OnAuthRequiredApiTest,
                       TestOnAuthRequiredExtensionServiceWorker) {
  // After the extension loads, trigger an async request to fetch an http auth
  // resource.
  std::string additional_js =
      R"(
          (async function() {
            try {
              const response = await fetch($1);
              if (response.ok) {
                 chrome.test.assertTrue(didInterceptAuth);
                 chrome.test.succeed();
              } else {
                 chrome.test.fail();
              }
            } catch (e) {
              chrome.test.fail();
            }
          })();
        )";
  additional_js = content::JsReplace(additional_js, MakeAuthUrl());

  // Loading the extension triggers the remaining steps of the test.
  ResultCatcher result_catcher;
  LoadExtensionWithAdditionalJs(additional_js);

  ASSERT_TRUE(result_catcher.GetNextResult());
}

// This test is similar to TestOnAuthRequiredExtensionServiceWorker but the
// service worker is hosted by a website instead of the extension istelf.
IN_PROC_BROWSER_TEST_F(OnAuthRequiredApiTest,
                       TestOnAuthRequiredWebsiteServiceWorker) {
  // Load the extension.
  LoadExtensionWithAdditionalJs("");

  // Navigate to the test page.
  GURL requestor_url = embedded_test_server()->GetURL(
      kTestDomain, "/ssl/service_worker_fetch/page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requestor_url));

  // Perform a fetch from a worker and validate that it succeeds.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string fetch_response =
      content::EvalJs(web_contents,
                      content::JsReplace("doFetchInWorker($1);", MakeAuthUrl()))
          .ExtractString();
  EXPECT_THAT(fetch_response, testing::HasSubstr("<title>"));
}

// Tests the behavior of an extension that registers an event listener
// asynchronously.
// Regression test for https://crbug.com/1397879 and https://crbug.com/1434212.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest, AsyncListenerRegistration) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "webRequestBlocking"],
           "host_permissions": [
             "http://example.com/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  // A background context that *conditionally* registers a blocking listener.
  // We send a "will_register" message and register the listener once we receive
  // the response from that message. If we never receive a response, we never
  // register the event listener.
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.sendMessage('will_register').then(() => {
           chrome.webRequest.onBeforeRequest.addListener(
               (details) => {
                 if (details.url.includes('example.com')) {
                   return {cancel: true}
                 }
                 return {};
               },
               {urls: ['<all_urls>'], types: ['main_frame']},
               ['blocking']);
           chrome.test.sendMessage('registered');
         });
         // Register an additional event properly so that the service worker
         // still has _a_ listener registered in the process.
         // https://crbug.com/1434212.
         chrome.webRequest.onHeadersReceived.addListener(
             (details) => {},
             {urls: ['<all_urls>'], types: ['main_frame']},
             ['blocking']);
         chrome.test.sendMessage('ready');)";

  // Load the extension and tell it to register the listener.
  ExtensionTestMessageListener will_register_listener(
      "will_register", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener registered_listener("registered");
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadPolicyExtension(test_dir);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(will_register_listener.WaitUntilSatisfied());
  EXPECT_FALSE(registered_listener.was_satisfied());
  will_register_listener.Reply("Go for it!");
  ASSERT_TRUE(registered_listener.WaitUntilSatisfied());

  // A single webRequest listener should be registered.
  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(0u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  // Navigate to example.com to check our setup; the request should be blocked.
  {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
  }

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());
  // Note: the task to remove listeners from ExtensionWebRequestEventRouter
  // is async; run to flush the posted task.
  base::RunLoop().RunUntilIdle();

  // The listener should still be registered, but should be counted as an
  // inactive listener.
  EXPECT_EQ(0u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
  EXPECT_EQ(1u, web_request_router()->GetInactiveListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Reset the "will register" listener. However, we'll never reply this time,
  // which means the extension will never register the listener again.
  will_register_listener.Reset();

  // Now, navigate to example.com again. This will wake up the extension service
  // worker, but we'll fail to dispatch the event to the extension because the
  // listener isn't registered. The request should be allowed to continue.
  {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, nav_observer.last_net_error_code());
  }

  // Clean up: ExtensionTestMessageListener requires a reply (or else will
  // DCHECK). Wait for it to receive the message (it probably already did, but
  // theoretically can race), and send a response.
  EXPECT_TRUE(will_register_listener.WaitUntilSatisfied());
  will_register_listener.Reply("unused");
}

// Tests behavior when a service worker is stopped while processing an event.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       ServiceWorkerGoesAwayWhileHandlingRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest", "webRequestBlocking"],
           "host_permissions": [
             "http://example.com/*"
           ],
           "background": {"service_worker": "background.js"}
         })";
  // An extension with a listener that will spin forever on example.com
  // requests.
  static constexpr char kBackgroundJs[] =
      R"(chrome.webRequest.onBeforeRequest.addListener(
             (details) => {
               if (details.url.includes('example.com')) {
                 chrome.test.sendMessage('received');
                 // Spin FOREVER.
                 while (true) { }
               }
               return {};
             },
             {urls: ['<all_urls>'], types: ['main_frame']},
             ['blocking']);
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadPolicyExtension(test_dir);
  ASSERT_TRUE(extension);

  // A single webRequest listener should be registered.
  EXPECT_EQ(1u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));

  // Navigate to example.com; the extension will receive the event and spin
  // indefinitely.
  // We navigate in a new tab to have a better signal of "request started".
  // We can't wait for the request to finish, since the extension's listener
  // never returns, which blocks the request.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::TestNavigationObserver nav_observer(url);
  nav_observer.StartWatchingNewWebContents();
  ExtensionTestMessageListener test_listener("received");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  // The web contents should still be loading, and should have no last
  // committed URL since the extension is blocking the request.
  EXPECT_TRUE(web_contents->IsLoading());
  EXPECT_EQ(GURL(), web_contents->GetLastCommittedURL());

  // Stop the extension service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());

  // The request should be unblocked.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
}

// Tests that a MV3 extension that doesn't have the `webRequestAuthProvider`
// permission cannot use blocking listeners for `onAuthRequired`.
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       TestOnAuthRequired_NoPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest"],
           "host_permissions": [ "http://example.com/*" ],
           "background": {"service_worker": "background.js"}
         })";
  // The extension tries to add a listener; this will fail asynchronously
  // as a part of the webRequestInternal API trying to add the listener.
  // This results in runtime.lastError being set, but since it's an
  // internal API, there's no way for the extension to catch the error.
  static constexpr char kBackgroundJs[] =
      R"(chrome.webRequest.onAuthRequired.addListener(
             (details, callback) => {},
             {urls: ['<all_urls>']},
             ['asyncBlocking']);)";

  // Since we can't catch the error in the extension's JS, we instead listen to
  // the error come into the error console.
  ErrorConsoleTestObserver error_observer(1u, profile());
  error_observer.EnableErrorCollection();

  // Load the extension and wait for the error to come.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());

  ASSERT_TRUE(extension);
  error_observer.WaitForErrors();

  const ErrorList& errors =
      ErrorConsole::Get(profile())->GetErrorsForExtension(extension->id());
  ASSERT_EQ(1u, errors.size());
  EXPECT_TRUE(
      base::StartsWith(errors[0]->message(),
                       u"Unchecked runtime.lastError: You do not have "
                       u"permission to use blocking webRequest listeners."))
      << errors[0]->message();
}

// Tests that an extension that doesn't have the `webView` permission cannot
// manually create and add a WebRequestEvent that specifies a webViewInstanceId.
// TODO(tjudkins): It would be good to also stop this on the JS layer by not
// allowing extensions to manually create and add WebRequestEvents.
// Regression test for crbug.com/1472830
IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest,
                       TestWebviewIdSpecifiedOnEvent_NoPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "permissions": ["webRequest"],
           "host_permissions": [ "http://example.com/*" ],
           "background": {"service_worker": "background.js"}
         })";
  // The extension tries to add a listener; this will fail asynchronously
  // as a part of the webRequestInternal API trying to add the listener.
  // This results in runtime.lastError being set, but since it's an
  // internal API, there's no way for the extension to catch the error.
  static constexpr char kBackgroundJs[] =
      R"(let event = new chrome.webRequest.onBeforeRequest.constructor(
             'webRequest.onBeforeRequest',
             undefined,
             undefined,
             undefined,
             1); // webViewInstanceId
         event.addListener(() => {},
         {urls: ['*://*.example.com/*']});)";

  // Since we can't catch the error in the extension's JS, we instead listen to
  // the error come into the error console.
  ErrorConsoleTestObserver error_observer(1u, profile());
  error_observer.EnableErrorCollection();

  // Load the extension and wait for the error to come.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());

  ASSERT_TRUE(extension);
  error_observer.WaitForErrors();

  const ErrorList& errors =
      ErrorConsole::Get(profile())->GetErrorsForExtension(extension->id());
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ(u"Unchecked runtime.lastError: Missing webview permission.",
            errors[0]->message());
  EXPECT_EQ(0u, web_request_router()->GetListenerCountForTesting(
                    profile(), "webRequest.onBeforeRequest"));
}

IN_PROC_BROWSER_TEST_F(ManifestV3WebRequestApiTest, RecordUkmOnNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  TestExtensionDir test_dir1;
  test_dir1.WriteManifest(R"({
           "name": "MV3 WebRequest",
           "version": "0.1",
           "manifest_version": 3,
           "content_scripts": [
             {
               "matches": ["<all_urls>"],
               "js": ["contentscript.js"]
             }
           ],
           "permissions": [
             "webRequest",
             "webRequestBlocking",
             "webRequestAuthProvider",
             "declarativeNetRequest",
             "declarativeNetRequestFeedback",
             "declarativeNetRequestWithHostAccess"
           ],
           "host_permissions": ["http://a.com/*"],
           "background": {"service_worker": "background.js"}
         })");
  test_dir1.WriteFile(FILE_PATH_LITERAL("contentscript.js"), /*contents=*/"");
  test_dir1.WriteFile(FILE_PATH_LITERAL("background.js"),
                      "chrome.test.sendMessage('ready');");
  ASSERT_TRUE(LoadPolicyExtension(test_dir1));

  // declarativeWebRequest is only supported by manifest version 2 or lower.
  TestExtensionDir test_dir2;
  test_dir2.WriteManifest(R"({
           "name": "MV2 WebRequest",
           "version": "0.1",
           "manifest_version": 2,
           "content_scripts": [
             {
               "matches": ["<all_urls>"],
               "js": ["contentscript.js"]
             }
           ],
           "permissions": [
             "declarativeWebRequest",
             "http://b.com/*"
           ],
           "background": {"scripts": ["background.js"], "persistent": true}
         })");
  test_dir2.WriteFile(FILE_PATH_LITERAL("contentscript.js"), /*contents=*/"");
  test_dir2.WriteFile(FILE_PATH_LITERAL("background.js"),
                      "chrome.test.sendMessage('ready');");
  ExtensionTestMessageListener listener("ready");
  ASSERT_TRUE(LoadExtension(test_dir2.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  base::RunLoop ukm_loop;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Extensions_OnNavigation::kEntryName,
      base::BindLambdaForTesting([&]() {
        if (ukm_recorder
                .GetMergedEntriesByName(
                    ukm::builders::Extensions_OnNavigation::kEntryName)
                .size() == 2) {
          ukm_loop.Quit();
        }
      }));

  const GURL kUrlA = embedded_test_server()->GetURL("a.com", "/simple.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrlA));

  const GURL kUrlB = embedded_test_server()->GetURL("b.com", "/simple.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrlB));

  // Waits until UKM data is recorded.
  ukm_loop.Run();

  const double kBucketSpacing = 2;
  auto merged_entries = ukm_recorder.GetMergedEntriesByName(
      ukm::builders::Extensions_OnNavigation::kEntryName);
  EXPECT_EQ(2u, merged_entries.size());
  for (const auto& entry : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = entry.second.get();
    const GURL& url =
        ukm_recorder.GetSourceForSourceId(ukm_entry->source_id)->url();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, url);
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        ukm_entry, "EnabledExtensionCount",
        ukm::GetExponentialBucketMin(2u, kBucketSpacing));
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        ukm_entry, "EnabledExtensionCount.InjectContentScript",
        ukm::GetExponentialBucketMin(2u, kBucketSpacing));
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        ukm_entry, "EnabledExtensionCount.HaveHostPermissions",
        ukm::GetExponentialBucketMin(1u, kBucketSpacing));
    if (url == kUrlA) {
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "WebRequestAuthProviderPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "WebRequestBlockingPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "WebRequestPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeNetRequestFeedbackPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeNetRequestPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeNetRequestWithHostAccessPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeWebRequestPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
    } else if (url == kUrlB) {
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "WebRequestAuthProviderPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "WebRequestBlockingPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "WebRequestPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeNetRequestFeedbackPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeNetRequestPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeNetRequestWithHostAccessPermissionCount",
          ukm::GetExponentialBucketMin(0u, kBucketSpacing));
      ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
          ukm_entry, "DeclarativeWebRequestPermissionCount",
          ukm::GetExponentialBucketMin(1u, kBucketSpacing));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

}  // namespace extensions
