// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/url_constants.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_loader.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/login/scoped_test_public_session_login_state.h"
#include "components/google/core/common/google_switches.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/test/test_url_loader_client.h"
#include "third_party/blink/public/platform/web_input_event.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/login/login_state.h"
#endif  // defined(OS_CHROMEOS)

using content::WebContents;

namespace extensions {

namespace {

class CancelLoginDialog : public content::NotificationObserver {
 public:
  CancelLoginDialog() {
    registrar_.Add(this,
                   chrome::NOTIFICATION_AUTH_NEEDED,
                   content::NotificationService::AllSources());
  }

  ~CancelLoginDialog() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    LoginHandler* handler =
        content::Details<LoginNotificationDetails>(details).ptr()->handler();
    handler->CancelAuth();
  }

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CancelLoginDialog);
};

// Sends an XHR request to the provided host, port, and path, and responds when
// the request was sent.
const char kPerformXhrJs[] =
    "var url = 'http://%s:%d/%s';\n"
    "var xhr = new XMLHttpRequest();\n"
    "xhr.open('GET', url);\n"
    "xhr.onload = function() {\n"
    "  window.domAutomationController.send(true);\n"
    "};\n"
    "xhr.onerror = function() {\n"
    "  window.domAutomationController.send(false);\n"
    "};\n"
    "xhr.send();\n";

// Header values set by the server and by the extension.
const char kHeaderValueFromExtension[] = "ValueFromExtension";
const char kHeaderValueFromServer[] = "ValueFromServer";

// Performs an XHR in the given |frame|, replying when complete.
void PerformXhrInFrame(content::RenderFrameHost* frame,
                      const std::string& host,
                      int port,
                      const std::string& page) {
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      frame,
      base::StringPrintf(kPerformXhrJs, host.c_str(), port, page.c_str()),
      &success));
  EXPECT_TRUE(success);
}

// Returns the current count of a variable stored in the |extension| background
// page. Returns -1 if something goes awry.
int GetCountFromBackgroundPage(const Extension* extension,
                               content::BrowserContext* context,
                               const std::string& variable_name) {
  ExtensionHost* host =
      ProcessManager::Get(context)->GetBackgroundHostForExtension(
          extension->id());
  if (!host || !host->host_contents())
    return -1;

  int count = -1;
  if (!ExecuteScriptAndExtractInt(
          host->host_contents(),
          "window.domAutomationController.send(" + variable_name + ")", &count))
    return -1;
  return count;
}

// Returns the current count of webRequests received by the |extension| in
// the background page (assumes the extension stores a value on the window
// object). Returns -1 if something goes awry.
int GetWebRequestCountFromBackgroundPage(const Extension* extension,
                                         content::BrowserContext* context) {
  return GetCountFromBackgroundPage(extension, context,
                                    "window.webRequestCount");
}

// Returns true if the |extension|'s background page saw an event for a request
// with the given |hostname| (|hostname| should exclude port).
bool HasSeenWebRequestInBackgroundPage(const Extension* extension,
                                       content::BrowserContext* context,
                                       const std::string& hostname) {
  // TODO(devlin): Here and in Get*CountFromBackgroundPage(), we should leverage
  // ExecuteScriptInBackgroundPage().
  ExtensionHost* host =
      ProcessManager::Get(context)->GetBackgroundHostForExtension(
          extension->id());
  if (!host || !host->host_contents())
    return false;

  bool seen = false;
  std::string script = base::StringPrintf(
      R"(domAutomationController.send(
                 window.requestedHostnames.includes('%s'));)",
      hostname.c_str());
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(host->host_contents(), script, &seen));
  return seen;
}

// The DevTool's remote front-end is hardcoded to a URL with a fixed port.
// Redirect all responses to a URL with port.
class DevToolsFrontendInterceptor : public net::URLRequestInterceptor {
 public:
  DevToolsFrontendInterceptor(int port, const base::FilePath& root_dir)
      : port_(port), test_root_dir_(root_dir) {}

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    // The DevTools front-end has a hard-coded scheme (and implicit port 443).
    // We simulate a response for it.
    // net::URLRequestRedirectJob cannot be used because DevToolsUIBindings
    // rejects URLs whose base URL is not the hard-coded URL.
    if (request->url().EffectiveIntPort() != port_) {
      return new net::URLRequestMockHTTPJob(
          request, network_delegate,
          test_root_dir_.AppendASCII(request->url().path().substr(1)));
    }
    return nullptr;
  }

 private:
  int port_;
  base::FilePath test_root_dir_;
};

void SetUpDevToolsFrontendInterceptorOnIO(int port,
                                          const base::FilePath& root_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      "https", kRemoteFrontendDomain,
      std::make_unique<DevToolsFrontendInterceptor>(port, root_dir));
}

void TearDownDevToolsFrontendInterceptorOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter::GetInstance()->ClearHandlers();
}

}  // namespace

class ExtensionWebRequestApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kGaiaUrl, "http://gaia.com");
  }

  void RunPermissionTest(
      const char* extension_directory,
      bool load_extension_with_incognito_permission,
      bool wait_for_extension_loaded_in_incognito,
      const char* expected_content_regular_window,
      const char* exptected_content_incognito_window);

  network::mojom::URLLoaderFactoryPtr CreateURLLoaderFactory() {
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network::mojom::URLLoaderFactoryPtr loader_factory;
    content::BrowserContext::GetDefaultStoragePartition(profile())
        ->GetNetworkContext()
        ->CreateURLLoaderFactory(mojo::MakeRequest(&loader_factory),
                                 std::move(params));
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
    auto dir = std::make_unique<TestExtensionDir>();
    dir->WriteManifest(base::StringPrintf(kManifest, name.c_str()));
    LoadExtension(dir->UnpackedPath());
    test_dirs_.push_back(std::move(dir));
  }

 private:
  std::vector<std::unique_ptr<TestExtensionDir>> test_dirs_;
};

class DevToolsFrontendInWebRequestApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    int port = embedded_test_server()->port();

    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
          base::BindRepeating(&DevToolsFrontendInWebRequestApiTest::OnIntercept,
                              base::Unretained(this), port));
    } else {
      base::RunLoop run_loop;
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&SetUpDevToolsFrontendInterceptorOnIO, port,
                         test_root_dir_),
          run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  void TearDownOnMainThread() override {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      url_loader_interceptor_.reset();
    } else {
      base::RunLoop run_loop;
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&TearDownDevToolsFrontendInterceptorOnIO),
          run_loop.QuitClosure());
      run_loop.Run();
    }
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
    // See comments in DevToolsFrontendInterceptor above. The devtools remote
    // frontend URLs are hardcoded into Chrome and are requested by some of the
    // tests here to exercise their behavior with respect to WebRequest.
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

  base::FilePath test_root_dir_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestApi) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_api.html")) << message_;
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_WebRequestSimple DISABLED_WebRequestSimple
#else
#define MAYBE_WebRequestSimple WebRequestSimple
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestSimple) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_simple.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestComplex) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_complex.html")) <<
      message_;
}

// This test times out regularly on MSAN trybots. See https://crbug.com/733395.
#if defined(MEMORY_SANITIZER)
#define MAYBE_WebRequestTypes DISABLED_WebRequestTypes
#else
#define MAYBE_WebRequestTypes WebRequestTypes
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestTypes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_types.html")) << message_;
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestPublicSession) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  chromeos::ScopedTestPublicSessionLoginState login_state;
  // Disable a CHECK while doing api tests.
  WebRequestPermissions::AllowAllExtensionLocationsInPublicSessionForTesting(
      true);
  ASSERT_TRUE(RunExtensionSubtest("webrequest_public_session", "test.html")) <<
      message_;
  WebRequestPermissions::AllowAllExtensionLocationsInPublicSessionForTesting(
      false);
}
#endif  // defined(OS_CHROMEOS)

// Test that a request to an OpenSearch description document (OSDD) generates
// an event with the expected details.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestTestOSDD) {
  // An OSDD request is only generated when a main frame at is loaded at /, so
  // serve osdd/index.html from the root of the test server:
  embedded_test_server()->ServeFilesFromDirectory(
      test_data_dir_.AppendASCII("webrequest/osdd"));
  ASSERT_TRUE(StartEmbeddedTestServer());

  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_osdd.html")) << message_;
}

// Test that the webRequest events are dispatched with the expected details when
// a frame or tab is removed while a response is being received.
// Flaky: https://crbug.com/617865
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestUnloadAfterRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?1")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?2")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?3")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?4")) <<
      message_;
}

// Test that the webRequest events are dispatched with the expected details when
// a frame or tab is immediately removed after starting a request.
// Flaky on Linux/Mac. See crbug.com/780369 for detail.
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_WebRequestUnloadImmediately DISABLED_WebRequestUnloadImmediately
#else
#define MAYBE_WebRequestUnloadImmediately WebRequestUnloadImmediately
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       MAYBE_WebRequestUnloadImmediately) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?5")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?6")) <<
      message_;
}

// Flaky (sometimes crash): http://crbug.com/140976
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestAuthRequired) {
  CancelLoginDialog login_dialog_helper;

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_auth_required.html")) <<
      message_;
}

// This test times out regularly on win_rel trybots. See http://crbug.com/122178
// Also on Linux/ChromiumOS debug, ASAN and MSAN builds.
// https://crbug.com/670415
#if defined(OS_WIN) || !defined(NDEBUG) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
#define MAYBE_WebRequestBlocking DISABLED_WebRequestBlocking
#else
#define MAYBE_WebRequestBlocking WebRequestBlocking
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestBlocking) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_blocking.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestBlockingSetCookieHeader) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_blocking_cookie.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestRedirects) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_redirects.html"))
      << message_;
}

// Tests that redirects from secure to insecure don't send the referrer header.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestRedirectsToInsecure) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL insecure_destination =
      embedded_test_server()->GetURL("/extensions/test_file.html");
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(https_test_server.Start());

  GURL url = https_test_server.GetURL("/webrequest/simulate_click.html");

  base::ListValue custom_args;
  custom_args.AppendString(url.spec());
  custom_args.AppendString(insecure_destination.spec());

  std::string config_string;
  base::JSONWriter::Write(custom_args, &config_string);
  ASSERT_TRUE(RunExtensionSubtestWithArg(
      "webrequest", "test_redirects_from_secure.html", config_string.c_str()))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestSubresourceRedirects) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionSubtest("webrequest", "test_subresource_redirects.html"))
      << message_;
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_WebRequestNewTab DISABLED_WebRequestNewTab
#else
#define MAYBE_WebRequestNewTab WebRequestNewTab
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestNewTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_newTab.html"))
      << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);
  GURL url = extension->GetResourceURL("newTab/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // There's a link on a.html with target=_blank. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// This test times out regularly on MSAN trybots. See https://crbug.com/733395.
// Also flaky. See https://crbug.com/846555.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestDeclarative1) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_declarative1.html"))
      << message_;
}

// This test times out regularly on MSAN trybots. See https://crbug.com/733395.
#if defined(MEMORY_SANITIZER)
#define MAYBE_WebRequestDeclarative2 DISABLED_WebRequestDeclarative2
#else
#define MAYBE_WebRequestDeclarative2 WebRequestDeclarative2
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       MAYBE_WebRequestDeclarative2) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const char* network_service_arg =
      base::FeatureList::IsEnabled(network::features::kNetworkService)
          ? "NetworkServiceEnabled"
          : "NetworkServiceDisabled";
  ASSERT_TRUE(RunExtensionSubtestWithArg("webrequest", "test_declarative2.html",
                                         network_service_arg))
      << message_;
}

void ExtensionWebRequestApiTest::RunPermissionTest(
    const char* extension_directory,
    bool load_extension_with_incognito_permission,
    bool wait_for_extension_loaded_in_incognito,
    const char* expected_content_regular_window,
    const char* exptected_content_incognito_window) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  ExtensionTestMessageListener listener("done", false);
  ExtensionTestMessageListener listener_incognito("done_incognito", false);

  int load_extension_flags = kFlagNone;
  if (load_extension_with_incognito_permission)
    load_extension_flags |= kFlagEnableIncognito;
  ASSERT_TRUE(LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("webrequest_permissions")
                    .AppendASCII(extension_directory),
      load_extension_flags));

  // Test that navigation in regular window is properly redirected.
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // This navigation should be redirected.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  std::string body;
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        tab,
        "window.domAutomationController.send(document.body.textContent)",
        &body));
  EXPECT_EQ(expected_content_regular_window, body);

  // Test that navigation in OTR window is properly redirected.
  Browser* otr_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  if (wait_for_extension_loaded_in_incognito)
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // This navigation should be redirected if
  // load_extension_with_incognito_permission is true.
  ui_test_utils::NavigateToURL(
      otr_browser,
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  body.clear();
  WebContents* otr_tab = otr_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      otr_tab,
      "window.domAutomationController.send(document.body.textContent)",
      &body));
  EXPECT_EQ(exptected_content_incognito_window, body);
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSpanning1) {
  // Test spanning with incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("spanning", true, false, "redirected1", "redirected1");
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSpanning2) {
  // Test spanning without incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("spanning", false, false, "redirected1", "");
}


IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSplit1) {
  // Test split with incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("split", true, true, "redirected1", "redirected2");
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSplit2) {
  // Test split without incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("split", false, false, "redirected1", "");
}

// TODO(vabr): Cure these flaky tests, http://crbug.com/238179.
#if !defined(NDEBUG)
#define MAYBE_PostData1 DISABLED_PostData1
#define MAYBE_PostData2 DISABLED_PostData2
#else
#define MAYBE_PostData1 PostData1
#define MAYBE_PostData2 PostData2
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_PostData1) {
  // Test HTML form POST data access with the default and "url" encoding.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_post1.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_PostData2) {
  // Test HTML form POST data access with the multipart and plaintext encoding.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_post2.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DeclarativeSendMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest_sendmessage")) << message_;
}

// Check that reloading an extension that runs in incognito split mode and
// has two active background pages with registered events does not crash the
// browser. Regression test for http://crbug.com/224094
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, IncognitoSplitModeReload) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Wait for rules to be set up.
  ExtensionTestMessageListener listener("done", false);
  ExtensionTestMessageListener listener_incognito("done_incognito", false);

  const Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("webrequest_reload"), kFlagEnableIncognito);
  ASSERT_TRUE(extension);
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // Reload extension and wait for rules to be set up again. This should not
  // crash the browser.
  ExtensionTestMessageListener listener2("done", false);
  ExtensionTestMessageListener listener_incognito2("done_incognito", false);

  ReloadExtension(extension->id());

  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, ExtensionRequests) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ExtensionTestMessageListener listener_main1("web_request_status1", true);
  ExtensionTestMessageListener listener_main2("web_request_status2", true);

  ExtensionTestMessageListener listener_app("app_done", false);
  ExtensionTestMessageListener listener_extension("extension_done", false);

  // Set up webRequest listener
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/main")));
  EXPECT_TRUE(listener_main1.WaitUntilSatisfied());
  EXPECT_TRUE(listener_main2.WaitUntilSatisfied());

  // Perform some network activity in an app and another extension.
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/app")));
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/extension")));

  EXPECT_TRUE(listener_app.WaitUntilSatisfied());
  EXPECT_TRUE(listener_extension.WaitUntilSatisfied());

  // Load a page, a content script will ping us when it is ready.
  ExtensionTestMessageListener listener_pageready("contentscript_ready", true);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
          "/extensions/test_file.html?match_webrequest_test"));
  EXPECT_TRUE(listener_pageready.WaitUntilSatisfied());

  // The extension and app-generated requests should not have triggered any
  // webRequest event filtered by type 'xmlhttprequest'.
  // (check this here instead of before the navigation, in case the webRequest
  // event routing is slow for some reason).
  ExtensionTestMessageListener listener_result(false);
  listener_main1.Reply("");
  EXPECT_TRUE(listener_result.WaitUntilSatisfied());
  EXPECT_EQ("Did not intercept any requests.", listener_result.message());

  ExtensionTestMessageListener listener_contentscript("contentscript_done",
                                                      false);
  ExtensionTestMessageListener listener_framescript("framescript_done", false);

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

  // The extension frame does run in the extension's process.
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
              DictionaryBuilder()
                  .Set("name", "Some hosted app")
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Set("app", DictionaryBuilder()
                                  .Set("launch", DictionaryBuilder()
                                                     .Set("web_url",
                                                          hosted_app_url.spec())
                                                     .Build())
                                  .Build())
                  .Build())
          .Build();
  ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(hosted_app.get());

  ExtensionTestMessageListener listener1("main_frame", false);
  ExtensionTestMessageListener listener2("xmlhttprequest", false);

  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_hosted_app")));

  ui_test_utils::NavigateToURL(browser(), hosted_app_url);

  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
}

// Tests that webRequest works with
// extensions_features::kRuntimeHostPermissions.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestWithWithheldPermissions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Navigate the browser to a page in a new tab.
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
  // since it doesn't have permission (and shouldn't receive any from an XHR).
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));
  EXPECT_FALSE(
      HasSeenWebRequestInBackgroundPage(extension, profile(), "b.com"));

  content::RenderFrameHost* main_frame = nullptr;
  content::RenderFrameHost* child_frame = nullptr;
  auto get_main_and_child_frame = [](content::WebContents* web_contents,
                                     content::RenderFrameHost** main_frame,
                                     content::RenderFrameHost** child_frame) {
    *child_frame = nullptr;
    *main_frame = web_contents->GetMainFrame();
    std::vector<content::RenderFrameHost*> all_frames =
        web_contents->GetAllFrames();
    ASSERT_EQ(3u, all_frames.size());
    *child_frame = all_frames[0] == *main_frame ? all_frames[1] : all_frames[0];
    ASSERT_TRUE(*child_frame);
  };

  get_main_and_child_frame(web_contents, &main_frame, &child_frame);
  const std::string kMainHost = main_frame->GetLastCommittedURL().host();
  const std::string kChildHost = child_frame->GetLastCommittedURL().host();

  PerformXhrInFrame(main_frame, kHost, port, kXhrPath);
  PerformXhrInFrame(child_frame, kChildHost, port, kXhrPath);
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner->GetBlockedActions(extension));

  // Grant activeTab permission, and perform another XHR. The extension should
  // receive the event.
  runner->set_default_bubble_close_action_for_testing(
      base::WrapUnique(new ToolbarActionsBarBubbleDelegate::CloseAction(
          ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE)));
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  // The runner will have refreshed the page...
  get_main_and_child_frame(web_contents, &main_frame, &child_frame);
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));

  int xhr_count = GetWebRequestCountFromBackgroundPage(extension, profile());
  // ... which means that we should have a non-zero xhr count, and the extension
  // should see the request for the cross-site subframe...
  EXPECT_GT(xhr_count, 0);
  EXPECT_TRUE(HasSeenWebRequestInBackgroundPage(extension, profile(), "b.com"));
  // ... and the extension should receive future events.
  PerformXhrInFrame(main_frame, kHost, port, kXhrPath);
  ++xhr_count;
  EXPECT_EQ(xhr_count,
            GetWebRequestCountFromBackgroundPage(extension, profile()));

  // However, activeTab only grants access to the main frame, not to child
  // frames. As such, trying to XHR in the child frame should still fail.
  PerformXhrInFrame(child_frame, kChildHost, port, kXhrPath);
  EXPECT_EQ(xhr_count,
            GetWebRequestCountFromBackgroundPage(extension, profile()));
  // But since there's no way for the user to currently grant access to child
  // frames, this shouldn't show up as a blocked action.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));

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
  PerformXhrInFrame(main_frame, kHost, port, kXhrPath);
  action_updated_waiter.Wait();
  EXPECT_EQ(web_contents, action_updated_waiter.last_web_contents());

  EXPECT_EQ(xhr_count,
            GetWebRequestCountFromBackgroundPage(extension, profile()));
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner->GetBlockedActions(extension));
}

// Test that extensions with granted runtime host permissions to a tab can
// intercept cross-origin requests from that tab.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestWithheldPermissionsCrossOriginRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/extensions/cross_site_script.html"));

  const std::string kCrossSiteHost("b.com");
  EXPECT_FALSE(
      HasSeenWebRequestInBackgroundPage(extension, profile(), kCrossSiteHost));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner->GetBlockedActions(extension));

  // Grant runtime host permission to the page. The page should refresh. Even
  // though the request is for b.com (and the extension only has access to
  // a.com), it should still see the request. This is necessary for extensions
  // with webRequest to work with runtime host permissions.
  // https://crbug.com/851722.
  runner->set_default_bubble_close_action_for_testing(
      base::WrapUnique(new ToolbarActionsBarBubbleDelegate::CloseAction(
          ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE)));
  runner->RunAction(extension, true /* grant tab permissions */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));

  EXPECT_TRUE(
      HasSeenWebRequestInBackgroundPage(extension, profile(), kCrossSiteHost));
}

// Tests behavior when an extension has withheld access to a request's URL, but
// not the initiator's (tab's) URL. Regression test for
// https://crbug.com/891586.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WithheldHostPermissionsForCrossOriginWithoutInitiator) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

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
           "background": { "scripts": ["background.js"] },
           "permissions": ["*://b.com:*/*", "webRequest"]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     R"(window.webRequestCount = 0;
         window.requestedHostnames = [];

         chrome.webRequest.onBeforeRequest.addListener(function(details) {
           ++window.webRequestCount;
           window.requestedHostnames.push((new URL(details.url)).hostname);
         }, {urls:['<all_urls>']});
         chrome.test.sendMessage('ready');)");

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension) << message_;
  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to example.com, which has a cross-site script to b.com.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/extensions/cross_site_script.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  // Even though the extension has access to b.com, it shouldn't show that it
  // wants to run, because example.com is not a requested host.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));
  EXPECT_FALSE(
      HasSeenWebRequestInBackgroundPage(extension, profile(), "b.com"));

  // Navigating to b.com (so that the script is hosted on the same origin as
  // the WebContents) should show the extension wants to run.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "b.com", "/extensions/cross_site_script.html"));
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner->GetBlockedActions(extension));
}

// Verify that requests to clientsX.google.com are protected properly.
// First test requests from a standard renderer and then a request from the
// browser process.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestClientsGoogleComProtection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("webrequest_clients_google_com"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  auto get_clients_google_request_count = [this, extension]() {
    return GetCountFromBackgroundPage(extension, profile(),
                                      "window.clientsGoogleWebRequestCount");
  };
  auto get_yahoo_request_count = [this, extension]() {
    return GetCountFromBackgroundPage(extension, profile(),
                                      "window.yahooWebRequestCount");
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
      xhr.onload = () => {window.domAutomationController.send(true);};
      xhr.onerror = () => {window.domAutomationController.send(false);};
      xhr.send();)";
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(web_contents->GetMainFrame(),
                                          kRequest, &success));
  // Requests always fail due to cross origin nature.
  EXPECT_FALSE(success);

  EXPECT_EQ(1, get_clients_google_request_count());
  EXPECT_EQ(0, get_yahoo_request_count());

  auto make_browser_request = [this](const GURL& url) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->resource_type = content::RESOURCE_TYPE_SUB_RESOURCE;

    auto* url_loader_factory =
        content::BrowserContext::GetDefaultStoragePartition(profile())
            ->GetURLLoaderFactoryForBrowserProcess()
            .get();
    content::SimpleURLLoaderTestHelper loader_helper;
    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory, loader_helper.GetCallback());

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
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestPacRequestProtection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension that registers a listener for webRequest events, and
  // wait until it's initialized.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_pac_request"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Configure a PAC script. Need to do this after the extension is loaded, so
  // that the PAC isn't already loaded by the time the extension starts
  // affecting requests.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->Set(proxy_config::prefs::kProxy,
                    ProxyConfigDictionary::CreatePacScript(
                        embedded_test_server()->GetURL("/self.pac").spec(),
                        true /* pac_mandatory */));
  // Flush the proxy configuration change over the Mojo pipe to avoid any races.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  // Navigate to a page. The URL doesn't matter.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));

  // The extension should not have seen the PAC request.
  EXPECT_EQ(0, GetCountFromBackgroundPage(extension, profile(),
                                          "window.pacRequestCount"));

  // The extension should have seen the request for the main frame.
  EXPECT_EQ(1, GetCountFromBackgroundPage(extension, profile(),
                                          "window.title2RequestCount"));

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
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDiceHeaderProtection) {
  // Load an extension that registers a listener for webRequest events, and
  // wait until it is initialized.
  ExtensionTestMessageListener listener("ready", false);
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
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that the Dice header was not changed by the extension.
  EXPECT_TRUE(test_webcontents_observer.did_finish_navigation_called());
  EXPECT_EQ(kHeaderValueFromServer,
            test_webcontents_observer.dice_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.new_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.control_header_value());

  // Check that the Dice header cannot be read by the extension.
  EXPECT_EQ(0, GetCountFromBackgroundPage(extension, profile(),
                                          "window.diceResponseHeaderCount"));
  EXPECT_EQ(1, GetCountFromBackgroundPage(extension, profile(),
                                          "window.controlResponseHeaderCount"));

  // Navigate to a non-Gaia URL intercepted by the extension.
  test_webcontents_observer.Clear();
  url = embedded_test_server()->GetURL("example.com", "/extensions/dice.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that the Dice header was changed by the extension.
  EXPECT_TRUE(test_webcontents_observer.did_finish_navigation_called());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.dice_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.new_header_value());
  EXPECT_EQ(kHeaderValueFromExtension,
            test_webcontents_observer.control_header_value());

  // Check that the Dice header can be read by the extension.
  EXPECT_EQ(1, GetCountFromBackgroundPage(extension, profile(),
                                          "window.diceResponseHeaderCount"));
  EXPECT_EQ(2, GetCountFromBackgroundPage(extension, profile(),
                                          "window.controlResponseHeaderCount"));
}

// Test that the webRequest events are dispatched for the WebSocket handshake
// requests.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebSocketRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_websocket.html"))
      << message_;
}

// Test that the webRequest events are dispatched for the WebSocket handshake
// requests when authenrication is requested by server.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebSocketRequestAuthRequired) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory(), true));
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_websocket_auth.html"))
      << message_;
}

// Test that the webRequest events are dispatched for the WebSocket handshake
// requests.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebSocketRequestOnWorker) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(StartWebSocketServer(net::GetWebSocketTestDataDirectory()));
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_websocket_worker.html"))
      << message_;
}

// Tests the WebRequestProxyingWebSocket does not crash when there is a
// connection error before AddChannelRequest is called. Regression test for
// http://crbug.com/878574.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebSocketConnectionErrorBeforeChannelRequest) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  InstallWebRequestExtension("extension");

  network::mojom::WebSocketPtr web_socket;
  network::mojom::WebSocketRequest request = mojo::MakeRequest(&web_socket);
  network::mojom::AuthenticationHandlerPtr auth_handler;
  content::RenderFrameHost* host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
      profile())
      ->MaybeProxyWebSocket(host, &request, &auth_handler);
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->GetNetworkContext()
      ->CreateWebSocket(std::move(request), network::mojom::kBrowserProcessId,
                        host->GetProcess()->GetID(),
                        url::Origin::Create(GURL("http://example.com")),
                        std::move(auth_handler));
  web_socket.reset();
}

// Test behavior when intercepting requests from a browser-initiated url fetch.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
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
           "background": { "scripts": ["background.js"] }
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
    ExtensionTestMessageListener listener("ready", false);
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
  ui_test_utils::NavigateToURL(browser(), google_url);
  EXPECT_EQ(google_url, web_contents->GetLastCommittedURL());
  {
    // google.com should succeed.
    std::string content;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents,
        "domAutomationController.send(document.body.textContent.trim());",
        &content));
    EXPECT_EQ(kGoogleBodyContent, content);
  }

  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/extensions/body2.html");

  ui_test_utils::NavigateToURL(browser(), example_url);
  {
    // example.com should fail.
    content::NavigationEntry* nav_entry =
        web_contents->GetController().GetLastCommittedEntry();
    ASSERT_TRUE(nav_entry);
    EXPECT_EQ(content::PAGE_TYPE_ERROR, nav_entry->GetPageType());
    std::string content;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents,
        "domAutomationController.send(document.body.textContent.trim());",
        &content));
    EXPECT_NE(kExampleBodyContent, content);
  }

  // A callback allow waiting for responses to complete with an expected status
  // and given content.
  auto make_browser_request =
      [](network::mojom::URLLoaderFactory* url_loader_factory, const GURL& url,
         const base::Optional<std::string>& expected_response,
         int expected_net_code) {
        auto request = std::make_unique<network::ResourceRequest>();
        request->url = url;

        content::SimpleURLLoaderTestHelper simple_loader_helper;
        auto simple_loader = network::SimpleURLLoader::Create(
            std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
        simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
            url_loader_factory, simple_loader_helper.GetCallback());

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
  auto* url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile())
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

// Test that initiator is only included as part of event details when the
// extension has a permission matching the initiator.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MinimumAccessInitiator) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener listener("ready", false);
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
                   {"no-permission.com", "example4.com", ""}};

  int port = embedded_test_server()->port();
  for (const auto& testcase : testcases) {
    SCOPED_TRACE(testcase.navigate_before_start + ":" + testcase.xhr_domain +
                 ":" + testcase.expected_initiator);
    ExtensionTestMessageListener initiator_listener(false);
    initiator_listener.set_extension_id(extension->id());
    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                                testcase.navigate_before_start,
                                                "/extensions/body1.html"));
    PerformXhrInFrame(web_contents->GetMainFrame(), testcase.xhr_domain, port,
                      "extensions/api_test/webrequest/xhr/data.json");
    EXPECT_TRUE(initiator_listener.WaitUntilSatisfied());
    if (testcase.expected_initiator.empty()) {
      ASSERT_EQ("NO_INITIATOR", initiator_listener.message());
    } else {
      ASSERT_EQ(
          "http://" + testcase.expected_initiator + ":" + std::to_string(port),
          initiator_listener.message());
    }
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestApiClearsBindingOnFirstListener) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  auto loader_factory = CreateURLLoaderFactory();
  bool has_connection_error = false;
  loader_factory.set_connection_error_handler(
      base::BindLambdaForTesting([&]() { has_connection_error = true; }));

  InstallWebRequestExtension("extension1");
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  EXPECT_TRUE(has_connection_error);

  // The second time there should be no connection error.
  loader_factory = CreateURLLoaderFactory();
  has_connection_error = false;
  loader_factory.set_connection_error_handler(
      base::BindLambdaForTesting([&]() { has_connection_error = true; }));
  InstallWebRequestExtension("extension2");
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  EXPECT_FALSE(has_connection_error);
}

// Regression test for http://crbug.com/878366.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestApiDoesNotCrashOnErrorAfterProfileDestroyed) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Create a profile that will be destroyed later.
  base::ScopedAllowBlockingForTesting allow_blocking;
#if defined(OS_CHROMEOS)
  chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
#endif  // defined(OS_CHROMEOS)
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* temp_profile = Profile::CreateProfile(
      profile_manager->user_data_dir().AppendASCII("profile"), nullptr,
      Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);

  // Create a WebRequestAPI instance that we can control the lifetime of.
  auto api = std::make_unique<WebRequestAPI>(temp_profile);
  // Make sure we are proxying for |temp_profile|.
  api->ForceProxyForTesting();
  content::BrowserContext::GetDefaultStoragePartition(temp_profile)
      ->FlushNetworkInterfaceForTesting();

  network::mojom::URLLoaderFactoryPtr factory;
  auto request = mojo::MakeRequest(&factory);
  auto temp_web_contents =
      WebContents::Create(WebContents::CreateParams(temp_profile));
  EXPECT_TRUE(api->MaybeProxyURLLoaderFactory(temp_web_contents->GetMainFrame(),
                                              false, &request));
  temp_web_contents.reset();
  auto params = network::mojom::URLLoaderFactoryParams::New();
  params->process_id = 0;
  content::BrowserContext::GetDefaultStoragePartition(temp_profile)
      ->GetNetworkContext()
      ->CreateURLLoaderFactory(std::move(request), std::move(params));

  network::TestURLLoaderClient client;
  network::mojom::URLLoaderPtr loader;
  network::ResourceRequest resource_request;
  resource_request.url = embedded_test_server()->GetURL("/hung");
  factory->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0, 0, network::mojom::kURLLoadOptionNone,
      resource_request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Destroy profile, unbind client to cause a connection error, and delete the
  // WebRequestAPI. This will cause the connection error that will reach the
  // proxy before the ProxySet shutdown code runs on the IO thread.
  api->Shutdown();
  ProfileDestroyer::DestroyProfileWhenAppropriate(temp_profile);
  client.Unbind();
  api.reset();
}

// Test fixture which sets a custom NTP Page.
class NTPInterceptionWebRequestAPITest : public ExtensionApiTest {
 public:
  NTPInterceptionWebRequestAPITest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  // ExtensionApiTest override:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    test_data_dir_ = test_data_dir_.AppendASCII("webrequest")
                         .AppendASCII("ntp_request_interception");
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());

    GURL ntp_url = https_test_server_.GetURL("/fake_ntp.html");
    local_ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        profile(), https_test_server_.base_url().spec(), ntp_url.spec());
  }

  const net::EmbeddedTestServer* https_test_server() const {
    return &https_test_server_;
  }

 private:
  net::EmbeddedTestServer https_test_server_;
  DISALLOW_COPY_AND_ASSIGN(NTPInterceptionWebRequestAPITest);
};

// Ensures that requests made by the NTP Instant renderer are hidden from the
// Web Request API. Regression test for crbug.com/797461.
IN_PROC_BROWSER_TEST_F(NTPInterceptionWebRequestAPITest,
                       NTPRendererRequestsHidden) {
  // Loads an extension which tries to intercept requests to
  // "fake_ntp_script.js", which will be loaded as part of the NTP renderer.
  ExtensionTestMessageListener listener("ready", true /*will_reply*/);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("extension"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  // Wait for webRequest listeners to be set up.
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();

  // Have the extension listen for requests to |fake_ntp_script.js|.
  listener.Reply(https_test_server()->GetURL("/fake_ntp_script.js").spec());

  // Returns true if the given extension was able to intercept the request to
  // "fake_ntp_script.js".
  auto was_script_request_intercepted =
      [this](const std::string& extension_id) {
        const std::string result = ExecuteScriptInBackgroundPage(
            extension_id, "getAndResetRequestIntercepted();");
        EXPECT_TRUE(result == "true" || result == "false")
            << "Unexpected result " << result;
        return result == "true";
      };

  // Returns true if the given |web_contents| has window.scriptExecuted set to
  // true;
  auto was_ntp_script_loaded = [](content::WebContents* web_contents) {
    bool was_ntp_script_loaded = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents, "domAutomationController.send(!!window.scriptExecuted);",
        &was_ntp_script_loaded));
    return was_ntp_script_loaded;
  };

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the NTP. The request for "fake_ntp_script.js" should not have
  // reached the extension, since it was made by the instant NTP renderer, which
  // is semi-privileged.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(was_ntp_script_loaded(web_contents));
  ASSERT_TRUE(search::IsInstantNTP(web_contents));
  EXPECT_FALSE(was_script_request_intercepted(extension->id()));

  // However, when a normal webpage requests the same script, the request should
  // be seen by the extension.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/page_with_ntp_script.html"));
  EXPECT_TRUE(was_ntp_script_loaded(web_contents));
  ASSERT_FALSE(search::IsInstantNTP(web_contents));
  EXPECT_TRUE(was_script_request_intercepted(extension->id()));
}

// Test fixture testing that requests made for the OneGoogleBar on behalf of
// the local NTP can't be intercepted by extensions.
class LocalNTPInterceptionWebRequestAPITest
    : public ExtensionApiTest,
      public OneGoogleBarServiceObserver {
 public:
  LocalNTPInterceptionWebRequestAPITest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  // ExtensionApiTest override:
  void SetUp() override {
    https_test_server_.RegisterRequestMonitor(base::BindRepeating(
        &LocalNTPInterceptionWebRequestAPITest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_test_server_.InitializeAndListen());
    ExtensionApiTest::SetUp();
    feature_list_.InitWithFeatures({::features::kUseGoogleLocalNtp}, {});
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
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> runloop_;

  // Initialized on the UI thread in SetUpOnMainThread. Read on UI and Embedded
  // Test Server IO thread thereafter.
  GURL one_google_bar_url_;

  // Accessed on multiple threads- UI and Embedded Test Server IO thread. Access
  // requires acquiring |lock_|.
  bool one_google_bar_request_seen_ = false;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(LocalNTPInterceptionWebRequestAPITest);
};

IN_PROC_BROWSER_TEST_F(LocalNTPInterceptionWebRequestAPITest,
                       OneGoogleBarRequestsHidden) {
  // Loads an extension which tries to intercept requests to the OneGoogleBar.
  ExtensionTestMessageListener listener("ready", true /*will_reply*/);
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
      [this](const std::string& extension_id) {
        const std::string result = ExecuteScriptInBackgroundPage(
            extension_id, "getAndResetRequestIntercepted();");
        EXPECT_TRUE(result == "true" || result == "false")
            << "Unexpected result " << result;
        return result == "true";
      };

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(GetAndResetOneGoogleBarRequestSeen());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(search::IsInstantNTP(web_contents));
  ASSERT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            web_contents->GetController().GetVisibleEntry()->GetURL());
  WaitForOneGoogleBarDataUpdate();
  ASSERT_TRUE(GetAndResetOneGoogleBarRequestSeen());

  // Ensure that the extension wasn't able to intercept the request.
  EXPECT_FALSE(was_script_request_intercepted(extension->id()));

  // A normal request to |one_google_bar_url()| (i.e. not made by
  // OneGoogleBarFetcher) should be intercepted by extensions.
  ui_test_utils::NavigateToURL(browser(), one_google_bar_url());
  EXPECT_TRUE(was_script_request_intercepted(extension->id()));
  ASSERT_TRUE(GetAndResetOneGoogleBarRequestSeen());
}

// Ensure that devtools frontend requests are hidden from the webRequest API.
IN_PROC_BROWSER_TEST_F(DevToolsFrontendInWebRequestApiTest, HiddenRequests) {
  // Test expectations differ with the Network Service because of the way
  // request interception is done for the test. In the legacy networking path a
  // URLRequestMockHTTPJob is used, which does not generate
  // |onBeforeHeadersSent| events. With the Network Service enabled, requests
  // issued to HTTP URLs by these tests look like real HTTP requests and
  // therefore do generate |onBeforeHeadersSent| events.
  //
  // These tests adjust their expectations accordingly based on whether or not
  // the Network Service is enabled.
  const char* network_service_arg =
      base::FeatureList::IsEnabled(network::features::kNetworkService)
          ? "NetworkServiceEnabled"
          : "NetworkServiceDisabled";
  ASSERT_TRUE(RunExtensionSubtestWithArg("webrequest", "test_devtools.html",
                                         network_service_arg))
      << message_;
}

// Tests that the webRequest events aren't dispatched when the request initiator
// is protected by policy.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
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

  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest/policy_blocked"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Extension communicates back using this listener name.
  const std::string listener_message = "protected_origin";

  // The number of requests initiated by a protected origin is tracked in
  // the extension's background page under this variable name.
  const std::string request_counter_name = "window.protectedOriginCount";

  EXPECT_EQ(0, GetCountFromBackgroundPage(extension, profile(),
                                          request_counter_name));

  // Wait until all remote Javascript files have been blocked / pulled down.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Domain that hosts javascript file referenced by example_com.
  const std::string example2_com = "example2.com";

  // The server saw a request for the remote Javascript file.
  EXPECT_TRUE(BrowsedTo(example2_com));

  // The request was seen by the extension.
  EXPECT_EQ(1, GetCountFromBackgroundPage(extension, profile(),
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // The server saw a request for the remote Javascript file.
  EXPECT_TRUE(BrowsedTo(example2_com));

  // The request was hidden from the extension.
  EXPECT_EQ(1, GetCountFromBackgroundPage(extension, profile(),
                                          request_counter_name));
}

// Tests that the webRequest events aren't dispatched when the URL of the
// request is protected by policy.
// Disabled because it is flaky. See https://crbug.com/835155
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       DISABLED_UrlProtectedByPolicy) {
  // Host protected by policy.
  const std::string protected_domain = "example.com";

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://" + protected_domain);
  }

  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadExtension(test_data_dir_.AppendASCII("webrequest/policy_blocked"));

  // Listen in case extension sees the requst.
  ExtensionTestMessageListener before_request_listener("protected_url", false);

  // Path to resolve during test navigations.
  const std::string test_path = "/defaultresponse?protected_url";

  // Navigate to the protected domain and wait until page fully loads.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL(protected_domain, test_path),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // The server saw a request for the non-protected site.
  EXPECT_TRUE(BrowsedTo(unprotected_domain));

  // The request was visible from the extension.
  EXPECT_TRUE(before_request_listener.was_satisfied());
}

// Test that no webRequest events are seen for a protected host during normal
// navigation. This replicates most of the tests from
// WebRequestWithWithheldPermissions with a protected host. Granting a tab
// specific permission shouldn't bypass our policy.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       WebRequestProtectedByPolicy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kRuntimeHostPermissions);

  // Host protected by policy.
  const std::string protected_domain = "example.com";

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://" + protected_domain);
  }

  ASSERT_TRUE(StartEmbeddedTestServer());

  ExtensionTestMessageListener listener("ready", false);
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
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));
  PerformXhrInFrame(web_contents->GetMainFrame(), protected_domain, port,
                    kXhrPath);
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));

  // Grant activeTab permission, and perform another XHR. The extension should
  // still be blocked due to ExtensionSettings policy on example.com.
  // Only records ACCESS_WITHHELD, not ACCESS_DENIED, this is why it matches
  // BLOCKED_ACTION_NONE.
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));
  runner->set_default_bubble_close_action_for_testing(
      base::WrapUnique(new ToolbarActionsBarBubbleDelegate::CloseAction(
          ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE)));
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));
  int xhr_count = GetWebRequestCountFromBackgroundPage(extension, profile());
  // ... which means that we should have a non-zero xhr count if the policy
  // didn't block the events.
  EXPECT_EQ(0, xhr_count);
  // And the extension should also block future events.
  PerformXhrInFrame(web_contents->GetMainFrame(), protected_domain, port,
                    kXhrPath);
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));
}

// A test fixture which mocks the Time::Now() function to ensure that the
// default clock returns monotonically increasing time.
class ExtensionWebRequestMockedClockTest : public ExtensionWebRequestApiTest {
 public:
  ExtensionWebRequestMockedClockTest()
      : scoped_time_clock_override_(&ExtensionWebRequestMockedClockTest::Now,
                                    nullptr,
                                    nullptr) {}

 private:
  static base::Time Now() {
    static base::Time now_time = base::Time::UnixEpoch();
    now_time += base::TimeDelta::FromMilliseconds(1);
    return now_time;
  }

  base::subtle::ScopedTimeClockOverrides scoped_time_clock_override_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestMockedClockTest);
};

// Tests that we correctly dispatch the OnActionIgnored event on an extension
// if the extension's proposed redirect is ignored.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestMockedClockTest,
                       OnActionIgnored_Redirect) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load the two extensions. They redirect "google.com" main-frame urls to
  // the corresponding "example.com and "foo.com" urls.
  base::FilePath test_dir =
      test_data_dir_.AppendASCII("webrequest/on_action_ignored");

  // Load the first extension.
  ExtensionTestMessageListener ready_1_listener("ready_1",
                                                false /*will_reply*/);
  const Extension* extension_1 =
      LoadExtension(test_dir.AppendASCII("extension_1"));
  ASSERT_TRUE(extension_1);
  ASSERT_TRUE(ready_1_listener.WaitUntilSatisfied());
  const std::string extension_id_1 = extension_1->id();

  // Load the second extension.
  ExtensionTestMessageListener ready_2_listener("ready_2",
                                                false /*will_reply*/);
  const Extension* extension_2 =
      LoadExtension(test_dir.AppendASCII("extension_2"));
  ASSERT_TRUE(extension_2);
  ASSERT_TRUE(ready_2_listener.WaitUntilSatisfied());
  const std::string extension_id_2 = extension_2->id();

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_LT(prefs->GetInstallTime(extension_id_1),
            prefs->GetInstallTime(extension_id_2));

  // The extensions will notify the browser if their proposed redirect was
  // successful or not.
  ExtensionTestMessageListener redirect_ignored_listener("redirect_ignored",
                                                         false /*will_reply*/);
  ExtensionTestMessageListener redirect_successful_listener(
      "redirect_successful", false /*will_reply*/);

  const GURL url = embedded_test_server()->GetURL("google.com", "/simple.html");
  const GURL expected_redirect_url_1 =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  const GURL expected_redirect_url_2 =
      embedded_test_server()->GetURL("foo.com", "/simple.html");

  ui_test_utils::NavigateToURL(browser(), url);

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

  EXPECT_LT(prefs->GetInstallTime(extension_id_2),
            prefs->GetInstallTime(extension_id_1));

  redirect_ignored_listener.Reset();
  redirect_successful_listener.Reset();
  ui_test_utils::NavigateToURL(browser(), url);

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

}  // namespace extensions
