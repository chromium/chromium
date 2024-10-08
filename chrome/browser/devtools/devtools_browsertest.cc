// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <deque>
#include <memory>
#include <optional>

#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/extensions/extension_side_panel_test_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

using content::DevToolsAgentHost;
using content::DevToolsAgentHostObserver;
using content::NavigationController;
using content::RenderFrameHost;
using content::WebContents;
using extensions::Extension;
using javascript_dialogs::AppModalDialogView;

namespace {

const char kDebuggerTestPage[] = "/devtools/debugger_test_page.html";
const char kPauseWhenLoadingDevTools[] =
    "/devtools/pause_when_loading_devtools.html";
const char kPageWithContentScript[] = "/devtools/page_with_content_script.html";
const char kNavigateBackTestPage[] = "/devtools/navigate_back.html";
const char kWindowOpenTestPage[] = "/devtools/window_open.html";
const char kLatencyInfoTestPage[] = "/devtools/latency_info.html";
const char kChunkedTestPage[] = "/chunked";
const char kPushTestPage[] = "/devtools/push_test_page.html";
// The resource is not really pushed, but mock url request job pretends it is.
const char kPushTestResource[] = "/devtools/image.png";
const char kPushUseNullEndTime[] = "pushUseNullEndTime";
const char kSlowTestPage[] =
    "/chunked?waitBeforeHeaders=100&waitBetweenChunks=100&chunksNumber=2";
const char kSharedWorkerTestPage[] = "/workers/workers_ui_shared_worker.html";
const char kSharedWorkerTestWorker[] = "/workers/workers_ui_shared_worker.js";
const char kReloadSharedWorkerTestPage[] =
    "/workers/debug_shared_worker_initialization.html";
const char kReloadSharedWorkerTestWorker[] =
    "/workers/debug_shared_worker_initialization.js";
const char kEmulateNetworkConditionsPage[] =
    "/devtools/emulate_network_conditions.html";
const char kDispatchKeyEventShowsAutoFill[] =
    "/devtools/dispatch_key_event_shows_auto_fill.html";
const char kDOMWarningsTestPage[] = "/devtools/dom_warnings_page.html";
const char kEmptyTestPage[] = "/devtools/empty.html";
// Arbitrary page that returns a 200 response, for tests that don't care about
// more than that.
const char kArbitraryPage[] = "/title1.html";

template <typename... T>
void DispatchOnTestSuiteSkipCheck(DevToolsWindow* window,
                                  const char* method,
                                  T... args) {
  WebContents* wc = DevToolsWindowTesting::Get(window)->main_web_contents();
  const char* args_array[] = {method, args...};
  std::ostringstream script;
  script << "uiTests.dispatchOnTestSuite([";
  for (size_t i = 0; i < std::size(args_array); ++i) {
    script << (i ? "," : "") << '\"' << args_array[i] << '\"';
  }
  script << "])";

  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(content::ExecJs(wc, script.str()));

  std::string result;
  EXPECT_TRUE(message_queue.WaitForMessage(&result));

  EXPECT_EQ("\"[OK]\"", result);
}

void LoadLegacyFilesInFrontend(DevToolsWindow* window) {
  WebContents* wc = DevToolsWindowTesting::Get(window)->main_web_contents();
  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(content::ExecJs(wc, "uiTests.setupLegacyFilesForTest();",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  std::string result;
  EXPECT_TRUE(message_queue.WaitForMessage(&result));

  ASSERT_EQ("\"[OK]\"", result);
}

template <typename... T>
void DispatchOnTestSuite(DevToolsWindow* window,
                         const char* method,
                         T... args) {
  WebContents* wc = DevToolsWindowTesting::Get(window)->main_web_contents();
  // At first check that JavaScript part of the front-end is loaded by
  // checking that global variable uiTests exists(it's created after all js
  // files have been loaded) and has runTest method.
  ASSERT_EQ(
      "function",
      content::EvalJs(
          wc, "'' + (window.uiTests && (typeof uiTests.dispatchOnTestSuite))"))
      << "DevTools front-end is broken.";
  LoadLegacyFilesInFrontend(window);
  DispatchOnTestSuiteSkipCheck(window, method, args...);
}

void RunTestFunction(DevToolsWindow* window, const char* test_name) {
  DispatchOnTestSuite(window, test_name);
}

void SwitchToPanel(DevToolsWindow* window, const char* panel) {
  DispatchOnTestSuite(window, "switchToPanel", panel);
}

// Version of SwitchToPanel that works with extension-created panels.
void SwitchToExtensionPanel(DevToolsWindow* window,
                            const Extension* devtools_extension,
                            const char* panel_name) {
  // The full name is the concatenation of the extension URL (stripped of its
  // trailing '/') and the |panel_name| that was passed to panels.create().
  std::string prefix(base::TrimString(devtools_extension->url().spec(), "/",
                                      base::TRIM_TRAILING));
  SwitchToPanel(window, (prefix + panel_name).c_str());
}

void DisallowDevToolsForForceInstalledExtenions(Browser* browser) {
  browser->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsPolicyHandler::Availability::
                           kDisallowedForForceInstalledExtensions));
}

void DisallowDevTools(Browser* browser) {
  browser->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));
}

void AllowDevTools(Browser* browser) {
  browser->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kAllowed));
}

scoped_refptr<DevToolsAgentHost> GetOrCreateDevToolsHostForWebContents(
    WebContents* wc) {
  return content::DevToolsAgentHost::GetOrCreateForTab(wc);
}

}  // namespace

class DevToolsTest : public InProcessBrowserTest {
 public:
  DevToolsTest() : window_(nullptr) {}

  void SetUpOnMainThread() override {
    // A number of tests expect favicon requests to succeed - otherwise, they'll
    // generate console errors.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&DevToolsTest::HandleFaviconRequest));
    // LoadNetworkResourceForFrontend depends on "hello.html" from content's
    // test directory.
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  static std::unique_ptr<net::test_server::HttpResponse> HandleFaviconRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/favicon.ico") {
      return nullptr;
    }
    // The response doesn't have to be a valid favicon to avoid logging a
    // console error. Any 200 response will do.
    return std::make_unique<net::test_server::BasicHttpResponse>();
  }

  void RunTest(const std::string& test_name, const std::string& test_page) {
    OpenDevToolsWindow(test_page, false);
    RunTestFunction(window_, test_name.c_str());
    CloseDevToolsWindow();
  }

  template <typename... T>
  void RunTestMethod(const char* method, T... args) {
    DispatchOnTestSuiteSkipCheck(window_, method, args...);
  }

  template <typename... T>
  void DispatchAndWait(const char* method, T... args) {
    DispatchOnTestSuiteSkipCheck(window_, "waitForAsync", method, args...);
  }

  template <typename... T>
  void DispatchInPageAndWait(const char* method, T... args) {
    DispatchAndWait("invokePageFunctionAsync", method, args...);
  }

  void LoadTestPage(const std::string& test_page) {
    GURL url;
    if (base::StartsWith(test_page, "/")) {
      url = embedded_test_server()->GetURL(test_page);
    } else {
      url = GURL(test_page);
    }
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  void OpenDevToolsWindow(const std::string& test_page, bool is_docked) {
    LoadTestPage(test_page);

    window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(),
                                                            is_docked);
  }

  void OpenDevToolsWindowOnOffTheRecordTab(const std::string& test_page) {
    GURL url;
    if (base::StartsWith(test_page, "/")) {
      url = embedded_test_server()->GetURL(test_page);
    } else {
      url = GURL(test_page);
    }
    auto* otr_browser = OpenURLOffTheRecord(browser()->profile(), url);

    window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(
        otr_browser->tab_strip_model()->GetWebContentsAt(0), false);
  }

  WebContents* GetInspectedTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
  }

  WebContents* main_web_contents() {
    return DevToolsWindowTesting::Get(window_)->main_web_contents();
  }

  WebContents* toolbox_web_contents() {
    return DevToolsWindowTesting::Get(window_)->toolbox_web_contents();
  }

  raw_ptr<DevToolsWindow, DanglingUntriaged> window_;
};

class SitePerProcessDevToolsTest : public DevToolsTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    DevToolsTest::SetUpOnMainThread();
  }
};

// Used to block until a dev tools window gets beforeunload event.
class DevToolsWindowBeforeUnloadObserver : public content::WebContentsObserver {
 public:
  explicit DevToolsWindowBeforeUnloadObserver(DevToolsWindow*);

  DevToolsWindowBeforeUnloadObserver(
      const DevToolsWindowBeforeUnloadObserver&) = delete;
  DevToolsWindowBeforeUnloadObserver& operator=(
      const DevToolsWindowBeforeUnloadObserver&) = delete;

  void Wait();

 private:
  // Invoked when the beforeunload handler fires.
  void BeforeUnloadFired(bool proceed) override;

  bool m_fired;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

DevToolsWindowBeforeUnloadObserver::DevToolsWindowBeforeUnloadObserver(
    DevToolsWindow* devtools_window)
    : WebContentsObserver(
          DevToolsWindowTesting::Get(devtools_window)->main_web_contents()),
      m_fired(false) {}

void DevToolsWindowBeforeUnloadObserver::Wait() {
  if (m_fired) {
    return;
  }
  message_loop_runner_ = base::MakeRefCounted<content::MessageLoopRunner>();
  message_loop_runner_->Run();
}

void DevToolsWindowBeforeUnloadObserver::BeforeUnloadFired(bool proceed) {
  m_fired = true;
  if (message_loop_runner_.get()) {
    message_loop_runner_->Quit();
  }
}

class DevToolsBeforeUnloadTest : public DevToolsTest {
 public:
  void CloseInspectedTab() {
    browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                     TabCloseTypes::CLOSE_NONE);
  }

  void CloseDevToolsWindowAsync() {
    DevToolsWindowTesting::CloseDevToolsWindow(window_);
  }

  void CloseInspectedBrowser() { chrome::CloseWindow(browser()); }

 protected:
  void InjectBeforeUnloadListener(content::WebContents* web_contents) {
    ASSERT_TRUE(
        content::ExecJs(web_contents,
                        "window.addEventListener('beforeunload',"
                        "function(event) { event.returnValue = 'Foo'; });"));
    content::PrepContentsForBeforeUnloadTest(web_contents);
  }

  void RunBeforeUnloadTest(bool is_docked,
                           base::RepeatingCallback<void(void)> close_method,
                           bool wait_for_browser_close = true) {
    OpenDevToolsWindow(kDebuggerTestPage, is_docked);
    auto runner = base::MakeRefCounted<content::MessageLoopRunner>();
    DevToolsWindowTesting::Get(window_)->SetCloseCallback(
        runner->QuitClosure());
    InjectBeforeUnloadListener(main_web_contents());
    {
      DevToolsWindowBeforeUnloadObserver before_unload_observer(window_);
      close_method.Run();
      CancelModalDialog();
      before_unload_observer.Wait();
    }
    {
      close_method.Run();
      AcceptModalDialog();
      if (wait_for_browser_close) {
        ui_test_utils::WaitForBrowserToClose(browser());
      }
    }
    runner->Run();
  }

  DevToolsWindow* OpenDevToolWindowOnWebContents(content::WebContents* contents,
                                                 bool is_docked) {
    DevToolsWindow* window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(contents, is_docked);
    return window;
  }

  void OpenDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    ASSERT_TRUE(content::ExecJs(
        DevToolsWindowTesting::Get(devtools_window)->main_web_contents(),
        "window.open(\"\", \"\", \"location=0\");"));
    Browser* popup_browser = BrowserList::GetInstance()->GetLastActive();
    WebContents* popup_contents =
        popup_browser->tab_strip_model()->GetActiveWebContents();
    content::WaitForLoadStop(popup_contents);
  }

  void CloseDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  }

  void AcceptModalDialog() {
    AppModalDialogView* view = GetDialog();
    view->AcceptAppModalDialog();
  }

  void CancelModalDialog() {
    AppModalDialogView* view = GetDialog();
    view->CancelAppModalDialog();
  }

  AppModalDialogView* GetDialog() {
    javascript_dialogs::AppModalDialogController* dialog =
        ui_test_utils::WaitForAppModalDialog();
    AppModalDialogView* view = dialog->view();
    EXPECT_TRUE(view);
    return view;
  }
};

constexpr char kPublicKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8c4fBSPZ6utYoZ8NiWF/"
    "DSaimBhihjwgOsskyleFGaurhi3TDClTVSGPxNkgCzrz0wACML7M4aNjpd05qupdbR2d294j"
    "kDuI7caxEGUucpP7GJRRHnm8Sx+"
    "y0ury28n8jbN0PnInKKWcxpIXXmNQyC19HBuO3QIeUq9Dqc+7YFQIDAQAB";

// Base class for DevTools tests that test devtools functionality for
// extensions and content scripts.
class DevToolsExtensionTest : public DevToolsTest {
 public:
  DevToolsExtensionTest()
      : test_extensions_dir_(
            base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
                .AppendASCII("devtools")
                .AppendASCII("extensions")) {}

 protected:
  // Load an extension from test\data\devtools\extensions\<extension_name>
  void LoadExtension(const char* extension_name) {
    base::FilePath path = test_extensions_dir_.AppendASCII(extension_name);
    ASSERT_TRUE(LoadExtensionFromPath(path)) << "Failed to load extension.";
  }

  const Extension* LoadExtensionFromPath(const base::FilePath& path,
                                         bool allow_file_access = false) {
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());
    extensions::TestExtensionRegistryObserver observer(registry);
    auto installer = extensions::UnpackedInstaller::Create(service);
    installer->set_allow_file_access(allow_file_access);
    installer->Load(path);
    observer.WaitForExtensionLoaded();

    // Wait for any additional extension views to load.
    extensions::ChromeExtensionTestNotificationObserver(browser())
        .WaitForExtensionViewsToLoad();

    return GetExtensionByPath(registry->enabled_extensions(), path);
  }

  base::Value::Dict BuildExtensionManifest(
      const std::string& name,
      const std::string& devtools_page = "",
      const std::string& key = "") {
    auto manifest = base::Value::Dict()
                        .Set("name", name)
                        .Set("version", "1")
                        .Set("manifest_version", 2)
                        // simple_test_page.html is currently the only page
                        // referenced outside of its own extension in the tests
                        .Set("web_accessible_resources",
                             base::Value::List()
                                 .Append("simple_test_page.html")
                                 .Append("source.map"));

    // If |devtools_page| isn't empty, make it a devtools extension in the
    // manifest.
    if (!devtools_page.empty()) {
      manifest.Set("devtools_page", devtools_page);
    }
    if (!key.empty()) {
      manifest.Set("key", key);
    }
    return manifest;
  }

  // Builds an extension populated with a bunch of test
  // pages. |name| is the extension name to use in the manifest.
  // |devtools_page|, if non-empty, indicates which test page should be be
  // listed as a devtools_page in the manifest.  If |devtools_page| is empty, a
  // non-devtools extension is created instead. |panel_iframe_src| controls the
  // src= attribute of the <iframe> element in the 'panel.html' test page.
  extensions::TestExtensionDir& BuildExtensionForTest(
      const std::string& name,
      const std::string& devtools_page,
      const std::string& panel_iframe_src) {
    test_extension_dirs_.emplace_back();
    extensions::TestExtensionDir& dir = test_extension_dirs_.back();

    dir.WriteManifest(BuildExtensionManifest(name, devtools_page));

    GURL http_frame_url =
        embedded_test_server()->GetURL("a.com", "/popup_iframe.html");

    // If this is a devtools extension, |devtools_page| will indicate which of
    // these devtools_pages will end up being used.  Different tests use
    // different devtools_pages.
    dir.WriteFile(FILE_PATH_LITERAL("web_devtools_page.html"),
                  "<html><body><iframe src='" + http_frame_url.spec() +
                      "'></iframe></body></html>");

    dir.WriteFile(FILE_PATH_LITERAL("simple_devtools_page.html"),
                  "<html><body></body></html>");

    dir.WriteFile(
        FILE_PATH_LITERAL("panel_devtools_page.html"),
        "<html><head><script "
        "src='panel_devtools_page.js'></script></head><body></body></html>");

    dir.WriteFile(FILE_PATH_LITERAL("panel_devtools_page.js"),
                  "chrome.devtools.panels.create('iframe-panel',\n"
                  "    null,\n"
                  "    'panel.html',\n"
                  "    function(panel) {\n"
                  "      chrome.devtools.inspectedWindow.eval(\n"
                  "        'console.log(\"PASS\")');\n"
                  "    }\n"
                  ");\n");

    dir.WriteFile(FILE_PATH_LITERAL("source.map"),
                  R"({"version":3,"sources":["foo.js"],"mappings":"AAyCAA"})");

    dir.WriteFile(FILE_PATH_LITERAL("sidebarpane_devtools_page.html"),
                  "<html><head><script src='sidebarpane_devtools_page.js'>"
                  "</script></head><body></body></html>");

    dir.WriteFile(
        FILE_PATH_LITERAL("sidebarpane_devtools_page.js"),
        "chrome.devtools.panels.elements.createSidebarPane('iframe-pane',\n"
        "    function(sidebar) {\n"
        "      chrome.devtools.inspectedWindow.eval(\n"
        "        'console.log(\"PASS\")');\n"
        "      sidebar.setPage('panel.html');\n"
        "    }\n"
        ");\n");

    dir.WriteFile(FILE_PATH_LITERAL("panel.html"),
                  "<html><body><iframe src='" + panel_iframe_src +
                      "'></iframe></body></html>");

    dir.WriteFile(FILE_PATH_LITERAL("simple_test_page.html"),
                  "<html><body>This is a test</body></html>");

    GURL web_url = embedded_test_server()->GetURL("a.com", "/title3.html");

    dir.WriteFile(FILE_PATH_LITERAL("multi_frame_page.html"),
                  "<html><body><iframe src='about:blank'>"
                  "</iframe><iframe src='data:text/html,foo'>"
                  "</iframe><iframe src='" +
                      web_url.spec() + "'></iframe></body></html>");
    return dir;
  }

  // Loads a dynamically generated extension populated with a bunch of test
  // pages.
  const Extension* LoadExtensionForTest(const std::string& name,
                                        const std::string& devtools_page,
                                        const std::string& panel_iframe_src) {
    extensions::TestExtensionDir& dir =
        BuildExtensionForTest(name, devtools_page, panel_iframe_src);
    return LoadExtensionFromPath(dir.UnpackedPath());
  }

  std::string BuildComponentExtension() {
    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    extensions::ComponentLoader* component_loader =
        extension_service->component_loader();
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(browser()->profile());

    extensions::TestExtensionDir& extension_dir =
        BuildExtensionForTest("Component extension", "" /* devtools_page */,
                              "" /* panel_iframe_src */);
    auto manifest = BuildExtensionManifest("Component extension",
                                           "" /* devtools_page */, kPublicKey);
    component_loader->set_ignore_allowlist_for_testing(true);
    std::string extension_id = component_loader->Add(
        std::move(manifest), extension_dir.UnpackedPath());
    EXPECT_TRUE(extension_registry->enabled_extensions().GetByID(extension_id));
    return extension_id;
  }

  const base::FilePath test_extensions_dir_;

 private:
  const Extension* GetExtensionByPath(
      const extensions::ExtensionSet& extensions,
      const base::FilePath& path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
    EXPECT_TRUE(!extension_path.empty());
    for (const scoped_refptr<const Extension>& extension : extensions) {
      if (extension->path() == extension_path) {
        return extension.get();
      }
    }
    return nullptr;
  }

  // Use std::deque to avoid dangling references to existing elements.
  std::deque<extensions::TestExtensionDir> test_extension_dirs_;
};

class DevToolsExperimentalExtensionTest : public DevToolsExtensionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }
};

class DevToolsServiceWorkerExtensionTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    extension_service_ =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    extension_registry_ = extensions::ExtensionRegistry::Get(profile);
  }

  const extensions::Extension* LoadExtension(base::FilePath extension_path) {
    extensions::TestExtensionRegistryObserver observer(extension_registry_);
    ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");
    extensions::UnpackedInstaller::Create(extension_service_)
        ->Load(extension_path);
    observer.WaitForExtensionLoaded();
    const extensions::Extension* extension = nullptr;
    for (const auto& enabled_extension :
         extension_registry_->enabled_extensions()) {
      if (enabled_extension->path() == extension_path) {
        extension = enabled_extension.get();
        break;
      }
    }
    CHECK(extension) << "Failed to find loaded extension " << extension_path;
    EXPECT_TRUE(activated_listener.WaitUntilSatisfied());
    return extension;
  }

  scoped_refptr<DevToolsAgentHost> FindExtensionHost(const std::string& id) {
    for (auto& host : DevToolsAgentHost::GetOrCreateAll()) {
      if (host->GetType() == DevToolsAgentHost::kTypeServiceWorker &&
          host->GetURL().host() == id) {
        return host;
      }
    }
    return nullptr;
  }

  void OpenDevToolsWindow(scoped_refptr<DevToolsAgentHost> host) {
    Profile* profile = browser()->profile();
    window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(profile, host);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
  }

  raw_ptr<DevToolsWindow, DanglingUntriaged> window_ = nullptr;
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;
  raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged>
      extension_registry_ = nullptr;
};

// TODO(crbug.com/40943436): Fix the memory leak and enable the test.
#if defined(LEAK_SANITIZER) && BUILDFLAG(IS_LINUX)
#define MAYBE_AttachOnReload DISABLED_AttachOnReload
#else
#define MAYBE_AttachOnReload AttachOnReload
#endif
IN_PROC_BROWSER_TEST_F(DevToolsServiceWorkerExtensionTest,
                       MAYBE_AttachOnReload) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("service_worker");
  std::string extension_id;
  {
    const extensions::Extension* extension = LoadExtension(extension_path);
    extension_id = extension->id();
  }
  scoped_refptr<DevToolsAgentHost> host = FindExtensionHost(extension_id);
  ASSERT_TRUE(host);
  OpenDevToolsWindow(host);
  extension_service_->ReloadExtension(extension_id);
  RunTestFunction(window_, "waitForTestResultsInConsole");
  CloseDevToolsWindow();
}

class WorkerDevToolsTest : public InProcessBrowserTest {
 public:
  WorkerDevToolsTest() : window_(nullptr) {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  class WorkerCreationObserver : public DevToolsAgentHostObserver {
   public:
    WorkerCreationObserver(const std::string& path,
                           scoped_refptr<DevToolsAgentHost>* out_host,
                           base::OnceClosure quit)
        : path_(path), out_host_(out_host), quit_(std::move(quit)) {
      DevToolsAgentHost::AddObserver(this);
    }

   private:
    ~WorkerCreationObserver() override {
      DevToolsAgentHost::RemoveObserver(this);
    }

    void DevToolsAgentHostCreated(DevToolsAgentHost* host) override {
      if (host->GetType() == DevToolsAgentHost::kTypeSharedWorker &&
          host->GetURL().path().rfind(path_) != std::string::npos) {
        *out_host_ = host;
        content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                     std::move(quit_));
        delete this;
      }
    }

    std::string path_;
    raw_ptr<scoped_refptr<DevToolsAgentHost>> out_host_;
    base::OnceClosure quit_;
  };

  static scoped_refptr<DevToolsAgentHost> WaitForFirstSharedWorker(
      const char* path) {
    for (auto& host : DevToolsAgentHost::GetOrCreateAll()) {
      if (host->GetType() == DevToolsAgentHost::kTypeSharedWorker &&
          host->GetURL().path().rfind(path) != std::string::npos) {
        return host;
      }
    }
    scoped_refptr<DevToolsAgentHost> host;
    base::RunLoop run_loop;
    new WorkerCreationObserver(path, &host, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
    return host;
  }

  void OpenDevToolsWindow(scoped_refptr<DevToolsAgentHost> agent_host) {
    Profile* profile = browser()->profile();
    window_ =
        DevToolsWindowTesting::OpenDevToolsWindowSync(profile, agent_host);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
  }

  raw_ptr<DevToolsWindow, DanglingUntriaged> window_;
};

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestDockedDevToolsClose) {
  RunBeforeUnloadTest(
      true,
      base::BindRepeating(&DevToolsBeforeUnloadTest::CloseDevToolsWindowAsync,
                          base::Unretained(this)),
      false);
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected page.
//
// TODO(crbug.com/40679397): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestDockedDevToolsInspectedTabClose \
  DISABLED_TestDockedDevToolsInspectedTabClose
#else
#define MAYBE_TestDockedDevToolsInspectedTabClose \
  TestDockedDevToolsInspectedTabClose
#endif
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       MAYBE_TestDockedDevToolsInspectedTabClose) {
  RunBeforeUnloadTest(
      true, base::BindRepeating(&DevToolsBeforeUnloadTest::CloseInspectedTab,
                                base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadTest(
      true,
      base::BindRepeating(&DevToolsBeforeUnloadTest::CloseInspectedBrowser,
                          base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestUndockedDevToolsClose) {
  RunBeforeUnloadTest(
      false,
      base::BindRepeating(&DevToolsBeforeUnloadTest::CloseDevToolsWindowAsync,
                          base::Unretained(this)),
      false);
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedTabClose) {
  RunBeforeUnloadTest(
      false, base::BindRepeating(&DevToolsBeforeUnloadTest::CloseInspectedTab,
                                 base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadTest(
      false,
      base::BindRepeating(&DevToolsBeforeUnloadTest::CloseInspectedBrowser,
                          base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to exit application.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsApplicationClose) {
  RunBeforeUnloadTest(false, base::BindRepeating(&chrome::CloseAllBrowsers));
}

// Tests that inspected tab gets closed if devtools renderer
// becomes unresponsive during beforeunload event interception.
// @see http://crbug.com/322380
// Disabled because of http://crbug.com/410327
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       DISABLED_TestUndockedDevToolsUnresponsive) {
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window =
      OpenDevToolWindowOnWebContents(GetInspectedTab(), false);

  auto runner = base::MakeRefCounted<content::MessageLoopRunner>();
  DevToolsWindowTesting::Get(devtools_window)
      ->SetCloseCallback(runner->QuitClosure());

  ASSERT_TRUE(content::ExecJs(
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents(),
      "window.addEventListener('beforeunload',"
      "function(event) { while (true); });"));
  CloseInspectedTab();
  runner->Run();
}

// Tests that closing worker inspector window does not cause browser crash
// @see http://crbug.com/323031
// TODO(crbug.com/40703256): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       DISABLED_TestWorkerWindowClosing) {
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window =
      OpenDevToolWindowOnWebContents(GetInspectedTab(), false);

  OpenDevToolsPopupWindow(devtools_window);
  CloseDevToolsPopupWindow(devtools_window);
}

// Tests that BeforeUnload event gets called on devtools that are opened
// on another devtools.
// TODO(crbug.com/40645764): Re-enable this test.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       DISABLED_TestDevToolsOnDevTools) {
  LoadTestPage(kDebuggerTestPage);

  std::vector<DevToolsWindow*> windows;
  std::vector<std::unique_ptr<content::WebContentsDestroyedWatcher>>
      close_observers;
  content::WebContents* inspected_web_contents = GetInspectedTab();
  for (int i = 0; i < 3; ++i) {
    DevToolsWindow* devtools_window =
        OpenDevToolWindowOnWebContents(inspected_web_contents, i == 0);
    windows.push_back(devtools_window);
    close_observers.push_back(
        std::make_unique<content::WebContentsDestroyedWatcher>(
            DevToolsWindowTesting::Get(devtools_window)->main_web_contents()));
    inspected_web_contents =
        DevToolsWindowTesting::Get(devtools_window)->main_web_contents();
  }

  InjectBeforeUnloadListener(
      DevToolsWindowTesting::Get(windows[0])->main_web_contents());
  InjectBeforeUnloadListener(
      DevToolsWindowTesting::Get(windows[2])->main_web_contents());
  // Try to close second devtools.
  {
    chrome::CloseWindow(DevToolsWindowTesting::Get(windows[1])->browser());
    CancelModalDialog();
    base::RunLoop().RunUntilIdle();
    // The second devtools hasn't closed.
    EXPECT_EQ(windows[1],
              DevToolsWindow::GetInstanceForInspectedWebContents(
                  DevToolsWindowTesting::Get(windows[0])->main_web_contents()));
  }
  // Try to close browser window.
  {
    chrome::CloseWindow(browser());
    AcceptModalDialog();
    CancelModalDialog();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(browser(), BrowserList::GetInstance()->get(0));
  }
  // Try to exit application.
  {
    chrome::CloseAllBrowsers();
    AcceptModalDialog();
    AcceptModalDialog();
    ui_test_utils::WaitForBrowserToClose(browser());
  }
  for (auto& close_observer : close_observers) {
    close_observer->Wait();
  }
}

// Tests scripts panel showing.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestShowScriptsTab) {
  RunTest("testShowScriptsTab", kDebuggerTestPage);
}

// Tests recorder panel showing.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestShowRecorderTab) {
  RunTest("testShowRecorderTab", kDebuggerTestPage);
}

// Tests that chrome.devtools extension is correctly exposed.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, TestDevToolsExtensionAPI) {
  LoadExtension("devtools_extension");
  RunTest("waitForTestResultsInConsole", kArbitraryPage);
}

class DevtoolsPanelForceUpdateTest : public DevToolsExtensionTest,
                                     public testing::WithParamInterface<bool> {
 public:
  DevtoolsPanelForceUpdateTest() = default;
};

// Tests that, for a extension using the devtools api to create a custom
// devtools panel, we can navigate to the panel successfully (whether devtools
// force update is enabled or not). Also confirms that we can manually browse to
// an extension resource file before and after loading devtools. Regression test
// for crbug.com/333670353.
IN_PROC_BROWSER_TEST_P(DevtoolsPanelForceUpdateTest, NavigateToDevtoolsPanel) {
  // Install devtools panel extension.
  const Extension* extension = LoadExtensionFromPath(
      test_extensions_dir_.AppendASCII("devtools_extension_force_update"));
  ASSERT_TRUE(extension) << "Failed to load extension.";

  // Manually navigate to an extension resource page to confirm the extension
  // resource can be loaded.
  GURL extension_resource_url =
      GURL(base::StringPrintf("chrome-extension://%s/extension_resource.html",
                              extension->id().c_str()));
  ExtensionTestMessageListener extension_resource_loaded_listener(
      "extension_resource.html loaded");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_resource_url));
  {
    SCOPED_TRACE("waiting for extension resource to load");
    ASSERT_TRUE(extension_resource_loaded_listener.WaitUntilSatisfied());
  }

  // Set whether the devtools panel has the "Update on reload" checkbox checked.
  bool force_update_service_workers = GetParam();
  content::ServiceWorkerContext* service_worker_context =
      extensions::service_worker_test_utils::GetServiceWorkerContext(
          browser()->profile());
  ASSERT_TRUE(service_worker_context);
  service_worker_context->SetForceUpdateOnPageLoadForTesting(
      force_update_service_workers);

  // Open the devtools panel/window on an arbitrary page.
  OpenDevToolsWindow(kDebuggerTestPage, /*is_docked=*/true);

  // Navigate to the extension's custom devtools panel.
  ExtensionTestMessageListener extension_test_panel_loaded_listener(
      "extension devtools panel loaded");
  SwitchToExtensionPanel(window_, extension, "TestPanel");
  {
    SCOPED_TRACE(
        "Waiting for the panel extension to finish loading, it should output "
        "\"PASS\" to the console");
    RunTestFunction(window_, "waitForTestResultsInConsole");
  }
  // Verify the panel loaded successfully by checking that the extension
  // service worker received a message from the panel.
  {
    SCOPED_TRACE("waiting for extension devtools panel to load");
    EXPECT_TRUE(extension_test_panel_loaded_listener.WaitUntilSatisfied());
  }

  // Manually navigate to the extension resource page again to confirm the
  // extension resource can be still be loaded.
  ExtensionTestMessageListener
      extension_resource_loaded_after_devtools_listener(
          "extension_resource.html loaded");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_resource_url));
  {
    SCOPED_TRACE(
        "waiting for extension resource to load after loading devtools");
    EXPECT_TRUE(
        extension_resource_loaded_after_devtools_listener.WaitUntilSatisfied());
  }
}

INSTANTIATE_TEST_SUITE_P(ForceUpdateOff,
                         DevtoolsPanelForceUpdateTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(ForceUpdateOn,
                         DevtoolsPanelForceUpdateTest,
                         testing::Values(true));

// Tests that http Iframes within the visible devtools panel for the devtools
// extension are rendered in their own processes and not in the devtools process
// or the extension's process.  This is tested because this is one of the
// extension pages with devtools access
// (https://developer.chrome.com/extensions/devtools).  Also tests that frames
// with data URLs and about:blank URLs are rendered in the devtools process,
// unless a web OOPIF navigates itself to about:blank, in which case it does not
// end up back in the devtools process.  Also tests that when a web IFrame is
// navigated back to a devtools extension page, it gets put back in the devtools
// process.
// http://crbug.com/570483
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       HttpIframeInDevToolsExtensionPanel) {
  // Install the dynamically-generated extension.
  const Extension* extension =
      LoadExtensionForTest("Devtools Extension", "panel_devtools_page.html",
                           "/multi_frame_page.html");
  ASSERT_TRUE(extension);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the extension's panel to finish loading -- it'll output 'PASS'
  // when it's installed. waitForTestResultsInConsole waits until that 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the panel is loaded, switch to it.
  SwitchToExtensionPanel(window_, extension, "iframe-panel");
  EXPECT_TRUE(content::WaitForLoadStop(main_web_contents()));

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(7U, rfhs.size());

  // This test creates a page with the following frame tree:
  // - DevTools
  //   - devtools_page from DevTools extension
  //   - Panel (DevTools extension)
  //     - iframe (DevTools extension)
  //       - about:blank
  //       - data:
  //       - web URL

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* devtools_extension_devtools_page_rfh =
      ChildFrameAt(main_devtools_rfh, 0);
  RenderFrameHost* devtools_extension_panel_rfh =
      ChildFrameAt(main_devtools_rfh, 1);
  RenderFrameHost* panel_frame_rfh =
      ChildFrameAt(devtools_extension_panel_rfh, 0);
  RenderFrameHost* about_blank_frame_rfh = ChildFrameAt(panel_frame_rfh, 0);
  RenderFrameHost* data_frame_rfh = ChildFrameAt(panel_frame_rfh, 1);
  RenderFrameHost* web_frame_rfh = ChildFrameAt(panel_frame_rfh, 2);

  GURL web_url = embedded_test_server()->GetURL("a.com", "/title3.html");
  GURL about_blank_url = GURL(url::kAboutBlankURL);
  GURL data_url = GURL("data:text/html,foo");

  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(extension->GetResourceURL("/panel_devtools_page.html"),
            devtools_extension_devtools_page_rfh->GetLastCommittedURL());
  EXPECT_EQ(extension->GetResourceURL("/panel.html"),
            devtools_extension_panel_rfh->GetLastCommittedURL());
  EXPECT_EQ(extension->GetResourceURL("/multi_frame_page.html"),
            panel_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(about_blank_url, about_blank_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(data_url, data_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(web_url, web_frame_rfh->GetLastCommittedURL());

  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extensions_instance =
      devtools_extension_devtools_page_rfh->GetSiteInstance();

  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_TRUE(
      extensions_instance->GetSiteURL().SchemeIs(extensions::kExtensionScheme));

  EXPECT_NE(devtools_instance, extensions_instance);
  EXPECT_EQ(extensions_instance,
            devtools_extension_panel_rfh->GetSiteInstance());
  EXPECT_EQ(extensions_instance, panel_frame_rfh->GetSiteInstance());
  EXPECT_EQ(extensions_instance, about_blank_frame_rfh->GetSiteInstance());
  EXPECT_EQ(extensions_instance, data_frame_rfh->GetSiteInstance());

  EXPECT_EQ(web_url.host(),
            web_frame_rfh->GetSiteInstance()->GetSiteURL().host());
  EXPECT_NE(devtools_instance, web_frame_rfh->GetSiteInstance());
  EXPECT_NE(extensions_instance, web_frame_rfh->GetSiteInstance());

  // Check that if the web iframe navigates itself to about:blank, it stays in
  // the web SiteInstance.
  std::string about_blank_javascript = "location.href='about:blank';";

  content::TestNavigationManager web_about_blank_manager(main_web_contents(),
                                                         about_blank_url);

  ASSERT_TRUE(content::ExecJs(web_frame_rfh, about_blank_javascript));

  ASSERT_TRUE(web_about_blank_manager.WaitForNavigationFinished());
  // After navigation, the frame may change.
  web_frame_rfh = ChildFrameAt(panel_frame_rfh, 2);

  EXPECT_EQ(about_blank_url, web_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(web_url.host(),
            web_frame_rfh->GetSiteInstance()->GetSiteURL().host());
  EXPECT_NE(devtools_instance, web_frame_rfh->GetSiteInstance());
  EXPECT_NE(extensions_instance, web_frame_rfh->GetSiteInstance());

  // Check that if the web IFrame is navigated back to a devtools extension
  // page, it gets put back in the devtools process.
  GURL extension_simple_url =
      extension->GetResourceURL("/simple_test_page.html");
  std::string renavigation_javascript =
      "location.href='" + extension_simple_url.spec() + "';";

  content::TestNavigationManager renavigation_manager(main_web_contents(),
                                                      extension_simple_url);

  ASSERT_TRUE(content::ExecJs(web_frame_rfh, renavigation_javascript));

  ASSERT_TRUE(renavigation_manager.WaitForNavigationFinished());

  // The old RFH is no longer valid after the renavigation, so we must get the
  // new one.
  RenderFrameHost* extension_simple_frame_rfh =
      ChildFrameAt(panel_frame_rfh, 2);

  EXPECT_EQ(extension_simple_url,
            extension_simple_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(extensions_instance, extension_simple_frame_rfh->GetSiteInstance());
}

// Tests that http Iframes within the sidebar pane page for the devtools
// extension that is visible in the elements panel are rendered in their own
// processes and not in the devtools process or the extension's process.  This
// is tested because this is one of the extension pages with devtools access
// (https://developer.chrome.com/extensions/devtools).  http://crbug.com/570483
// TODO(crbug.com/40944663): Enable once the test is fixed.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_HttpIframeInDevToolsExtensionSideBarPane \
  DISABLED_HttpIframeInDevToolsExtensionSideBarPane
#else
#define MAYBE_HttpIframeInDevToolsExtensionSideBarPane \
  HttpIframeInDevToolsExtensionSideBarPane
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_HttpIframeInDevToolsExtensionSideBarPane) {
  GURL web_url = embedded_test_server()->GetURL("a.com", "/title3.html");

  // Install the dynamically-generated extension.
  const Extension* extension = LoadExtensionForTest(
      "Devtools Extension", "sidebarpane_devtools_page.html", web_url.spec());
  ASSERT_TRUE(extension);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the extension's sidebarpane to finish loading -- it'll output
  // 'PASS' when it's installed. waitForTestResultsInConsole waits until that
  // 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the sidebarpane is loaded, switch to it.
  content::TestNavigationManager web_manager(main_web_contents(), web_url);
  SwitchToPanel(window_, "elements");
  // This is a bit of a hack to switch to the sidebar pane in the elements panel
  // that the Iframe has been added to.
  SwitchToPanel(window_, "iframe-pane");
  ASSERT_TRUE(web_manager.WaitForNavigationFinished());

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(4U, rfhs.size());

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* devtools_extension_devtools_page_rfh =
      ChildFrameAt(main_devtools_rfh, 0);
  RenderFrameHost* devtools_sidebar_pane_extension_rfh =
      ChildFrameAt(main_devtools_rfh, 1);
  RenderFrameHost* http_iframe_rfh =
      ChildFrameAt(devtools_sidebar_pane_extension_rfh, 0);

  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(extension->GetResourceURL("/sidebarpane_devtools_page.html"),
            devtools_extension_devtools_page_rfh->GetLastCommittedURL());
  EXPECT_EQ(extension->GetResourceURL("/panel.html"),
            devtools_sidebar_pane_extension_rfh->GetLastCommittedURL());
  EXPECT_EQ(web_url, http_iframe_rfh->GetLastCommittedURL());

  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extensions_instance =
      devtools_extension_devtools_page_rfh->GetSiteInstance();
  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_NE(devtools_instance, extensions_instance);
  EXPECT_EQ(extensions_instance,
            devtools_extension_devtools_page_rfh->GetSiteInstance());
  EXPECT_EQ(extensions_instance,
            devtools_sidebar_pane_extension_rfh->GetSiteInstance());
  EXPECT_EQ(web_url.host(),
            http_iframe_rfh->GetSiteInstance()->GetSiteURL().host());
  EXPECT_NE(devtools_instance, http_iframe_rfh->GetSiteInstance());
  EXPECT_NE(extensions_instance, http_iframe_rfh->GetSiteInstance());
}

// Tests that http Iframes within the devtools background page, which is
// different from the extension's background page, are rendered in their own
// processes and not in the devtools process or the extension's process.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       HttpIframeInDevToolsExtensionDevtools) {
  // Install the dynamically-generated extension.
  const Extension* extension =
      LoadExtensionForTest("Devtools Extension", "web_devtools_page.html",
                           "" /* panel_iframe_src */);
  ASSERT_TRUE(extension);

  // Wait for a 'DONE' message sent from popup_iframe.html, indicating that it
  // loaded successfully.
  std::unique_ptr<content::DOMMessageQueue> message_queue;
  std::string message;

  // OpenDevToolsWindow() internally creates and initializes a WebContents,
  // which we need to listen to messages from; to ensure that we don't miss
  // the message, listen for that WebContents being created and set up a
  // DOMMessageQueue for it.
  {
    auto subscription = content::RegisterWebContentsCreationCallback(
        // Note that we only care about the first WebContents; for all
        // subsequent WebContents, message_queue will already be non-null.
        base::BindLambdaForTesting([&](content::WebContents* contents) {
          if (!message_queue) {
            message_queue =
                std::make_unique<content::DOMMessageQueue>(contents);
          }
        }));
    OpenDevToolsWindow(kDebuggerTestPage, false);
  }

  ASSERT_TRUE(message_queue)
      << "OpenDevToolsWindow must create at least one WebContents";
  while (true) {
    ASSERT_TRUE(message_queue->WaitForMessage(&message));
    if (message == "\"DONE\"") {
      break;
    }
  }

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(3U, rfhs.size());

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* devtools_extension_devtools_page_rfh =
      ChildFrameAt(main_devtools_rfh, 0);
  RenderFrameHost* http_iframe_rfh =
      ChildFrameAt(devtools_extension_devtools_page_rfh, 0);

  GURL web_url = embedded_test_server()->GetURL("a.com", "/popup_iframe.html");

  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(extension->GetResourceURL("/web_devtools_page.html"),
            devtools_extension_devtools_page_rfh->GetLastCommittedURL());
  EXPECT_EQ(web_url, http_iframe_rfh->GetLastCommittedURL());

  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extensions_instance =
      devtools_extension_devtools_page_rfh->GetSiteInstance();

  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_NE(devtools_instance, extensions_instance);
  EXPECT_EQ(web_url.host(),
            http_iframe_rfh->GetSiteInstance()->GetSiteURL().host());
  EXPECT_NE(devtools_instance, http_iframe_rfh->GetSiteInstance());
  EXPECT_NE(extensions_instance, http_iframe_rfh->GetSiteInstance());
}

// Tests that iframes to a non-devtools extension embedded in a devtools
// extension will be isolated from devtools and the devtools extension.
// http://crbug.com/570483
// Disabled due to flakiness https://crbug.com/1062802
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DISABLED_NonDevToolsExtensionInDevToolsExtension) {
  // Install the dynamically-generated non-devtools extension.
  const Extension* non_devtools_extension =
      LoadExtensionForTest("Non-DevTools Extension", "" /* devtools_page */,
                           "" /* panel_iframe_src */);
  ASSERT_TRUE(non_devtools_extension);

  GURL non_dt_extension_test_url =
      non_devtools_extension->GetResourceURL("/simple_test_page.html");

  // Install the dynamically-generated devtools extension.
  const Extension* devtools_extension =
      LoadExtensionForTest("Devtools Extension", "panel_devtools_page.html",
                           non_dt_extension_test_url.spec());
  ASSERT_TRUE(devtools_extension);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the extension's panel to finish loading -- it'll output 'PASS'
  // when it's installed. waitForTestResultsInConsole waits until that 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the panel is loaded, switch to it.
  content::TestNavigationManager non_devtools_manager(
      main_web_contents(), non_dt_extension_test_url);
  SwitchToExtensionPanel(window_, devtools_extension, "iframe-panel");
  ASSERT_TRUE(non_devtools_manager.WaitForNavigationFinished());

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(4U, rfhs.size());

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* devtools_extension_devtools_page_rfh =
      ChildFrameAt(main_devtools_rfh, 0);
  RenderFrameHost* devtools_extension_panel_rfh =
      ChildFrameAt(main_devtools_rfh, 1);
  RenderFrameHost* non_devtools_extension_rfh =
      ChildFrameAt(devtools_extension_panel_rfh, 0);

  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(devtools_extension->GetResourceURL("/panel_devtools_page.html"),
            devtools_extension_devtools_page_rfh->GetLastCommittedURL());
  EXPECT_EQ(devtools_extension->GetResourceURL("/panel.html"),
            devtools_extension_panel_rfh->GetLastCommittedURL());
  EXPECT_EQ(non_dt_extension_test_url,
            non_devtools_extension_rfh->GetLastCommittedURL());

  // simple_test_page.html's frame should be in |non_devtools_extension|'s
  // process, not in devtools or |devtools_extension|'s process.
  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extensions_instance =
      devtools_extension_devtools_page_rfh->GetSiteInstance();
  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_NE(devtools_instance, extensions_instance);
  EXPECT_EQ(extensions_instance,
            devtools_extension_panel_rfh->GetSiteInstance());
  EXPECT_EQ(non_dt_extension_test_url.DeprecatedGetOriginAsURL(),
            non_devtools_extension_rfh->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(devtools_instance, non_devtools_extension_rfh->GetSiteInstance());
  EXPECT_NE(extensions_instance, non_devtools_extension_rfh->GetSiteInstance());
}

// Tests that if a devtools extension's devtools panel page has a subframe to a
// page for another devtools extension, the subframe is rendered in the devtools
// process as well.  http://crbug.com/570483
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DevToolsExtensionInDevToolsExtension) {
  // Install the dynamically-generated extension.
  const Extension* devtools_b_extension =
      LoadExtensionForTest("Devtools Extension B", "simple_devtools_page.html",
                           "" /* panel_iframe_src */);
  ASSERT_TRUE(devtools_b_extension);

  GURL extension_b_page_url =
      devtools_b_extension->GetResourceURL("/simple_test_page.html");

  // Install another dynamically-generated extension.  This extension's
  // panel.html's iframe will point to an extension b URL.
  const Extension* devtools_a_extension =
      LoadExtensionForTest("Devtools Extension A", "panel_devtools_page.html",
                           extension_b_page_url.spec());
  ASSERT_TRUE(devtools_a_extension);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the extension's panel to finish loading -- it'll output 'PASS'
  // when it's installed. waitForTestResultsInConsole waits until that 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the panel is loaded, switch to it.
  content::TestNavigationManager extension_b_manager(main_web_contents(),
                                                     extension_b_page_url);
  SwitchToExtensionPanel(window_, devtools_a_extension, "iframe-panel");
  ASSERT_TRUE(extension_b_manager.WaitForNavigationFinished());

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(5U, rfhs.size());

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();

  RenderFrameHost* devtools_extension_a_devtools_rfh =
      content::FrameMatchingPredicate(
          main_web_contents()->GetPrimaryPage(),
          base::BindRepeating(&content::FrameHasSourceUrl,
                              devtools_a_extension->GetResourceURL(
                                  "/panel_devtools_page.html")));
  EXPECT_TRUE(devtools_extension_a_devtools_rfh);
  RenderFrameHost* devtools_extension_b_devtools_rfh =
      content::FrameMatchingPredicate(
          main_web_contents()->GetPrimaryPage(),
          base::BindRepeating(&content::FrameHasSourceUrl,
                              devtools_b_extension->GetResourceURL(
                                  "/simple_devtools_page.html")));
  EXPECT_TRUE(devtools_extension_b_devtools_rfh);

  RenderFrameHost* devtools_extension_a_panel_rfh =
      ChildFrameAt(main_devtools_rfh, 2);
  RenderFrameHost* devtools_extension_b_frame_rfh =
      ChildFrameAt(devtools_extension_a_panel_rfh, 0);

  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(devtools_a_extension->GetResourceURL("/panel_devtools_page.html"),
            devtools_extension_a_devtools_rfh->GetLastCommittedURL());
  EXPECT_EQ(devtools_b_extension->GetResourceURL("/simple_devtools_page.html"),
            devtools_extension_b_devtools_rfh->GetLastCommittedURL());
  EXPECT_EQ(devtools_a_extension->GetResourceURL("/panel.html"),
            devtools_extension_a_panel_rfh->GetLastCommittedURL());
  EXPECT_EQ(extension_b_page_url,
            devtools_extension_b_frame_rfh->GetLastCommittedURL());

  // Main extension frame should be loaded in the extensions process. Nested
  // iframes should be loaded consistently with any other extensions iframes
  // (in or out of process).
  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extension_a_instance =
      devtools_extension_a_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extension_b_instance =
      devtools_extension_b_devtools_rfh->GetSiteInstance();
  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_NE(devtools_instance, extension_a_instance);
  EXPECT_NE(devtools_instance, extension_b_instance);
  EXPECT_NE(extension_a_instance, extension_b_instance);
  EXPECT_EQ(extension_a_instance,
            devtools_extension_a_panel_rfh->GetSiteInstance());
  EXPECT_EQ(extension_b_instance,
            devtools_extension_b_frame_rfh->GetSiteInstance());
}

// Tests that a devtools extension can still have subframes to itself in a
// "devtools page" and that they will be rendered within the extension process
// as well, not in some other process.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, DevToolsExtensionInItself) {
  // Install the dynamically-generated extension.
  const Extension* extension =
      LoadExtensionForTest("Devtools Extension", "panel_devtools_page.html",
                           "/simple_test_page.html");
  ASSERT_TRUE(extension);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the extension's panel to finish loading -- it'll output 'PASS'
  // when it's installed. waitForTestResultsInConsole waits until that 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the panel is loaded, switch to it.
  GURL extension_test_url = extension->GetResourceURL("/simple_test_page.html");
  content::TestNavigationManager test_page_manager(main_web_contents(),
                                                   extension_test_url);
  SwitchToExtensionPanel(window_, extension, "iframe-panel");
  ASSERT_TRUE(test_page_manager.WaitForNavigationFinished());

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(4U, rfhs.size());

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* devtools_extension_devtools_page_rfh =
      ChildFrameAt(main_devtools_rfh, 0);
  RenderFrameHost* devtools_extension_panel_rfh =
      ChildFrameAt(main_devtools_rfh, 1);
  RenderFrameHost* devtools_extension_panel_frame_rfh =
      ChildFrameAt(devtools_extension_panel_rfh, 0);

  // Extension frames should be in the extensions process, including
  // simple_test_page.html
  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(extension->GetResourceURL("/panel_devtools_page.html"),
            devtools_extension_devtools_page_rfh->GetLastCommittedURL());
  EXPECT_EQ(extension->GetResourceURL("/panel.html"),
            devtools_extension_panel_rfh->GetLastCommittedURL());
  EXPECT_EQ(extension_test_url,
            devtools_extension_panel_frame_rfh->GetLastCommittedURL());

  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  content::SiteInstance* extensions_instance =
      devtools_extension_devtools_page_rfh->GetSiteInstance();
  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_EQ(extensions_instance,
            devtools_extension_panel_rfh->GetSiteInstance());
  EXPECT_EQ(extensions_instance,
            devtools_extension_panel_frame_rfh->GetSiteInstance());
}

// Tests that a devtools (not a devtools extension) Iframe can be injected into
// devtools.  http://crbug.com/570483
// crbug.com/1124981: flaky on win
#if BUILDFLAG(IS_WIN)
#define MAYBE_DevtoolsInDevTools DISABLED_DevtoolsInDevTools
#else
#define MAYBE_DevtoolsInDevTools DevtoolsInDevTools
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_DevtoolsInDevTools) {
  GURL devtools_url = GURL(chrome::kChromeUIDevToolsURL);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  std::string javascript =
      "var devtoolsFrame = document.createElement('iframe');"
      "document.body.appendChild(devtoolsFrame);"
      "devtoolsFrame.src = '" +
      devtools_url.spec() + "';";

  RenderFrameHost* main_devtools_rfh =
      main_web_contents()->GetPrimaryMainFrame();

  content::TestNavigationManager manager(main_web_contents(), devtools_url);
  ASSERT_TRUE(content::ExecJs(main_devtools_rfh, javascript));
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  std::vector<RenderFrameHost*> rfhs =
      CollectAllRenderFrameHosts(main_web_contents());
  EXPECT_EQ(2U, rfhs.size());
  RenderFrameHost* devtools_iframe_rfh = ChildFrameAt(main_devtools_rfh, 0);
  EXPECT_TRUE(main_devtools_rfh->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  EXPECT_EQ(devtools_url, devtools_iframe_rfh->GetLastCommittedURL());
  content::SiteInstance* devtools_instance =
      main_devtools_rfh->GetSiteInstance();
  EXPECT_TRUE(
      devtools_instance->GetSiteURL().SchemeIs(content::kChromeDevToolsScheme));
  EXPECT_EQ(devtools_instance, devtools_iframe_rfh->GetSiteInstance());

  std::string message =
      content::EvalJs(devtools_iframe_rfh, "self.origin").ExtractString();
  EXPECT_EQ(devtools_url.DeprecatedGetOriginAsURL().spec(), message + "/");
}

// Some web features, when used from an extension, are subject to browser-side
// security policy enforcement. Make sure they work properly from inside a
// devtools extension.
// ToDo(993982): The test is flaky (timeout, crash, and fail) on several builds:
// Debug, Windows, Mac, MSan, and ASan.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DISABLED_DevToolsExtensionSecurityPolicyGrants) {
  auto dir = std::make_unique<extensions::TestExtensionDir>();

  dir->WriteManifest(base::Value::Dict()
                         .Set("name", "Devtools Panel")
                         .Set("version", "1")
                         // Allow the script we stuff into the 'blob:' URL:
                         .Set("content_security_policy",
                              "script-src 'self' "
                              "'sha256-uv9gxBEOFchPzak3TK6O39RdKxJeZvfha9zOHGam"
                              "TB4='; "
                              "object-src 'none'")
                         .Set("manifest_version", 2)
                         .Set("devtools_page", "devtools.html"));

  dir->WriteFile(
      FILE_PATH_LITERAL("devtools.html"),
      "<html><head><script src='devtools.js'></script></head></html>");

  dir->WriteFile(
      FILE_PATH_LITERAL("devtools.js"),
      "chrome.devtools.panels.create('the_panel_name',\n"
      "    null,\n"
      "    'panel.html',\n"
      "    function(panel) {\n"
      "      chrome.devtools.inspectedWindow.eval('console.log(\"PASS\")');\n"
      "    }\n"
      ");\n");

  dir->WriteFile(FILE_PATH_LITERAL("panel.html"),
                 "<html><body>A panel."
                 "<script src='blob_xhr.js'></script>"
                 "<script src='blob_iframe.js'></script>"
                 "</body></html>");
  // Creating blobs from chrome-extension:// origins is only permitted if the
  // process has been granted permission to commit 'chrome-extension' schemes.
  dir->WriteFile(
      FILE_PATH_LITERAL("blob_xhr.js"),
      "var blob_url = URL.createObjectURL(new Blob(['xhr blob contents']));\n"
      "var xhr = new XMLHttpRequest();\n"
      "xhr.open('GET', blob_url, true);\n"
      "xhr.onload = function (e) {\n"
      "    domAutomationController.send(xhr.response);\n"
      "};\n"
      "xhr.send(null);\n");
  dir->WriteFile(
      FILE_PATH_LITERAL("blob_iframe.js"),
      "var payload = `"
      "<html><body>iframe blob contents"
      "<script>"
      "    domAutomationController.send(document.body.innerText);\n"
      "</script></body></html>"
      "`;"
      "document.body.appendChild(document.createElement('iframe')).src ="
      "    URL.createObjectURL(new Blob([payload], {type: 'text/html'}));");
  // Install the extension.
  const Extension* extension = LoadExtensionFromPath(dir->UnpackedPath());
  ASSERT_TRUE(extension);

  // Open a devtools window.
  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the panel extension to finish loading -- it'll output 'PASS'
  // when it's installed. waitForTestResultsInConsole waits until that 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the panel is loaded, switch to it. We'll wait until we
  // see a 'DONE' message sent from popup_iframe.html, indicating that it
  // loaded successfully.
  content::DOMMessageQueue message_queue(main_web_contents());
  SwitchToExtensionPanel(window_, extension, "the_panel_name");
  std::string message;
  while (true) {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    if (message == "\"xhr blob contents\"") {
      break;
    }
  }
  while (true) {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    if (message == "\"iframe blob contents\"") {
      break;
    }
  }
}

// Disabled on Windows due to flakiness. http://crbug.com/183649
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestDevToolsExtensionMessaging \
  DISABLED_TestDevToolsExtensionMessaging
#else
#define MAYBE_TestDevToolsExtensionMessaging TestDevToolsExtensionMessaging
#endif

// Tests that chrome.devtools extension can communicate with background page
// using extension messaging.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_TestDevToolsExtensionMessaging) {
  LoadExtension("devtools_messaging");
  RunTest("waitForTestResultsInConsole", kArbitraryPage);
}

// Tests that chrome.experimental.devtools extension is correctly exposed
// when the extension has experimental permission.
IN_PROC_BROWSER_TEST_F(DevToolsExperimentalExtensionTest,
                       TestDevToolsExperimentalExtensionAPI) {
  LoadExtension("devtools_experimental");
  RunTest("waitForTestResultsInConsole", kArbitraryPage);
}

// Tests that a content script is in the scripts list.
//
// TODO(crbug.com/40933538): Flaky on "Linux Tests (dbg)(1)".
#if BUILDFLAG(IS_LINUX)
#define MAYBE_TestContentScriptIsPresent DISABLED_TestContentScriptIsPresent
#else
#define MAYBE_TestContentScriptIsPresent TestContentScriptIsPresent
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_TestContentScriptIsPresent) {
  LoadExtension("simple_content_script");
  RunTest("testContentScriptIsPresent", kPageWithContentScript);
}

// Tests that console selector shows correct context names.
// TODO(crbug.com/328131890): Test is flaky on multiple platforms. Tends to time
// out when trying to open the devtools window.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DISABLED_TestConsoleContextNames) {
  LoadExtension("simple_content_script");
  RunTest("testConsoleContextNames", kPageWithContentScript);
}

// TODO(crbug.com/40930033): Flaky on Linux and ChromeOS Tests.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CantInspectNewTabPage DISABLED_CantInspectNewTabPage
#else
#define MAYBE_CantInspectNewTabPage CantInspectNewTabPage
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, MAYBE_CantInspectNewTabPage) {
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#chrome://newtab/"}));
}

// TODO(crbug.com/40943634): Re-enable the test once it is fixed.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CantInspectChromeScheme DISABLED_CantInspectChromeScheme
#else
#define MAYBE_CantInspectChromeScheme CantInspectChromeScheme
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, MAYBE_CantInspectChromeScheme) {
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#chrome://version/"}));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, CantInspectDevtoolsScheme) {
  LoadExtension("can_inspect_url");
  RunTest(
      "waitForTestResultsAsMessage",
      base::StrCat({kArbitraryPage,
                    "#devtools://devtools/bundled/devtools_compatibility.js"}));
}

// TODO(crbug.com/369074885): Flaky on Linux
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CantInspectViewSourceDevtoolsScheme \
  DISABLED_CantInspectViewSourceDevtoolsScheme
#else
#define MAYBE_CantInspectViewSourceDevtoolsScheme \
  CantInspectViewSourceDevtoolsScheme
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_CantInspectViewSourceDevtoolsScheme) {
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage,
                        "#view-source:devtools://devtools/bundled/"
                        "devtools_compatibility.js"}));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, CantInspectComponentExtension) {
  std::string extension_id = BuildComponentExtension();
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#chrome-extension://", extension_id,
                        "/simple_test_page.html"}));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, CantInspectRemoteNewTabPage) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server.Start());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  TemplateURLData data;
  data.SetShortName(u"example.com");
  data.SetURL("https://example.com/url?bar={searchTerms}");
  data.new_tab_url =
      https_test_server.GetURL("localhost", "/devtools/empty.html").spec();

  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#", data.new_tab_url}));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       CantInspectViewSourceComponentExtension) {
  std::string extension_id = BuildComponentExtension();
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#view-source:chrome-extension://",
                        extension_id, "/simple_test_page.html"}));
}

// Flaky on several platforms: https://crbug.com/1487065
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DISABLED_CantInspectFileUrlWithoutFileAccess) {
  LoadExtension("can_inspect_url");
  std::string file_url =
      net::FilePathToFileURL(
          base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
              .AppendASCII("content/test/data/devtools/navigation.html"))
          .spec();
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#", file_url}));
}

// Test that an extension's side panel view is inspectable whether or not the
// `kDevToolsTabTarget` flag is enabled.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       CanInspectExtensionSidePanelView) {
  base::FilePath side_panel_extension_dir =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("extensions/api_test/side_panel");

  // Load an extension and wait for its side panel view to be shown.
  scoped_refptr<const extensions::Extension> extension = LoadExtensionFromPath(
      side_panel_extension_dir.AppendASCII("simple_default"));
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener default_path_listener("default_path");
  extensions::OpenExtensionSidePanel(*browser(), extension->id());
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());

  content::WebContents* side_panel_contents =
      browser()
          ->GetFeatures()
          .extension_side_panel_manager()
          ->GetExtensionCoordinatorForTesting(extension->id())
          ->GetHostWebContentsForTesting();
  ASSERT_TRUE(side_panel_contents);
  EXPECT_TRUE(content::WaitForLoadStop(side_panel_contents));

  std::vector<RenderFrameHost*> frames =
      CollectAllRenderFrameHosts(side_panel_contents);
  ASSERT_EQ(1u, frames.size());
  RenderFrameHost* side_panel_host = frames[0];

  // Inspect the extension's side panel view and check that the top level html
  // tag is inspected.
  DevToolsWindowCreationObserver observer;
  DevToolsWindow::InspectElement(side_panel_host, 0, 0);
  observer.WaitForLoad();
  DevToolsWindow* window = observer.devtools_window();

  DispatchOnTestSuite(window, "testInspectedElementIs", "HTML");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

// TODO(crbug.com/41495883): Re-enable on linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CanInspectExtensionOffscreenDoc \
  DISABLED_CanInspectExtensionOffscreenDoc
#else
#define MAYBE_CanInspectExtensionOffscreenDoc CanInspectExtensionOffscreenDoc
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_CanInspectExtensionOffscreenDoc) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  const extensions::Extension* extension =
      LoadExtensionFromPath(test_dir.UnpackedPath());

  // Create an offscreen document and wait for it to load.
  GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  std::unique_ptr<extensions::OffscreenDocumentHost> offscreen_document =
      std::make_unique<extensions::OffscreenDocumentHost>(
          *extension,
          extensions::ProcessManager::Get(browser()->profile())
              ->GetSiteInstanceForURL(offscreen_url)
              .get(),
          offscreen_url);
  {
    extensions::ExtensionHostTestHelper offscreen_waiter(browser()->profile(),
                                                         extension->id());
    offscreen_waiter.RestrictToType(
        extensions::mojom::ViewType::kOffscreenDocument);
    offscreen_document->CreateRendererSoon();
    offscreen_waiter.WaitForHostCompletedFirstLoad();
  }

  // Get the list of inspectable views for the extension.
  auto get_info_function = base::MakeRefCounted<
      extensions::api::DeveloperPrivateGetExtensionInfoFunction>();
  std::optional<base::Value> result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          get_info_function.get(),
          content::JsReplace(R"([$1])", extension->id()), browser()->profile());
  ASSERT_TRUE(result);
  auto info =
      extensions::api::developer_private::ExtensionInfo::FromValue(*result);
  ASSERT_TRUE(info);

  // The only inspectable view should be the offscreen document. Validate the
  // metadata.
  ASSERT_EQ(1u, info->views.size());
  const extensions::api::developer_private::ExtensionView& view =
      info->views[0];
  EXPECT_EQ(extensions::api::developer_private::ViewType::kOffscreenDocument,
            view.type);
  content::WebContents* offscreen_contents =
      offscreen_document->host_contents();
  EXPECT_EQ(offscreen_url.spec(), view.url);
  EXPECT_EQ(offscreen_document->render_process_host()->GetID(),
            view.render_process_id);
  EXPECT_EQ(offscreen_contents->GetPrimaryMainFrame()->GetRoutingID(),
            view.render_view_id);
  EXPECT_FALSE(view.incognito);
  EXPECT_FALSE(view.is_iframe);

  // The document shouldn't currently be under inspection.
  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(offscreen_contents));
  DevToolsWindowCreationObserver observer;

  // Call the API function to inspect the offscreen document.
  auto dev_tools_function = base::MakeRefCounted<
      extensions::api::DeveloperPrivateOpenDevToolsFunction>();
  extensions::api_test_utils::RunFunction(
      dev_tools_function.get(),
      content::JsReplace(
          R"([{"renderViewId": $1,
               "renderProcessId": $2,
               "extensionId": $3
            }])",
          view.render_view_id, view.render_process_id, extension->id()),
      browser()->profile());

  // Validate that the devtools window is now shown.
  observer.WaitForLoad();
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      view.render_process_id, view.render_view_id);
  ASSERT_TRUE(rfh);
  DevToolsWindow::InspectElement(rfh, 0, 0);
  DispatchOnTestSuite(observer.devtools_window(), "testInspectedElementIs",
                      "HTML");
}

class DevToolsExtensionFileAccessTest : public DevToolsExtensionTest {
 protected:
  void Run(bool allow_file_access, const std::string& url_scheme) {
    extensions::TestExtensionDir dir;

    dir.WriteManifest(BuildExtensionManifest("File Access", "devtools.html"));
    dir.WriteFile(
        FILE_PATH_LITERAL("devtools.html"),
        "<html><head><script src='devtools.js'></script></head></html>");
    dir.WriteFile(FILE_PATH_LITERAL("devtools.js"),
                  base::StringPrintf(R"(
        chrome.devtools.inspectedWindow.getResources((resources) => {
          const hasFile = !!resources.find(r => r.url.startsWith('file:'));
          setInterval(() => {
            top.postMessage(
                {testOutput: (hasFile == %d) ? 'PASS' : 'FAIL'}, '*');
          }, 10);
        });)",
                                     allow_file_access));

    std::string file_url =
        net::FilePathToFileURL(
            base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
                .AppendASCII("content/test/data/devtools/navigation.html"))
            .spec();

    const Extension* extension =
        LoadExtensionFromPath(dir.UnpackedPath(), allow_file_access);
    ASSERT_TRUE(extension);

    std::string url = base::StringPrintf(
        R"(data:text/html,<script>//%%23%%20sourceMappingURL=data:application/json,{"version":3,"sources":["%s:%s"]}</script>)",
        url_scheme.c_str(), file_url.c_str() + strlen("file:///"));
    OpenDevToolsWindow(url, false);
    RunTestFunction(window_, "waitForTestResultsAsMessage");
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsExtensionFileAccessTest,
                       CanGetFileResourceWithFileAccess) {
  Run(true, "file:///");
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionFileAccessTest,
                       CantGetFileResourceWithoutFileAccess) {
  Run(false, "file:///");
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionFileAccessTest,
                       CantGetFileResourceWithoutFileAccessNoSlashes) {
  Run(false, "file:");
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionFileAccessTest,
                       CantGetFileResourceWithoutFileAccessMixedCase) {
  Run(false, "fILe:");
}

// Tests that scripts are not duplicated after Scripts Panel switch.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestNoScriptDuplicatesOnPanelSwitch) {
  RunTest("testNoScriptDuplicatesOnPanelSwitch", kDebuggerTestPage);
}

// Tests that debugger works correctly if pause event occurs when DevTools
// frontend is being loaded.
// Flaky on win and linux: crbug.com/1092924.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestPauseWhenLoadingDevTools DISABLED_TestPauseWhenLoadingDevTools
#else
#define MAYBE_TestPauseWhenLoadingDevTools TestPauseWhenLoadingDevTools
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestPauseWhenLoadingDevTools) {
  RunTest("testPauseWhenLoadingDevTools", kPauseWhenLoadingDevTools);
}

// Tests network timing.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestNetworkTiming) {
  RunTest("testNetworkTiming", kSlowTestPage);
}

// Tests network size.
// TODO(crbug/40218872): Enable this flaky test. This is flaky on Linux debug
// build. See also: https://crrev.com/c/2772698
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_TestNetworkSize DISABLED_TestNetworkSize
#else
#define MAYBE_TestNetworkSize TestNetworkSize
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestNetworkSize) {
  RunTest("testNetworkSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestNetworkSyncSize) {
  RunTest("testNetworkSyncSize", kChunkedTestPage);
}

namespace {

bool InterceptURLLoad(content::URLLoaderInterceptor::RequestParams* params) {
  const GURL& url = params->url_request.url;
  if (!base::EndsWith(url.path(), kPushTestResource,
                      base::CompareCase::SENSITIVE)) {
    return false;
  }

  auto response = network::mojom::URLResponseHead::New();

  response->headers = new net::HttpResponseHeaders("200 OK\r\n\r\n");

  auto start_time = base::TimeTicks::Now() - base::Milliseconds(10);
  response->request_start = start_time;
  response->response_start = base::TimeTicks::Now();
  response->request_time = base::Time::Now() - base::Milliseconds(10);
  response->response_time = base::Time::Now();

  auto& load_timing = response->load_timing;
  load_timing.request_start = start_time;
  load_timing.request_start_time = response->request_time;
  load_timing.send_start = start_time;
  load_timing.send_end = base::TimeTicks::Now();
  load_timing.receive_headers_end = base::TimeTicks::Now();
  load_timing.push_start = start_time - base::Milliseconds(100);
  if (url.query() != kPushUseNullEndTime) {
    load_timing.push_end = base::TimeTicks::Now();
  }

  // The response's body is empty. The pipe is not filled.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  EXPECT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  params->client->OnReceiveResponse(std::move(response),
                                    std::move(consumer_handle), std::nullopt);
  params->client->OnComplete(network::URLLoaderCompletionStatus());
  return true;
}

}  // namespace

// TODO(crbug.com/40116595) Flaky
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_TestNetworkPushTime) {
  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(InterceptURLLoad));

  OpenDevToolsWindow(kPushTestPage, false);
  GURL push_url = embedded_test_server()->GetURL(kPushTestResource);

  DispatchOnTestSuite(window_, "testPushTimes", push_url.spec().c_str());

  CloseDevToolsWindow();
}

#if BUILDFLAG(IS_WIN)
// Flaky on Windows: https://crbug.com/1087320
#define MAYBE_TestDOMWarnings DISABLED_TestDOMWarnings
#else
#define MAYBE_TestDOMWarnings TestDOMWarnings
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestDOMWarnings) {
  RunTest("testDOMWarnings", kDOMWarningsTestPage);
}

// Tests that console messages are not duplicated on navigation back.
#if BUILDFLAG(IS_WIN) || defined(MEMORY_SANITIZER)
// Flaking on windows swarm try runs: crbug.com/409285.
// Also flaking on MSan runs: crbug.com/1182861
#define MAYBE_TestConsoleOnNavigateBack DISABLED_TestConsoleOnNavigateBack
#else
#define MAYBE_TestConsoleOnNavigateBack TestConsoleOnNavigateBack
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestConsoleOnNavigateBack) {
  RunTest("testConsoleOnNavigateBack", kNavigateBackTestPage);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Flaking on linux runs, see crbug.com/990692.
#define MAYBE_TestDeviceEmulation DISABLED_TestDeviceEmulation
#else
#define MAYBE_TestDeviceEmulation TestDeviceEmulation
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestDeviceEmulation) {
  RunTest("testDeviceMetricsOverrides", "about:blank");
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, TestDispatchKeyEventDoesNotCrash) {
  RunTest("testDispatchKeyEventDoesNotCrash", "about:blank");
}

class BrowserAutofillManagerTestDelegateDevtoolsImpl
    : public autofill::BrowserAutofillManagerTestDelegate {
 public:
  explicit BrowserAutofillManagerTestDelegateDevtoolsImpl(
      WebContents* inspected_contents)
      : inspected_contents_(inspected_contents) {}

  BrowserAutofillManagerTestDelegateDevtoolsImpl(
      const BrowserAutofillManagerTestDelegateDevtoolsImpl&) = delete;
  BrowserAutofillManagerTestDelegateDevtoolsImpl& operator=(
      const BrowserAutofillManagerTestDelegateDevtoolsImpl&) = delete;

  ~BrowserAutofillManagerTestDelegateDevtoolsImpl() override {}

  void DidPreviewFormData() override {}

  void DidFillFormData() override {}

  void DidShowSuggestions() override {
    // Set an override for the minimum 500 ms threshold before enter key strokes
    // are accepted.
    if (base::WeakPtr<autofill::AutofillSuggestionController> controller =
            autofill::ChromeAutofillClient::FromWebContentsForTesting(
                inspected_contents_.get())
                ->suggestion_controller_for_testing()) {
      test_api(static_cast<autofill::AutofillPopupControllerImpl&>(*controller))
          .DisableThreshold(true);
    }
    ASSERT_TRUE(content::ExecJs(inspected_contents_,
                                "console.log('didShowSuggestions');"));
  }

  void DidHideSuggestions() override {}

 private:
  const raw_ptr<WebContents> inspected_contents_;
};

// Disabled. Failing on MacOS MSAN. See https://crbug.com/849129.
// Also failing on Linux. See https://crbug.com/1187693.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TestDispatchKeyEventShowsAutoFill \
  DISABLED_TestDispatchKeyEventShowsAutoFill
#else
#define MAYBE_TestDispatchKeyEventShowsAutoFill \
  TestDispatchKeyEventShowsAutoFill
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestDispatchKeyEventShowsAutoFill) {
  OpenDevToolsWindow(kDispatchKeyEventShowsAutoFill, false);
  GetInspectedTab()->Focus();

  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          GetInspectedTab()->GetPrimaryMainFrame());
  auto& autofill_manager = static_cast<autofill::BrowserAutofillManager&>(
      autofill_driver->GetAutofillManager());
  BrowserAutofillManagerTestDelegateDevtoolsImpl autofill_test_delegate(
      GetInspectedTab());
  autofill_test_delegate.Observe(autofill_manager);

  RunTestFunction(window_, "testDispatchKeyEventShowsAutoFill");
  CloseDevToolsWindow();
}

// Tests that allowed unhandled shortcuts are forwarded from inspected page
// into devtools frontend
IN_PROC_BROWSER_TEST_F(DevToolsTest, testKeyEventUnhandled) {
  OpenDevToolsWindow("about:blank", true);
  RunTestFunction(window_, "testKeyEventUnhandled");
  CloseDevToolsWindow();
}

// Tests that the keys that are forwarded from the browser update
// when their shortcuts change
IN_PROC_BROWSER_TEST_F(DevToolsTest, testForwardedKeysChanged) {
  OpenDevToolsWindow("about:blank", true);
  RunTestFunction(window_, "testForwardedKeysChanged");
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, testCloseActionRecorded) {
  base::UserActionTester user_action_tester;
  OpenDevToolsWindow("about:blank", true);
  CloseDevToolsWindow();

  EXPECT_EQ(1, user_action_tester.GetActionCount("DevTools_Close"));
}

// Test that showing a certificate in devtools does not crash the process.
// Disabled on windows as this opens a modal in its own thread, which leads to a
// test timeout.
#if BUILDFLAG(IS_WIN)
#define MAYBE_testShowCertificate DISABLED_testShowCertificate
#else
#define MAYBE_testShowCertificate testShowCertificate
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_testShowCertificate) {
  OpenDevToolsWindow("about:blank", true);
  RunTestFunction(window_, "testShowCertificate");
  CloseDevToolsWindow();
}

// Tests that settings are stored in profile correctly.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestSettings) {
  OpenDevToolsWindow("about:blank", true);
  RunTestFunction(window_, "testSettings");
  CloseDevToolsWindow();
}

// Tests that external navigation from inspector page is always handled by
// DevToolsWindow and results in inspected page navigation.  See also
// https://crbug.com/180555.
IN_PROC_BROWSER_TEST_F(DevToolsTest, TestDevToolsExternalNavigation) {
  OpenDevToolsWindow(kDebuggerTestPage, true);
  GURL url = embedded_test_server()->GetURL(kNavigateBackTestPage);
  ui_test_utils::UrlLoadObserver observer(url);
  ASSERT_TRUE(
      content::ExecJs(main_web_contents(),
                      std::string("window.location = \"") + url.spec() + "\""));
  observer.Wait();

  ASSERT_TRUE(main_web_contents()->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  ASSERT_EQ(url, GetInspectedTab()->GetLastCommittedURL());
  CloseDevToolsWindow();
}

// Tests that toolbox window is loaded when DevTools window is undocked.
// TODO(crbug.com/40929457) - Fix this failing browser test.
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_TestToolboxLoadedUndocked) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  ASSERT_TRUE(toolbox_web_contents());
  DevToolsWindow* on_self =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), false);
  EXPECT_FALSE(DevToolsWindowTesting::Get(on_self)->toolbox_web_contents());
  DevToolsWindowTesting::CloseDevToolsWindowSync(on_self);
  CloseDevToolsWindow();
}

// Tests that toolbox window is not loaded when DevTools window is docked.
// TODO(crbug.com/40836594): Re-enable this test
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_TestToolboxNotLoadedDocked) {
  OpenDevToolsWindow(kDebuggerTestPage, true);
  EXPECT_FALSE(toolbox_web_contents());
  DevToolsWindow* on_self =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), false);
  EXPECT_FALSE(DevToolsWindowTesting::Get(on_self)->toolbox_web_contents());
  DevToolsWindowTesting::CloseDevToolsWindowSync(on_self);
  CloseDevToolsWindow();
}

// Tests that inspector will reattach to inspected page when it is reloaded
// after a crash. See http://crbug.com/101952
// Disabled. it doesn't check anything right now: http://crbug.com/461790
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_TestReattachAfterCrash) {
  RunTest("testReattachAfterCrash", kArbitraryPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, TestPageWithNoJavaScript) {
  OpenDevToolsWindow("about:blank", false);
  std::string result;
  ASSERT_EQ(
      "function",
      content::EvalJs(
          main_web_contents(),
          "'' + (window.uiTests && (typeof uiTests.dispatchOnTestSuite));"))
      << "DevTools front-end is broken.";
  CloseDevToolsWindow();
}

class DevToolsAutoOpenerTest : public DevToolsTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAutoOpenDevToolsForTabs);
    observer_ = std::make_unique<DevToolsWindowCreationObserver>();
  }

 protected:
  std::unique_ptr<DevToolsWindowCreationObserver> observer_;
};

// TODO(crbug.com/40742539): Flaky on debug builds.
#if !defined(NDEBUG)
#define MAYBE_TestAutoOpenForTabs DISABLED_TestAutoOpenForTabs
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// TODO(crbug.com/40817460): Flaky failures
#define MAYBE_TestAutoOpenForTabs DISABLED_TestAutoOpenForTabs
#else
#define MAYBE_TestAutoOpenForTabs TestAutoOpenForTabs
#endif
IN_PROC_BROWSER_TEST_F(DevToolsAutoOpenerTest, MAYBE_TestAutoOpenForTabs) {
  {
    DevToolsWindowCreationObserver observer;
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0, GURL("about:blank"),
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                       false));
    observer.WaitForLoad();
  }
  Browser* new_browser = nullptr;
  {
    DevToolsWindowCreationObserver observer;
    new_browser = CreateBrowser(browser()->profile());
    observer.WaitForLoad();
  }
  {
    DevToolsWindowCreationObserver observer;
    ASSERT_TRUE(AddTabAtIndexToBrowser(new_browser, 0, GURL("about:blank"),
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                       false));
    observer.WaitForLoad();
  }
  observer_->CloseAllSync();
}

class DevToolsReattachAfterCrashTest : public DevToolsTest {
 protected:
  void RunTestWithPanel(const char* panel_name) {
    OpenDevToolsWindow("about:blank", false);
    SwitchToPanel(window_, panel_name);
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    content::RenderProcessHostWatcher crash_observer(
        GetInspectedTab(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(blink::kChromeUICrashURL)));
    crash_observer.Wait();
    content::TestNavigationObserver navigation_observer(GetInspectedTab(), 1);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
  }
};

// TODO(crbug.com/40936829): Reenable after fixing consistent Windows failure.
IN_PROC_BROWSER_TEST_F(DevToolsReattachAfterCrashTest,
                       DISABLED_TestReattachAfterCrashOnTimeline) {
  RunTestWithPanel("timeline");
}

// TODO(crbug.com/40938244): Gardener 2023-10-26: Flaky on bots.
IN_PROC_BROWSER_TEST_F(DevToolsReattachAfterCrashTest,
                       DISABLED_TestReattachAfterCrashOnNetwork) {
  RunTestWithPanel("network");
}

// Very flaky on Linux only.  http://crbug.com/1216219
#if BUILDFLAG(IS_LINUX)
#define MAYBE_AutoAttachToWindowOpen DISABLED_AutoAttachToWindowOpen
#else
#define MAYBE_AutoAttachToWindowOpen AutoAttachToWindowOpen
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_AutoAttachToWindowOpen) {
  OpenDevToolsWindow(kWindowOpenTestPage, false);
  DevToolsWindowTesting::Get(window_)->SetOpenNewWindowForPopups(true);
  DevToolsWindowCreationObserver observer;
  ASSERT_TRUE(content::ExecJs(GetInspectedTab(),
                              "window.open('window_open.html', '_blank');"));
  observer.WaitForLoad();
  DispatchOnTestSuite(observer.devtools_window(), "waitForDebuggerPaused");
  DevToolsWindowTesting::CloseDevToolsWindowSync(observer.devtools_window());
  CloseDevToolsWindow();
}

// TODO(crbug.com/40704377) Flaky
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_SecondTabAfterDevTools) {
  OpenDevToolsWindow(kDebuggerTestPage, true);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL(kDebuggerTestPage),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* second = browser()->tab_strip_model()->GetActiveWebContents();

  scoped_refptr<content::DevToolsAgentHost> agent(
      GetOrCreateDevToolsHostForWebContents(second));
  EXPECT_EQ("page", agent->GetType());

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(WorkerDevToolsTest, InspectSharedWorker) {
  GURL url = embedded_test_server()->GetURL(kSharedWorkerTestPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  scoped_refptr<DevToolsAgentHost> host =
      WaitForFirstSharedWorker(kSharedWorkerTestWorker);
  OpenDevToolsWindow(host);
  RunTestFunction(window_, "testSharedWorker");
  CloseDevToolsWindow();
}

// Flaky on multiple platforms. See http://crbug.com/1263230
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_PauseInSharedWorkerInitialization \
  DISABLED_PauseInSharedWorkerInitialization
#else
#define MAYBE_PauseInSharedWorkerInitialization \
  PauseInSharedWorkerInitialization
#endif
IN_PROC_BROWSER_TEST_F(WorkerDevToolsTest,
                       MAYBE_PauseInSharedWorkerInitialization) {
  GURL url = embedded_test_server()->GetURL(kReloadSharedWorkerTestPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  scoped_refptr<DevToolsAgentHost> host =
      WaitForFirstSharedWorker(kReloadSharedWorkerTestWorker);
  OpenDevToolsWindow(host);

  // We should make sure that the worker inspector has loaded before
  // terminating worker.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization1");

  host->Close();

  // Reload page to restart the worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Wait until worker script is paused on the debugger statement.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization2");
  CloseDevToolsWindow();
}

class DevToolsAgentHostTest : public InProcessBrowserTest {};

// Tests DevToolsAgentHost retention by its target.
IN_PROC_BROWSER_TEST_F(DevToolsAgentHostTest, TestAgentHostReleased) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WebContents* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  DevToolsAgentHost* agent_raw =
      DevToolsAgentHost::GetOrCreateFor(web_contents).get();
  const std::string agent_id = agent_raw->GetId();
  ASSERT_EQ(agent_raw, DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost cannot be found by id";
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost is not released when the tab is closed";
}

class RemoteDebuggingTest : public extensions::ExtensionApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "9222");
    command_line->AppendSwitchASCII(switches::kRemoteAllowOrigins, "*");

    // Override the extension root path.
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("devtools");
  }
};

// Fails on CrOS. crbug.com/431399
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_RemoteDebugger DISABLED_RemoteDebugger
#else
// TODO(crbug.com/41478279): Flaky on all platforms.
#define MAYBE_RemoteDebugger DISABLED_RemoteDebugger
#endif
IN_PROC_BROWSER_TEST_F(RemoteDebuggingTest, MAYBE_RemoteDebugger) {
  ASSERT_TRUE(RunExtensionTest("target_list")) << message_;
}

IN_PROC_BROWSER_TEST_F(RemoteDebuggingTest, DiscoveryPage) {
  ASSERT_TRUE(RunExtensionTest("discovery_page")) << message_;
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, PolicyDisallowed) {
  DisallowDevTools(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, PolicyDisallowedCloseConnection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);

  // Policy change must close the connection
  DisallowDevTools(browser());
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

using ManifestLocation = extensions::mojom::ManifestLocation;
class DevToolsDisallowedForForceInstalledExtensionsPolicyTest
    : public extensions::ExtensionBrowserTest {
 public:
  // Installs an extensions, using the specified manifest location.
  // Contains assertions - callers should wrap calls of this method in
  // |ASSERT_NO_FATAL_FAILURE|.
  void InstallExtensionWithLocation(ManifestLocation location,
                                    std::string* extension_id) {
    base::FilePath crx_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &crx_path);
    crx_path = crx_path.AppendASCII("devtools")
                   .AppendASCII("extensions")
                   .AppendASCII("options.crx");
    const Extension* extension = InstallExtension(crx_path, 1, location);
    ASSERT_TRUE(extension);
    *extension_id = extension->id();
  }

  // Same as above, but also fills |*out_web_contents| with a |WebContents|
  // that has been navigated to the installed extension.
  void InstallExtensionAndOpen(ManifestLocation location,
                               content::WebContents** out_web_contents) {
    std::string extension_id;
    InstallExtensionWithLocation(location, &extension_id);
    GURL url("chrome-extension://" + extension_id + "/options.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    *out_web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void PolicyInstallExtensionAndOpen(content::WebContents** out_web_contents) {
    InstallExtensionAndOpen(ManifestLocation::kExternalPolicyDownload,
                            out_web_contents);
  }

  void InstallComponentExtensionAndOpen(
      content::WebContents** out_web_contents) {
    InstallExtensionAndOpen(ManifestLocation::kExternalComponent,
                            out_web_contents);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsDisallowedForForceInstalledExtensionsPolicyTest,
                       DisallowedForExternalPolicyDownloadExtension) {
  // DevTools are disallowed for policy-installed extensions by default.
  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(PolicyInstallExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsDisallowedForForceInstalledExtensionsPolicyTest,
                       DisallowedForComponentExtensionForManagedUsers) {
  // DevTools are disallowed for component extensions by default for managed
  // profiles.
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(InstallComponentExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsDisallowedForForceInstalledExtensionsPolicyTest,
                       AllowedForComponentExtensionForNonManagedUsers) {
  // DevTools are allowed for component extensions by default non-managed
  // profiles.
  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(InstallComponentExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsDisallowedForForceInstalledExtensionsPolicyTest,
                       ExtensionConnectionClosedOnPolicyChange) {
  AllowDevTools(browser());
  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(PolicyInstallExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);

  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));

  // Policy change must close the connection with the policy installed
  // extension.
  DisallowDevToolsForForceInstalledExtenions(browser());
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsDisallowedForForceInstalledExtensionsPolicyTest,
                       ClosedAfterNavigationToExtension) {
  // DevTools are disallowed for policy-installed extensions by default.
  std::string extension_id;
  ASSERT_NO_FATAL_FAILURE(InstallExtensionWithLocation(
      ManifestLocation::kExternalPolicyDownload, &extension_id));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // It's possible to open DevTools for about:blank.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));

  // Navigating to extension page should close DevTools.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-extension://" + extension_id + "/options.html")));
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsDisallowedForForceInstalledExtensionsPolicyTest,
                       AboutBlankConnectionKeptOnPolicyChange) {
  AllowDevTools(browser());

  std::string extension_id;
  ASSERT_NO_FATAL_FAILURE(InstallExtensionWithLocation(
      ManifestLocation::kExternalPolicyDownload, &extension_id));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));

  // Policy change to must not disrupt CDP coneciton unrelated to a force
  // installed extension.
  DisallowDevToolsForForceInstalledExtenions(browser());
  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

class DevToolsAllowedByCommandLineSwitch
    : public DevToolsDisallowedForForceInstalledExtensionsPolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    // Same as `switches::kForceDevToolsAvailable`, but used as a
    // literal here so it's possible to verify that the switch does not apply on
    // non-ChromeOS platforms.
    const std::string kForceDevToolsAvailableBase = "force-devtools-available";
#if BUILDFLAG(IS_CHROMEOS)
    ASSERT_EQ(kForceDevToolsAvailableBase, switches::kForceDevToolsAvailable);
#endif
    command_line->AppendSwitch("--" + kForceDevToolsAvailableBase);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsAllowedByCommandLineSwitch,
                       SwitchOverridesPolicyOnChromeOS) {
  // DevTools are disallowed for policy-installed extensions by default.
  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(PolicyInstallExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  auto agent_host = GetOrCreateDevToolsHostForWebContents(web_contents);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
#else
  EXPECT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
#endif
}

class DevToolsPixelOutputTests : public DevToolsTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    DevToolsTest::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }
};

// This test enables switches::kUseGpuInTests which causes false positives
// with MemorySanitizer. This is also flakey on many configurations.
// See https://crbug.com/510291
IN_PROC_BROWSER_TEST_F(DevToolsPixelOutputTests,
                       DISABLED_TestScreenshotRecording) {
  RunTest("testScreenshotRecording", kArbitraryPage);
}

// This test enables switches::kUseGpuInTests which causes false positives
// with MemorySanitizer.
// Flaky on multiple platforms https://crbug.com/624215
IN_PROC_BROWSER_TEST_F(DevToolsPixelOutputTests,
                       DISABLED_TestLatencyInfoInstrumentation) {
  WebContents* web_contents = GetInspectedTab();
  OpenDevToolsWindow(kLatencyInfoTestPage, false);
  DispatchAndWait("startTimeline");

  for (int i = 0; i < 3; ++i) {
    SimulateMouseEvent(web_contents, blink::WebInputEvent::Type::kMouseMove,
                       gfx::Point(30, 60));
    DispatchInPageAndWait("waitForEvent", "mousemove");
  }

  SimulateMouseClickAt(web_contents, 0,
                       blink::WebPointerProperties::Button::kLeft,
                       gfx::Point(30, 60));
  DispatchInPageAndWait("waitForEvent", "click");

  SimulateMouseWheelEvent(web_contents, gfx::Point(300, 100),
                          gfx::Vector2d(0, 120),
                          blink::WebMouseWheelEvent::kPhaseBegan);
  DispatchInPageAndWait("waitForEvent", "wheel");

  SimulateTapAt(web_contents, gfx::Point(30, 60));
  DispatchInPageAndWait("waitForEvent", "gesturetap");

  DispatchAndWait("stopTimeline");
  RunTestMethod("checkInputEventsPresent", "MouseMove", "MouseDown",
                "MouseWheel", "GestureTap");

  CloseDevToolsWindow();
}

class DevToolsNetInfoTest : public DevToolsTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableNetworkInformationDownlinkMax);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsNetInfoTest, EmulateNetworkConditions) {
  RunTest("testEmulateNetworkConditions", kEmulateNetworkConditionsPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsNetInfoTest, OfflineNetworkConditions) {
  RunTest("testOfflineNetworkConditions", kEmulateNetworkConditionsPage);
}

class StaticURLDataSource : public content::URLDataSource {
 public:
  StaticURLDataSource(const std::string& source, const std::string& content)
      : source_(source), content_(content) {}

  StaticURLDataSource(const StaticURLDataSource&) = delete;
  StaticURLDataSource& operator=(const StaticURLDataSource&) = delete;

  ~StaticURLDataSource() override = default;

  // content::URLDataSource:
  std::string GetSource() override { return source_; }
  void StartDataRequest(const GURL& url,
                        const content::WebContents::Getter& wc_getter,
                        GotDataCallback callback) override {
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::string(content_)));
  }
  std::string GetMimeType(const GURL& url) override { return "text/html"; }
  bool ShouldAddContentSecurityPolicy() override { return false; }

 private:
  const std::string source_;
  const std::string content_;
};

class MockWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MockWebUIProvider(const std::string& source, const std::string& content)
      : source_(source), content_(content) {}

  MockWebUIProvider(const MockWebUIProvider&) = delete;
  MockWebUIProvider& operator=(const MockWebUIProvider&) = delete;

  ~MockWebUIProvider() override = default;

  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    content::URLDataSource::Add(
        Profile::FromWebUI(web_ui),
        std::make_unique<StaticURLDataSource>(source_, content_));
    return std::make_unique<content::WebUIController>(web_ui);
  }

 private:
  std::string source_;
  std::string content_;
};

// This tests checks that window is correctly initialized when DevTools is
// opened while navigation through history with forward and back actions.
// (crbug.com/627407)
// TODO(crbug.com/40267320): Deflake and re-enable this test.
IN_PROC_BROWSER_TEST_F(DevToolsTest,
                       DISABLED_TestWindowInitializedOnNavigateBack) {
  TestChromeWebUIControllerFactory test_factory;
  content::ScopedWebUIControllerFactoryRegistration factory_registration(
      &test_factory);
  MockWebUIProvider mock_provider("dummyurl",
                                  "<script>\n"
                                  "  window.abc = 239;\n"
                                  "  console.log(abc);\n"
                                  "</script>");
  test_factory.AddFactoryOverride(GURL("chrome://foo/dummyurl").host(),
                                  &mock_provider);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://foo/dummyurl")));
  DevToolsWindow* window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), true);
  chrome::DuplicateTab(browser());
  chrome::SelectPreviousTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  RunTestFunction(window, "testWindowInitializedOnNavigateBack");

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, TestRawHeadersWithRedirectAndHSTS) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_test_server.Start());
  GURL https_url = https_test_server.GetURL("localhost", "/devtools/image.png");
  base::Time expiry = base::Time::Now() + base::Days(1000);
  bool include_subdomains = false;
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  content::StoragePartition* partition =
      browser()->profile()->GetDefaultStoragePartition();
  base::RunLoop run_loop;
  partition->GetNetworkContext()->AddHSTS(
      https_url.host(), expiry, include_subdomains, run_loop.QuitClosure());
  run_loop.Run();

  OpenDevToolsWindow(kArbitraryPage, false);

  net::EmbeddedTestServer test_server2;
  test_server2.AddDefaultHandlers();
  ASSERT_TRUE(test_server2.Start());
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");
  GURL http_url = https_url.ReplaceComponents(replace_scheme);
  GURL redirect_url =
      test_server2.GetURL("/server-redirect?" + http_url.spec());

  DispatchOnTestSuite(window_, "testRawHeadersWithHSTS",
                      redirect_url.spec().c_str());
  CloseDevToolsWindow();
}

// Tests that OpenInNewTab filters URLs.
// TODO(crbug.com/40847130): Flaky on Windows and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_TestOpenInNewTabFilter DISABLED_TestOpenInNewTabFilter
#else
#define MAYBE_TestOpenInNewTabFilter TestOpenInNewTabFilter
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest, MAYBE_TestOpenInNewTabFilter) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  DevToolsUIBindings::Delegate* bindings_delegate_ =
      static_cast<DevToolsUIBindings::Delegate*>(window_);
  std::string test_url =
      embedded_test_server()->GetURL(kDebuggerTestPage).spec();
  const std::string self_blob_url =
      base::StringPrintf("blob:%s", test_url.c_str());
  const std::string self_filesystem_url =
      base::StringPrintf("filesystem:%s", test_url.c_str());

  // Pairs include a URL string and boolean whether it should be allowed.
  std::vector<std::pair<const std::string, const std::string>> tests = {
      {test_url, test_url},
      {"data:,foo", "data:,foo"},
      {"about://inspect", "about:blank"},
      {"chrome://inspect", "about:blank"},
      {"chrome://inspect/#devices", "about:blank"},
      {self_blob_url, self_blob_url},
      {"blob:chrome://inspect", "about:blank"},
      {self_filesystem_url, self_filesystem_url},
      {"filesystem:chrome://inspect", "about:blank"},
      {"view-source:http://chromium.org", "about:blank"},
      {"file:///", "about:blank"},
      {"about://gpu", "about:blank"},
      {"chrome://gpu", "about:blank"},
      {"chrome://crash", "about:blank"},
      {"", "about:blank"},
  };

  TabStripModel* tabs = browser()->tab_strip_model();
  int i = 0;
  for (const std::pair<const std::string, const std::string>& pair : tests) {
    bindings_delegate_->OpenInNewTab(pair.first);
    i++;

    std::string opened_url = tabs->GetWebContentsAt(i)->GetVisibleURL().spec();
    SCOPED_TRACE(
        base::StringPrintf("while testing URL: %s", pair.first.c_str()));
    EXPECT_EQ(opened_url, pair.second);
  }

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, TestOpenSearchResultsInNewTab) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  DevToolsUIBindings::Delegate* bindings_delegate_ =
      static_cast<DevToolsUIBindings::Delegate*>(window_);

  TabStripModel* tabs = browser()->tab_strip_model();

  bindings_delegate_->OpenSearchResultsInNewTab("test query");

  std::string opened_url = tabs->GetWebContentsAt(1)->GetVisibleURL().spec();
  EXPECT_EQ(
      opened_url,
      "https://www.google.com/search?q=test+query&sourceid=chrome&ie=UTF-8");

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, LoadNetworkResourceForFrontend) {
  std::string file_url =
      "file://" + base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
                      .AppendASCII("content/test/data/devtools/navigation.html")
                      .NormalizePathSeparatorsTo('/')
                      .AsUTF8Unsafe();

  GURL url(embedded_test_server()->GetURL("/"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/hello.html")));
  window_ =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), false);
  LoadLegacyFilesInFrontend(window_);
  RunTestMethod("testLoadResourceForFrontend", url.spec().c_str(),
                file_url.c_str());
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

// TODO(crbug.com/41435439) Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_CreateBrowserContext) {
  GURL url(embedded_test_server()->GetURL("/devtools/empty.html"));
  window_ = DevToolsWindowTesting::OpenDiscoveryDevToolsWindowSync(
      browser()->profile());
  RunTestMethod("testCreateBrowserContext", url.spec().c_str());
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

// TODO(crbug.com/40708597): Flaky.
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_DisposeEmptyBrowserContext) {
  window_ = DevToolsWindowTesting::OpenDiscoveryDevToolsWindowSync(
      browser()->profile());
  RunTestMethod("testDisposeEmptyBrowserContext");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

// TODO(crbug.com/40689291): Find a better strategy for testing protocol methods
// against non-headless Chrome.
IN_PROC_BROWSER_TEST_F(DevToolsTest, NewWindowFromBrowserContext) {
  window_ = DevToolsWindowTesting::OpenDiscoveryDevToolsWindowSync(
      browser()->profile());
  LoadLegacyFilesInFrontend(window_);
  RunTestMethod("testNewWindowFromBrowserContext");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsTest, InspectElement) {
  GURL url(embedded_test_server()->GetURL("a.com", "/devtools/oopif.html"));
  GURL iframe_url(
      embedded_test_server()->GetURL("b.com", "/devtools/oopif_frame.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager(tab, url);
  content::TestNavigationManager navigation_manager_iframe(tab, iframe_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_manager_iframe.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  std::vector<RenderFrameHost*> frames =
      CollectAllRenderFrameHosts(GetInspectedTab());
  ASSERT_EQ(2u, frames.size());
  ASSERT_NE(frames[0]->GetProcess(), frames[1]->GetProcess());
  RenderFrameHost* frame_host = frames[0]->GetParent() ? frames[0] : frames[1];

  DevToolsWindowCreationObserver observer;
  DevToolsWindow::InspectElement(frame_host, 100, 100);
  observer.WaitForLoad();
  DevToolsWindow* window = observer.devtools_window();

  DispatchOnTestSuite(window, "testInspectedElementIs", "INSPECTED-DIV");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, InspectElement) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/devtools/oopif_frame.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager(tab, url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  std::vector<RenderFrameHost*> frames =
      CollectAllRenderFrameHosts(GetInspectedTab());
  ASSERT_EQ(1u, frames.size());
  RenderFrameHost* frame_host = frames[0];

  DevToolsWindowCreationObserver observer;
  DevToolsWindow::InspectElement(frame_host, 100, 100);
  observer.WaitForLoad();
  DevToolsWindow* window = observer.devtools_window();

  DispatchOnTestSuite(window, "testInspectedElementIs", "INSPECTED-DIV");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, UKMTest) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  GURL url(
      embedded_test_server()->GetURL("a.com", "/devtools/oopif_frame.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  std::vector<RenderFrameHost*> frames =
      CollectAllRenderFrameHosts(GetInspectedTab());
  RenderFrameHost* frame_host = frames[0];
  DevToolsWindow::InspectElement(frame_host, 100, 100);

  // Make sure we are recording the UKM when DevTools are opened.
  auto ukm_entries = test_ukm_recorder.GetEntriesByName("DevTools.Opened");
  EXPECT_EQ(1u, ukm_entries.size());
  test_ukm_recorder.ExpectEntrySourceHasUrl(ukm_entries[0], url);
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, ExistsForWebContentsAfterClosing) {
  EXPECT_FALSE(content::DevToolsAgentHost::HasFor(GetInspectedTab()));

  // Simulate opening devtools for the current tab.
  OpenDevToolsWindow(kDebuggerTestPage, true);
  ASSERT_TRUE(content::DevToolsAgentHost::HasFor(GetInspectedTab()));

  // Closes devtools window for the current tab i.e. exit the devtools
  // inspector.
  CloseDevToolsWindow();

  // The devtools window instance still exists for the current tab even though
  // it is now closed.
  ASSERT_TRUE(content::DevToolsAgentHost::HasFor(GetInspectedTab()));
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, BrowserCloseWithBeforeUnload) {
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::REMOTE_DEBUGGING));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(tab,
                      "window.addEventListener('beforeunload',"
                      "function(event) { event.returnValue = 'Foo'; });"));
  content::PrepContentsForBeforeUnloadTest(tab);
  BrowserHandler handler(nullptr, std::string());
  handler.Close();
  ui_test_utils::WaitForBrowserToClose(browser());
}

// Flaky.
// TODO(crbug.com/40721876): Re-enable.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest,
                       DISABLED_BrowserCloseWithContextMenuOpened) {
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::REMOTE_DEBUGGING));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  auto callback = [](RenderViewContextMenu* context_menu) {
    BrowserHandler handler(nullptr, std::string());
    handler.Close();
  };
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
      base::BindOnce(callback));
  content::SimulateMouseClickAt(tab, 0, blink::WebMouseEvent::Button::kRight,
                                gfx::Point(15, 15));
  ui_test_utils::WaitForBrowserToClose(browser());
}

#if !BUILDFLAG(IS_CHROMEOS)
// Skip for ChromeOS because the keep alive is not created for ChromeOS.
// See https://crbug.com/1174627.
class KeepAliveDevToolsTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "0");
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }
};

IN_PROC_BROWSER_TEST_F(KeepAliveDevToolsTest, KeepsAliveUntilBrowserClose) {
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::REMOTE_DEBUGGING));
  chrome::NewEmptyWindow(ProfileManager::GetLastUsedProfile());
  EXPECT_FALSE(BrowserList::GetInstance()->empty());
  BrowserHandler handler(nullptr, std::string());
  handler.Close();
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::REMOTE_DEBUGGING));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

class DevToolsPolicyTest : public InProcessBrowserTest {
 protected:
  DevToolsPolicyTest() {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(DevToolsPolicyTest, OpenBlockedDevTools) {
  base::Value::List blocklist;
  blocklist.Append("devtools://*");
  policy::PolicyMap policies;
  policies.Set(policy::key::kURLBlocklist, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);
  provider_.UpdateChromePolicy(policies);

  WebContents* wc = browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<content::DevToolsAgentHost> agent(
      GetOrCreateDevToolsHostForWebContents(wc));
  DevToolsWindow::OpenDevToolsWindow(wc, DevToolsOpenedByAction::kUnknown);
  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent.get());
  if (window) {
    base::RunLoop run_loop;
    DevToolsWindowTesting::Get(window)->SetCloseCallback(
        run_loop.QuitClosure());
    run_loop.Run();
  }
  window = DevToolsWindow::FindDevToolsWindow(agent.get());
  ASSERT_EQ(nullptr, window);
}

class DevToolsExtensionHostsPolicyTest : public DevToolsExtensionTest {
 protected:
  DevToolsExtensionHostsPolicyTest() {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }
  void SetUpInProcessBrowserTestFixture() override {
    DevToolsExtensionTest::SetUpInProcessBrowserTestFixture();

    base::Value::Dict settings;
    settings.Set(
        "*", base::Value::Dict()
                 .Set(extensions::schema_constants::kPolicyBlockedHosts,
                      base::Value::List().Append("*://*.example.com"))
                 .Set(extensions::schema_constants::kPolicyAllowedHosts,
                      base::Value::List().Append("*://public.example.com")));

    policy::PolicyMap policies;
    policies.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(std::move(settings)),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsExtensionHostsPolicyTest,
                       CantInspectBlockedHost) {
  GURL url(embedded_test_server()->GetURL("example.com", kArbitraryPage));
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#", url.spec()}));
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionHostsPolicyTest,
                       CantInspectBlockedSubdomainHost) {
  GURL url(embedded_test_server()->GetURL("foo.example.com", kArbitraryPage));
  LoadExtension("can_inspect_url");
  RunTest("waitForTestResultsAsMessage",
          base::StrCat({kArbitraryPage, "#", url.spec()}));
}

// TODO(crbug.com/333791064): Flaky on multiple Mac & Linux builders.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_CanInspectAllowedHttpHost DISABLED_CanInspectAllowedHttpHost
#else
#define MAYBE_CanInspectAllowedHttpHost CanInspectAllowedHttpHost
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionHostsPolicyTest,
                       MAYBE_CanInspectAllowedHttpHost) {
  GURL url(
      embedded_test_server()->GetURL("public.example.com", kArbitraryPage));
  extensions::TestExtensionDir dir;

  dir.WriteManifest(
      BuildExtensionManifest("Runtime Hosts Policy", "devtools.html"));
  dir.WriteFile(
      FILE_PATH_LITERAL("devtools.html"),
      "<html><head><script src='devtools.js'></script></head></html>");
  dir.WriteFile(FILE_PATH_LITERAL("devtools.js"),
                R"(
        chrome.devtools.network.getHAR((result) => {
          setInterval(() => {
            top.postMessage(
              {testOutput: ('entries' in result) ? 'PASS' : 'FAIL'},
              '*'
            );
          }, 10);
        });)");

  const Extension* extension = LoadExtensionFromPath(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  RunTest("waitForTestResultsAsMessage", url.spec());
}

// Times out. See https://crbug.com/819285.
IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsTest,
                       DISABLED_InputDispatchEventsToOOPIF) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/devtools/oopif-input.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "b.com", "/devtools/oopif-input-frame.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager(tab, url);
  content::TestNavigationManager navigation_manager_iframe(tab, iframe_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_manager_iframe.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  for (auto* frame : CollectAllRenderFrameHosts(GetInspectedTab())) {
    content::WaitForHitTestData(frame);
  }
  DevToolsWindow* window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), false);
  RunTestFunction(window, "testInputDispatchEventsToOOPIF");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

// See https://crbug.com/971241
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DISABLED_ExtensionWebSocketUserAgentOverride) {
  net::SpawnedTestServer websocket_server(
      net::SpawnedTestServer::TYPE_WS,
      base::FilePath(FILE_PATH_LITERAL("net/data/websocket")));
  websocket_server.set_websocket_basic_auth(false);
  ASSERT_TRUE(websocket_server.Start());
  uint16_t websocket_port = websocket_server.host_port_pair().port();

  LoadExtension("web_request");
  OpenDevToolsWindow(kEmptyTestPage, /* is_docked */ false);
  DispatchOnTestSuite(window_, "testExtensionWebSocketUserAgentOverride",
                      base::NumberToString(websocket_port).c_str());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, SourceMapsFromExtension) {
  const Extension* extension =
      LoadExtensionForTest("Non-DevTools Extension", "" /* devtools_page */,
                           "" /* panel_iframe_src */);
  ASSERT_TRUE(extension);
  OpenDevToolsWindow(kEmptyTestPage, /* is_docked */ false);
  DispatchOnTestSuite(window_, "testSourceMapsFromExtension",
                      extension->id().c_str());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, SourceMapsFromDevtools) {
  OpenDevToolsWindow(kEmptyTestPage, /* is_docked */ false);
  DispatchOnTestSuite(window_, "testSourceMapsFromDevtools");
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsTest,
                       DoesNotCrashOnSourceMapsFromUnknownScheme) {
  OpenDevToolsWindow(kEmptyTestPage, /* is_docked */ false);
  DispatchOnTestSuite(window_, "testDoesNotCrashOnSourceMapsFromUnknownScheme");
  CloseDevToolsWindow();
}

// TODO(crbug.com/40937316): Test is flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ExtensionWebSocketOfflineNetworkConditions \
  DISABLED_ExtensionWebSocketOfflineNetworkConditions
#else
#define MAYBE_ExtensionWebSocketOfflineNetworkConditions \
  ExtensionWebSocketOfflineNetworkConditions
#endif
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_ExtensionWebSocketOfflineNetworkConditions) {
  net::SpawnedTestServer websocket_server(
      net::SpawnedTestServer::TYPE_WS,
      base::FilePath(FILE_PATH_LITERAL("net/data/websocket")));
  websocket_server.set_websocket_basic_auth(false);
  ASSERT_TRUE(websocket_server.Start());
  uint16_t websocket_port = websocket_server.host_port_pair().port();

  LoadExtension("web_request");
  OpenDevToolsWindow(kEmptyTestPage, /* is_docked */ false);
  DispatchOnTestSuite(window_, "testExtensionWebSocketOfflineNetworkConditions",
                      base::NumberToString(websocket_port).c_str());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, IsDeveloperModeTrueHistogram) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDeveloperMode, true);
  base::HistogramTester histograms;
  const char* histogram_name = "Extensions.DevTools.UserIsInDeveloperMode";

  LoadExtension("devtools_extension");
  RunTest("waitForTestResultsInConsole", kArbitraryPage);

  histograms.ExpectBucketCount(histogram_name, true, 2);
}

IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, IsDeveloperModeFalseHistogram) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDeveloperMode, false);
  base::HistogramTester histograms;
  const char* histogram_name = "Extensions.DevTools.UserIsInDeveloperMode";

  LoadExtension("devtools_extension");
  RunTest("waitForTestResultsInConsole", kArbitraryPage);

  histograms.ExpectBucketCount(histogram_name, false, 2);
}

namespace {

class DevToolsLocalizationTest : public DevToolsTest {
 public:
  bool NavigatorLanguageMatches(const std::string& expected_locale) {
    return content::EvalJs(main_web_contents(),
                           "window.navigator.language === "
                           "'" +
                               expected_locale + "'")
        .ExtractBool();
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsLocalizationTest,
                       NavigatorLanguageMatchesApplicationLocaleDocked) {
  g_browser_process->SetApplicationLocale("es");

  OpenDevToolsWindow("about:blank", /* is_docked */ true);
  EXPECT_TRUE(NavigatorLanguageMatches("es"));
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsLocalizationTest,
                       NavigatorLanguageMatchesApplicationLocaleUndocked) {
  g_browser_process->SetApplicationLocale("es");

  OpenDevToolsWindow("about:blank", /* is_docked */ false);
  EXPECT_TRUE(NavigatorLanguageMatches("es"));
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsLocalizationTest,
                       AcceptedLanguageChangesWhileDevToolsIsOpen) {
  g_browser_process->SetApplicationLocale("es");

  OpenDevToolsWindow("about:blank", true);
  EXPECT_TRUE(NavigatorLanguageMatches("es"));

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetString(language::prefs::kAcceptLanguages, "de-DE");

  EXPECT_TRUE(NavigatorLanguageMatches("es"));

  CloseDevToolsWindow();
}

namespace {

class DevToolsFetchTest : public DevToolsTest {
 protected:
  content::EvalJsResult Fetch(
      const content::ToRenderFrameHost& execution_target,
      const std::string& url) {
    return content::EvalJs(execution_target, content::JsReplace(R"(
      (async function() {
        const response = await fetch($1);
        return response.status;
      })();
    )",
                                                                url));
  }

  content::EvalJsResult FetchFromDevToolsWindow(const std::string& url) {
    WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
    return Fetch(wc, url);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsFetchTest,
                       DevToolsFetchFromDevToolsSchemeUndocked) {
  OpenDevToolsWindow("about:blank", false);

  EXPECT_EQ(200, FetchFromDevToolsWindow(
                     "devtools://devtools/bundled/devtools_compatibility.js"));

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsFetchTest,
                       DevToolsFetchFromDevToolsSchemeDocked) {
  OpenDevToolsWindow("about:blank", true);

  EXPECT_EQ(200, FetchFromDevToolsWindow(
                     "devtools://devtools/bundled/devtools_compatibility.js"));

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsFetchTest, DevToolsFetchFromHttpDisallowed) {
  OpenDevToolsWindow("about:blank", true);

  const auto result = FetchFromDevToolsWindow("http://www.google.com");
  EXPECT_THAT(result.error,
              ::testing::StartsWith(
                  "a JavaScript error: \"TypeError: Failed to fetch\n"));

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsFetchTest, FetchFromDevToolsSchemeIsProhibited) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  const auto result =
      Fetch(GetInspectedTab(),
            "devtools://devtools/bundled/devtools_compatibility.js");
  EXPECT_THAT(result.error,
              ::testing::StartsWith(
                  "a JavaScript error: \"TypeError: Failed to fetch\n"));
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, HostBindingsSyncIntegration) {
  // Smoke test to make sure that `registerPreference` works from JavaScript.
  OpenDevToolsWindow("about:blank", true);
  LoadLegacyFilesInFrontend(window_);

  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  ASSERT_TRUE(content::ExecJs(
      wc, content::JsReplace(
              R"(
    Host.InspectorFrontendHost.setPreference($1, 'false');  // Disable sync.
    Host.InspectorFrontendHost.registerPreference(
        'synced_setting', {synced: true});
    Host.InspectorFrontendHost.registerPreference(
        'unsynced_setting', {synced: false});
    Host.InspectorFrontendHost.setPreference('synced_setting', 'synced value');
    Host.InspectorFrontendHost.setPreference(
        'unsynced_setting', 'unsynced value');
  )",
              DevToolsSettings::kSyncDevToolsPreferencesFrontendName)));

  const base::Value::Dict& synced_settings =
      browser()->profile()->GetPrefs()->GetDict(
          prefs::kDevToolsSyncedPreferencesSyncDisabled);
  const base::Value::Dict& unsynced_settings =
      browser()->profile()->GetPrefs()->GetDict(prefs::kDevToolsPreferences);
  EXPECT_EQ(*synced_settings.FindString("synced_setting"), "synced value");
  EXPECT_EQ(*unsynced_settings.FindString("unsynced_setting"),
            "unsynced value");
}

IN_PROC_BROWSER_TEST_F(DevToolsTest, NoJavascriptUrlOnDevtools) {
  // As per crbug/1115460 one could use javascript: url as a homepage URL and
  // then trigger homepage navigation (e.g. via keyboard shortcut) to execute in
  // the context of the privileged devtools frontend.
  OpenDevToolsWindow("about:blank", true);

  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  wc->GetController().LoadURL(GURL("javascript:window.xss=true"),
                              content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                              std::string());
  EXPECT_EQ(false, content::EvalJs(wc, "!!window.xss"));
}

// According to DevToolsTest.AutoAttachToWindowOpen, using
// `waitForDebuggerPaused()` is flaky on Linux.
// TODO(crbug.com/40770357): Enable the test on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PauseWhenSameOriginDebuggerAlreadyAttached \
  DISABLED_PauseWhenSameOriginDebuggerAlreadyAttached
#else
#define MAYBE_PauseWhenSameOriginDebuggerAlreadyAttached \
  PauseWhenSameOriginDebuggerAlreadyAttached
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest,
                       MAYBE_PauseWhenSameOriginDebuggerAlreadyAttached) {
  base::HistogramTester histograms;

  const GURL hello_url =
      embedded_test_server()->GetURL("a.test", "/hello.html");
  const GURL pause_url = embedded_test_server()->GetURL(
      "a.test", "/devtools/pause_when_loading_devtools.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), hello_url));
  DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), true);

  Browser* another_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(another_browser, pause_url));
  DevToolsWindow* another_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(
          another_browser->tab_strip_model()->GetWebContentsAt(0), true);
  DispatchOnTestSuite(another_window, "waitForDebuggerPaused");

  histograms.ExpectBucketCount(
      "DevTools.IsSameOriginDebuggerAttachedInAnotherRenderer", true, 1);
}

// According to DevToolsTest.AutoAttachToWindowOpen, using
// `waitForDebuggerPaused()` is flaky on Linux.
// TODO(crbug.com/40770357): Enable the test on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PauseWhenSameOriginDebuggerAlreadyPaused \
  DISABLED_PauseWhenSameOriginDebuggerAlreadyPaused
#else
#define MAYBE_PauseWhenSameOriginDebuggerAlreadyPaused \
  PauseWhenSameOriginDebuggerAlreadyPaused
#endif
IN_PROC_BROWSER_TEST_F(DevToolsTest,
                       MAYBE_PauseWhenSameOriginDebuggerAlreadyPaused) {
  base::HistogramTester histograms;

  const GURL pause_url = embedded_test_server()->GetURL(
      "a.test", "/devtools/pause_when_loading_devtools.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pause_url));
  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), true);
  DispatchOnTestSuite(window, "waitForDebuggerPaused");

  Browser* another_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(another_browser, pause_url));
  DevToolsWindow* another_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(
          another_browser->tab_strip_model()->GetWebContentsAt(0), true);
  DispatchOnTestSuite(another_window, "waitForDebuggerPaused");

  histograms.ExpectBucketCount(
      "DevTools.IsSameOriginDebuggerPausedInAnotherRenderer", true, 1);
}

class DevToolsSyncTest : public SyncTest {
 public:
  DevToolsSyncTest() : SyncTest(SyncTest::SINGLE_CLIENT) {}
};

IN_PROC_BROWSER_TEST_F(DevToolsSyncTest, GetSyncInformation) {
  // Smoke test to make sure that `getSyncInformation` works from JavaScript.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetActiveWebContents(), GetProfile(0),
      true);
  LoadLegacyFilesInFrontend(window);

  WebContents* wc = DevToolsWindowTesting::Get(window)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
      (async function() {
        return new Promise(resolve => {
          Host.InspectorFrontendHost.getSyncInformation(resolve);
        });
      })();
    )"));
  ASSERT_TRUE(result.value.is_dict());
  EXPECT_TRUE(*result.value.GetDict().FindBool("isSyncActive"));
  EXPECT_TRUE(*result.value.GetDict().FindBool("arePreferencesSynced"));
  EXPECT_EQ(*result.value.GetDict().FindString("accountEmail"),
            "user@gmail.com");
}

// Regression test for https://crbug.com/1270184.
// TODO(crbug.com/40809266): Fix flakyness. Test is disabled for now.
IN_PROC_BROWSER_TEST_F(DevToolsTest, DISABLED_NoCrashFor1270184) {
  OpenDevToolsWindow("/devtools/regress-crbug-1270184.html", true);
}

class DevToolsProcessPerSiteUpToMainFrameThresholdTest : public DevToolsTest {
 public:
  DevToolsProcessPerSiteUpToMainFrameThresholdTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kProcessPerSiteUpToMainFrameThreshold);
  }

  ~DevToolsProcessPerSiteUpToMainFrameThresholdTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsProcessPerSiteUpToMainFrameThresholdTest,
                       DevToolsWasAttachedBefore) {
  const GURL url = embedded_test_server()->GetURL("foo.test", "/hello.html");

  OpenDevToolsWindow(kDebuggerTestPage, false);

  Browser* browser1 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, url));

  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, url));

  ASSERT_NE(browser1->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetProcess(),
            browser2->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetProcess());
}

// TODO(crbug.com/40924806): The test is failing on multiple builders.
IN_PROC_BROWSER_TEST_F(DevToolsProcessPerSiteUpToMainFrameThresholdTest,
                       DISABLED_DontReuseProcess) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  DevToolsWindow* window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), true);
  WebContents* webcontents =
      DevToolsWindowTesting::Get(window)->main_web_contents();

  DevToolsWindow* window2 =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), false);
  WebContents* webcontents2 =
      DevToolsWindowTesting::Get(window2)->main_web_contents();

  ASSERT_NE(webcontents->GetPrimaryMainFrame()->GetProcess(),
            webcontents2->GetPrimaryMainFrame()->GetProcess());
}

class DevToolsProcessPerSiteTest
    : public DevToolsProcessPerSiteUpToMainFrameThresholdTest {
 public:
  DevToolsProcessPerSiteTest() = default;

  ~DevToolsProcessPerSiteTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ::features::kDevToolsSharedProcessInfobar};
};

// TODO(https://crbug.com/328693031): Flaky on Linux dbg.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_DevToolsSharedProcessInfobar DISABLED_DevToolsSharedProcessInfobar
#else
#define MAYBE_DevToolsSharedProcessInfobar DevToolsSharedProcessInfobar
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProcessPerSiteTest,
                       MAYBE_DevToolsSharedProcessInfobar) {
  const GURL url = embedded_test_server()->GetURL("foo.test", "/hello.html");

  Browser* browser1 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, url));

  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, url));

  ASSERT_EQ(browser1->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetProcess(),
            browser2->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetProcess());

  auto* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser1->tab_strip_model()->GetActiveWebContents(), true);
  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      browser1->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(infobar_manager->infobars().size(), 1u);
  ASSERT_EQ(infobar_manager->infobars()[0]->GetIdentifier(),
            infobars::InfoBarDelegate::DEV_TOOLS_SHARED_PROCESS_DELEGATE);
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
  ASSERT_EQ(infobar_manager->infobars().size(), 0u);

  // Now try in the undocked case.
  window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser1->tab_strip_model()->GetActiveWebContents(), false);

  // The infobar should appear in the undocked window.
  ASSERT_EQ(infobar_manager->infobars().size(), 0u);

  // Retrieve the infobar manager from the devtools window, this is different
  // than `infobar_maanger` when undocked.
  auto* undocked_infobar_manager =
      static_cast<DevToolsUIBindings::Delegate*>(window)->GetInfoBarManager();
  ASSERT_EQ(undocked_infobar_manager->infobars().size(), 1u);
  ASSERT_EQ(undocked_infobar_manager->infobars()[0]->GetIdentifier(),
            infobars::InfoBarDelegate::DEV_TOOLS_SHARED_PROCESS_DELEGATE);
}

// Observe that the active tab has changed.
class ActiveTabChangedObserver : public TabStripModelObserver {
 public:
  explicit ActiveTabChangedObserver(TabStripModel* tab_strip_model) {
    tab_strip_model->AddObserver(this);
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kSelectionOnly &&
        tab_strip_model->active_index() == 0) {
      loop_.Quit();
      return;
    }
  }

  void Wait() { loop_.Run(); }

 private:
  base::RunLoop loop_;
};

// TODO: crbug.com/337141755 - Flaky on Windows ASAN.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_PausedDebuggerFocus DISABLED_PausedDebuggerFocus
#else
#define MAYBE_PausedDebuggerFocus PausedDebuggerFocus
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProcessPerSiteTest, MAYBE_PausedDebuggerFocus) {
  const GURL url = embedded_test_server()->GetURL("foo.test", "/hello.html");

  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* devtools_window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      tab_strip_model->GetWebContentsAt(0), true);
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1, url,
                                     ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_EQ(
      tab_strip_model->GetWebContentsAt(0)->GetPrimaryMainFrame()->GetProcess(),
      tab_strip_model->GetWebContentsAt(1)
          ->GetPrimaryMainFrame()
          ->GetProcess());
  ASSERT_EQ(1, tab_strip_model->active_index());

  ASSERT_TRUE(content::ExecJs(tab_strip_model->GetWebContentsAt(0),
                              "setTimeout(() => {debugger;}, 0);"));
  DispatchOnTestSuite(devtools_window, "waitForDebuggerPaused");
  ActiveTabChangedObserver active_tab_observer(tab_strip_model);
  content::SimulateMouseClick(tab_strip_model->GetActiveWebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  active_tab_observer.Wait();
  ASSERT_EQ(0, tab_strip_model->active_index());
}

class DevToolsConsoleInsightsTest : public DevToolsTest {
 public:
  DevToolsConsoleInsightsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDevToolsConsoleInsights},
        /*disabled_features=*/{});
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetupAccountCapabilities(bool is_minor = false) {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    auto account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@example.com", signin::ConsentLevel::kSync);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_devtools_generative_ai_features(!is_minor);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  ~DevToolsConsoleInsightsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

bool hasQueryParam(WebContents* wc, std::string query_param) {
  return std::string::npos !=
         wc->GetLastCommittedURL().query().find(query_param);
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest, NotBeBlockedByFeatureFlag) {
  SetupAccountCapabilities();
  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
  auto* configConsoleInsights =
      result.value.GetDict().FindDict("devToolsConsoleInsights");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
#endif
  EXPECT_TRUE(configConsoleInsights->FindBool("enabled").value());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest,
                       EnterprisePolicyEnabledByDefault) {
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  SetupAccountCapabilities();
  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_FALSE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  EXPECT_FALSE(configAidaAvailability->FindBool("blockedByAge").value());
  EXPECT_FALSE(configAidaAvailability->FindBool("blockedByGeo").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_TRUE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  EXPECT_TRUE(configAidaAvailability->FindBool("blockedByAge").value());
  EXPECT_TRUE(configAidaAvailability->FindBool("blockedByGeo").value());
#endif

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest, IsBlockedByGeo) {
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("cn");
  SetupAccountCapabilities();
  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_FALSE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  EXPECT_FALSE(configAidaAvailability->FindBool("blockedByAge").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_TRUE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  EXPECT_TRUE(configAidaAvailability->FindBool("blockedByAge").value());
#endif
  EXPECT_TRUE(configAidaAvailability->FindBool("blockedByGeo").value());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest, IsNotEnabledForMinors) {
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  SetupAccountCapabilities(true);
  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_FALSE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  EXPECT_FALSE(configAidaAvailability->FindBool("blockedByGeo").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_TRUE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  EXPECT_TRUE(configAidaAvailability->FindBool("blockedByGeo").value());
#endif
  EXPECT_TRUE(configAidaAvailability->FindBool("blockedByAge").value());

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest,
                       CanBeDisabledByEnterprisePolicy) {
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  SetupAccountCapabilities();
  // Disable via enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kDevToolsGenAiSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(/* disable */ 2),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
#endif
  EXPECT_TRUE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest,
                       CanBeEnabledByEnterprisePolicy) {
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  SetupAccountCapabilities();
  // Enable via enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kDevToolsGenAiSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(/* allow */ 0),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_FALSE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_TRUE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
#endif

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsConsoleInsightsTest,
                       IsDisabledWhenPolicySetToOne) {
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  policy::PolicyMap policies;
  policies.Set(
      policy::key::kDevToolsGenAiSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(/* enable and don't use data for training */ 1), nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  OpenDevToolsWindow(kDebuggerTestPage, false);
  LoadLegacyFilesInFrontend(window_);
  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  const auto result = content::EvalJs(wc, content::JsReplace(R"(
    (async function() {
      return new Promise(resolve => {
        Host.InspectorFrontendHost.getHostConfig(resolve);
      });
    })();
  )"));
  ASSERT_TRUE(result.value.is_dict());
  auto* configAidaAvailability =
      result.value.GetDict().FindDict("aidaAvailability");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_TRUE(configAidaAvailability->FindBool("disallowLogging").value());
  EXPECT_FALSE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
#else
  EXPECT_FALSE(configAidaAvailability->FindBool("enabled").value());
  EXPECT_TRUE(
      configAidaAvailability->FindBool("blockedByEnterprisePolicy").value());
#endif

  CloseDevToolsWindow();
}

class DevToolsSelfXssTest : public DevToolsTest {
 public:
  DevToolsSelfXssTest() = default;

  ~DevToolsSelfXssTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kUnsafelyDisableDevToolsSelfXssWarnings);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsSelfXssTest, FooFoo) {
  OpenDevToolsWindow(kDebuggerTestPage, false);

  WebContents* wc = DevToolsWindowTesting::Get(window_)->main_web_contents();
  EXPECT_TRUE(hasQueryParam(wc, "&disableSelfXssWarnings=true"));

  CloseDevToolsWindow();
}

class DevToolsRenderDocumentTest : public DevToolsTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::InitAndEnableRenderDocumentForAllFrames(
        &feature_list_for_render_document_);
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
};

// This test verifies that the devtools window is not accidentally destroyed
// on reload with RenderDocument enabled (https://crbug.com/337794575).
IN_PROC_BROWSER_TEST_F(DevToolsRenderDocumentTest, ReloadWithRFHSwap) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  bool called = false;
  DevToolsWindowTesting::Get(window_)->SetCloseCallback(
      base::BindOnce([](bool& called) { called = true; }, std::ref(called)));
  WebContents* main_web_contents =
      DevToolsWindowTesting::Get(window_)->main_web_contents();
  main_web_contents->ReloadFocusedFrame();
  EXPECT_TRUE(WaitForLoadStop(main_web_contents));
  EXPECT_FALSE(called);
  DevToolsWindowTesting::Get(window_)->CloseDevToolsWindowSync(window_);
}
