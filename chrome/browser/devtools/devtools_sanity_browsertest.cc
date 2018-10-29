// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using app_modal::JavaScriptAppModalDialog;
using app_modal::NativeAppModalDialog;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::DevToolsAgentHostObserver;
using content::NavigationController;
using content::RenderFrameHost;
using content::WebContents;
using extensions::Extension;

namespace {

const char kDebuggerTestPage[] = "files/devtools/debugger_test_page.html";
const char kPauseWhenLoadingDevTools[] =
    "files/devtools/pause_when_loading_devtools.html";
const char kPauseWhenScriptIsRunning[] =
    "files/devtools/pause_when_script_is_running.html";
const char kPageWithContentScript[] =
    "files/devtools/page_with_content_script.html";
const char kNavigateBackTestPage[] =
    "files/devtools/navigate_back.html";
const char kWindowOpenTestPage[] = "files/devtools/window_open.html";
const char kLatencyInfoTestPage[] = "files/devtools/latency_info.html";
const char kChunkedTestPage[] = "chunked";
const char kPushTestPage[] = "files/devtools/push_test_page.html";
// The resource is not really pushed, but mock url request job pretends it is.
const char kPushTestResource[] = "devtools/image.png";
const char kPushUseNullEndTime[] = "pushUseNullEndTime";
const char kSlowTestPage[] =
    "chunked?waitBeforeHeaders=100&waitBetweenChunks=100&chunksNumber=2";
const char kSharedWorkerTestPage[] =
    "files/workers/workers_ui_shared_worker.html";
const char kSharedWorkerTestWorker[] =
    "files/workers/workers_ui_shared_worker.js";
const char kReloadSharedWorkerTestPage[] =
    "files/workers/debug_shared_worker_initialization.html";
const char kReloadSharedWorkerTestWorker[] =
    "files/workers/debug_shared_worker_initialization.js";
const char kEmulateNetworkConditionsPage[] =
    "files/devtools/emulate_network_conditions.html";
const char kDispatchKeyEventShowsAutoFill[] =
    "files/devtools/dispatch_key_event_shows_auto_fill.html";
const char kDOMWarningsTestPage[] = "files/devtools/dom_warnings_page.html";

template <typename... T>
void DispatchOnTestSuiteSkipCheck(DevToolsWindow* window,
                                  const char* method,
                                  T... args) {
  WebContents* wc = DevToolsWindowTesting::Get(window)->main_web_contents();
  std::string result;
  const char* args_array[] = {method, args...};
  std::ostringstream script;
  script << "uiTests.dispatchOnTestSuite([";
  for (size_t i = 0; i < arraysize(args_array); ++i)
    script << (i ? "," : "") << '\"' << args_array[i] << '\"';
  script << "])";
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(wc, script.str(), &result));
  EXPECT_EQ("[OK]", result);
}

template <typename... T>
void DispatchOnTestSuite(DevToolsWindow* window,
                         const char* method,
                         T... args) {
  std::string result;
  WebContents* wc = DevToolsWindowTesting::Get(window)->main_web_contents();
  // At first check that JavaScript part of the front-end is loaded by
  // checking that global variable uiTests exists(it's created after all js
  // files have been loaded) and has runTest method.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      wc,
      "window.domAutomationController.send("
      "    '' + (window.uiTests && (typeof uiTests.dispatchOnTestSuite)));",
      &result));
  ASSERT_EQ("function", result) << "DevTools front-end is broken.";
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
  std::string prefix = base::TrimString(devtools_extension->url().spec(), "/",
                                        base::TRIM_TRAILING)
                           .as_string();
  SwitchToPanel(window, (prefix + panel_name).c_str());
}

}  // namespace

class DevToolsSanityTest : public InProcessBrowserTest {
 public:
  DevToolsSanityTest() : window_(NULL) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
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
    GURL url = spawned_test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDevToolsWindow(const std::string& test_page, bool is_docked) {
    ASSERT_TRUE(spawned_test_server()->Start());
    LoadTestPage(test_page);

    window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(),
                                                            is_docked);
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

  DevToolsWindow* window_;
};

class SitePerProcessDevToolsSanityTest : public DevToolsSanityTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsSanityTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  };

  void SetUpOnMainThread() override {
    DevToolsSanityTest::SetUpOnMainThread();
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Used to block until a dev tools window gets beforeunload event.
class DevToolsWindowBeforeUnloadObserver
    : public content::WebContentsObserver {
 public:
  explicit DevToolsWindowBeforeUnloadObserver(DevToolsWindow*);
  void Wait();
 private:
  // Invoked when the beforeunload handler fires.
  void BeforeUnloadFired(bool proceed,
                         const base::TimeTicks& proceed_time) override;

  bool m_fired;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowBeforeUnloadObserver);
};

DevToolsWindowBeforeUnloadObserver::DevToolsWindowBeforeUnloadObserver(
    DevToolsWindow* devtools_window)
    : WebContentsObserver(
          DevToolsWindowTesting::Get(devtools_window)->main_web_contents()),
      m_fired(false) {
}

void DevToolsWindowBeforeUnloadObserver::Wait() {
  if (m_fired)
    return;
  message_loop_runner_ = base::MakeRefCounted<content::MessageLoopRunner>();
  message_loop_runner_->Run();
}

void DevToolsWindowBeforeUnloadObserver::BeforeUnloadFired(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  m_fired = true;
  if (message_loop_runner_.get())
    message_loop_runner_->Quit();
}

class DevToolsBeforeUnloadTest: public DevToolsSanityTest {
 public:
  void CloseInspectedTab() {
    browser()->tab_strip_model()->CloseWebContentsAt(0,
        TabStripModel::CLOSE_NONE);
  }

  void CloseDevToolsWindowAsync() {
    DevToolsWindowTesting::CloseDevToolsWindow(window_);
  }

  void CloseInspectedBrowser() {
    chrome::CloseWindow(browser());
  }

 protected:
  void InjectBeforeUnloadListener(content::WebContents* web_contents) {
    ASSERT_TRUE(content::ExecuteScript(
        web_contents,
        "window.addEventListener('beforeunload',"
        "function(event) { event.returnValue = 'Foo'; });"));
    content::PrepContentsForBeforeUnloadTest(web_contents);
  }

  void RunBeforeUnloadSanityTest(bool is_docked,
                                 base::Callback<void(void)> close_method,
                                 bool wait_for_browser_close = true) {
    OpenDevToolsWindow(kDebuggerTestPage, is_docked);
    auto runner = base::MakeRefCounted<content::MessageLoopRunner>();
    DevToolsWindowTesting::Get(window_)->
        SetCloseCallback(runner->QuitClosure());
    InjectBeforeUnloadListener(main_web_contents());
    {
      DevToolsWindowBeforeUnloadObserver before_unload_observer(window_);
      close_method.Run();
      CancelModalDialog();
      before_unload_observer.Wait();
    }
    {
      content::WindowedNotificationObserver close_observer(
          chrome::NOTIFICATION_BROWSER_CLOSED,
          content::Source<Browser>(browser()));
      close_method.Run();
      AcceptModalDialog();
      if (wait_for_browser_close)
        close_observer.Wait();
    }
    runner->Run();
  }

  DevToolsWindow* OpenDevToolWindowOnWebContents(
      content::WebContents* contents, bool is_docked) {
    DevToolsWindow* window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(contents, is_docked);
    return window;
  }

  void OpenDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    ASSERT_TRUE(content::ExecuteScript(
        DevToolsWindowTesting::Get(devtools_window)->main_web_contents(),
        "window.open(\"\", \"\", \"location=0\");"));
    observer.Wait();
  }

  void CloseDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  }

  void AcceptModalDialog() {
    NativeAppModalDialog* native_dialog = GetDialog();
    native_dialog->AcceptAppModalDialog();
  }

  void CancelModalDialog() {
    NativeAppModalDialog* native_dialog = GetDialog();
    native_dialog->CancelAppModalDialog();
  }

  NativeAppModalDialog* GetDialog() {
    JavaScriptAppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
    NativeAppModalDialog* native_dialog = dialog->native_dialog();
    EXPECT_TRUE(native_dialog);
    return native_dialog;
  }
};

void TimeoutCallback(const std::string& timeout_message) {
  ADD_FAILURE() << timeout_message;
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

// Base class for DevTools tests that test devtools functionality for
// extensions and content scripts.
class DevToolsExtensionTest : public DevToolsSanityTest,
                              public content::NotificationObserver {
 public:
  DevToolsExtensionTest() : DevToolsSanityTest() {
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_extensions_dir_);
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("devtools");
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("extensions");
  }

 protected:
  // Load an extension from test\data\devtools\extensions\<extension_name>
  void LoadExtension(const char* extension_name) {
    base::FilePath path = test_extensions_dir_.AppendASCII(extension_name);
    ASSERT_TRUE(LoadExtensionFromPath(path)) << "Failed to load extension.";
  }

  const Extension* LoadExtensionFromPath(const base::FilePath& path) {
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());
    extensions::TestExtensionRegistryObserver observer(registry);
    extensions::UnpackedInstaller::Create(service)->Load(path);
    observer.WaitForExtensionLoaded();

    if (!WaitForExtensionViewsToLoad())
      return nullptr;

    return GetExtensionByPath(registry->enabled_extensions(), path);
  }

  // Loads a dynamically generated extension populated with a bunch of test
  // pages. |name| is the extension name to use in the manifest.
  // |devtools_page|, if non-empty, indicates which test page should be be
  // listed as a devtools_page in the manifest.  If |devtools_page| is empty, a
  // non-devtools extension is created instead. |panel_iframe_src| controls the
  // src= attribute of the <iframe> element in the 'panel.html' test page.
  const Extension* LoadExtensionForTest(const std::string& name,
                                        const std::string& devtools_page,
                                        const std::string& panel_iframe_src) {
    test_extension_dirs_.push_back(
        std::make_unique<extensions::TestExtensionDir>());
    extensions::TestExtensionDir* dir = test_extension_dirs_.back().get();

    extensions::DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1")
        .Set("manifest_version", 2)
        // simple_test_page.html is currently the only page referenced outside
        // of its own extension in the tests
        .Set("web_accessible_resources",
             extensions::ListBuilder().Append("simple_test_page.html").Build());

    // If |devtools_page| isn't empty, make it a devtools extension in the
    // manifest.
    if (!devtools_page.empty())
      manifest.Set("devtools_page", devtools_page);

    dir->WriteManifest(manifest.ToJSON());

    GURL http_frame_url =
        embedded_test_server()->GetURL("a.com", "/popup_iframe.html");

    // If this is a devtools extension, |devtools_page| will indicate which of
    // these devtools_pages will end up being used.  Different tests use
    // different devtools_pages.
    dir->WriteFile(FILE_PATH_LITERAL("web_devtools_page.html"),
                   "<html><body><iframe src='" + http_frame_url.spec() +
                       "'></iframe></body></html>");

    dir->WriteFile(FILE_PATH_LITERAL("simple_devtools_page.html"),
                   "<html><body></body></html>");

    dir->WriteFile(
        FILE_PATH_LITERAL("panel_devtools_page.html"),
        "<html><head><script "
        "src='panel_devtools_page.js'></script></head><body></body></html>");

    dir->WriteFile(FILE_PATH_LITERAL("panel_devtools_page.js"),
                   "chrome.devtools.panels.create('iframe_panel',\n"
                   "    null,\n"
                   "    'panel.html',\n"
                   "    function(panel) {\n"
                   "      chrome.devtools.inspectedWindow.eval(\n"
                   "        'console.log(\"PASS\")');\n"
                   "    }\n"
                   ");\n");

    dir->WriteFile(FILE_PATH_LITERAL("sidebarpane_devtools_page.html"),
                   "<html><head><script src='sidebarpane_devtools_page.js'>"
                   "</script></head><body></body></html>");

    dir->WriteFile(
        FILE_PATH_LITERAL("sidebarpane_devtools_page.js"),
        "chrome.devtools.panels.elements.createSidebarPane('iframe_pane',\n"
        "    function(sidebar) {\n"
        "      chrome.devtools.inspectedWindow.eval(\n"
        "        'console.log(\"PASS\")');\n"
        "      sidebar.setPage('panel.html');\n"
        "    }\n"
        ");\n");

    dir->WriteFile(FILE_PATH_LITERAL("panel.html"),
                   "<html><body><iframe src='" + panel_iframe_src +
                       "'></iframe></body></html>");

    dir->WriteFile(FILE_PATH_LITERAL("simple_test_page.html"),
                   "<html><body>This is a test</body></html>");

    GURL web_url = embedded_test_server()->GetURL("a.com", "/title3.html");

    dir->WriteFile(FILE_PATH_LITERAL("multi_frame_page.html"),
                   "<html><body><iframe src='about:blank'>"
                   "</iframe><iframe src='data:text/html,foo'>"
                   "</iframe><iframe src='" +
                       web_url.spec() + "'></iframe></body></html>");

    // Install the extension.
    return LoadExtensionFromPath(dir->UnpackedPath());
  }

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

  bool WaitForExtensionViewsToLoad() {
    // Wait for all the extension render views that exist to finish loading.
    // NOTE: This assumes that the extension views list is not changing while
    // this method is running.

    content::NotificationRegistrar registrar;
    registrar.Add(this,
                  extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                  content::NotificationService::AllSources());
    base::CancelableClosure timeout(
        base::Bind(&TimeoutCallback, "Extension host load timed out."));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());

    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(browser()->profile());
    extensions::ProcessManager::FrameSet all_frames = manager->GetAllFrames();
    for (auto iter = all_frames.begin(); iter != all_frames.end();) {
      if (!content::WebContents::FromRenderFrameHost(*iter)->IsLoading())
        ++iter;
      else
        content::RunMessageLoop();
    }

    timeout.Cancel();
    return true;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
              type);
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  std::vector<std::unique_ptr<extensions::TestExtensionDir>>
      test_extension_dirs_;
  base::FilePath test_extensions_dir_;
};

class DevToolsExperimentalExtensionTest : public DevToolsExtensionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }
};

class WorkerDevToolsSanityTest : public InProcessBrowserTest {
 public:
  WorkerDevToolsSanityTest() : window_(NULL) {}

 protected:
  class WorkerCreationObserver : public DevToolsAgentHostObserver {
   public:
    WorkerCreationObserver(const std::string& path,
                           scoped_refptr<DevToolsAgentHost>* out_host,
                           base::Closure quit)
        : path_(path), out_host_(out_host), quit_(quit) {
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
        base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, quit_);
        delete this;
      }
    }

    std::string path_;
    scoped_refptr<DevToolsAgentHost>* out_host_;
    base::Closure quit_;
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
    content::RunThisRunLoop(&run_loop);
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

  DevToolsWindow* window_;
};

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestDockedDevToolsClose) {
  RunBeforeUnloadSanityTest(true, base::Bind(
      &DevToolsBeforeUnloadTest::CloseDevToolsWindowAsync,
      base::Unretained(this)), false);
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedTabClose) {
  RunBeforeUnloadSanityTest(true, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedTab,
      base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadSanityTest(true, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedBrowser,
      base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestUndockedDevToolsClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &DevToolsBeforeUnloadTest::CloseDevToolsWindowAsync,
      base::Unretained(this)), false);
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedTabClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedTab,
      base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedBrowser,
      base::Unretained(this)));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to exit application.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsApplicationClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &chrome::CloseAllBrowsers));
}

// Tests that inspected tab gets closed if devtools renderer
// becomes unresponsive during beforeunload event interception.
// @see http://crbug.com/322380
// Disabled because of http://crbug.com/410327
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       DISABLED_TestUndockedDevToolsUnresponsive) {
  ASSERT_TRUE(spawned_test_server()->Start());
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      GetInspectedTab(), false);

  auto runner = base::MakeRefCounted<content::MessageLoopRunner>();
  DevToolsWindowTesting::Get(devtools_window)->SetCloseCallback(
      runner->QuitClosure());

  ASSERT_TRUE(content::ExecuteScript(
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents(),
      "window.addEventListener('beforeunload',"
      "function(event) { while (true); });"));
  CloseInspectedTab();
  runner->Run();
}

// Tests that closing worker inspector window does not cause browser crash
// @see http://crbug.com/323031
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestWorkerWindowClosing) {
  ASSERT_TRUE(spawned_test_server()->Start());
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      GetInspectedTab(), false);

  OpenDevToolsPopupWindow(devtools_window);
  CloseDevToolsPopupWindow(devtools_window);
}

// Tests that BeforeUnload event gets called on devtools that are opened
// on another devtools.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDevToolsOnDevTools) {
  ASSERT_TRUE(spawned_test_server()->Start());
  LoadTestPage(kDebuggerTestPage);

  std::vector<DevToolsWindow*> windows;
  std::vector<std::unique_ptr<content::WindowedNotificationObserver>>
      close_observers;
  content::WebContents* inspected_web_contents = GetInspectedTab();
  for (int i = 0; i < 3; ++i) {
    DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      inspected_web_contents, i == 0);
    windows.push_back(devtools_window);
    close_observers.push_back(
        std::make_unique<content::WindowedNotificationObserver>(
            content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
            content::Source<content::WebContents>(
                DevToolsWindowTesting::Get(devtools_window)
                    ->main_web_contents())));
    inspected_web_contents =
        DevToolsWindowTesting::Get(devtools_window)->main_web_contents();
  }

  InjectBeforeUnloadListener(
      DevToolsWindowTesting::Get(windows[0])->main_web_contents());
  InjectBeforeUnloadListener(
      DevToolsWindowTesting::Get(windows[2])->main_web_contents());
  // Try to close second devtools.
  {
    content::WindowedNotificationObserver cancel_browser(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllSources());
    chrome::CloseWindow(DevToolsWindowTesting::Get(windows[1])->browser());
    CancelModalDialog();
    cancel_browser.Wait();
  }
  // Try to close browser window.
  {
    content::WindowedNotificationObserver cancel_browser(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllSources());
    chrome::CloseWindow(browser());
    AcceptModalDialog();
    CancelModalDialog();
    cancel_browser.Wait();
  }
  // Try to exit application.
  {
    content::WindowedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser()));
    chrome::CloseAllBrowsers();
    AcceptModalDialog();
    AcceptModalDialog();
    close_observer.Wait();
  }
  for (auto& close_observer : close_observers)
    close_observer->Wait();
}

// Tests scripts panel showing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestShowScriptsTab) {
  RunTest("testShowScriptsTab", kDebuggerTestPage);
}

// Tests that scripts tab is populated with inspected scripts even if it
// hadn't been shown by the moment inspected paged refreshed.
// @see http://crbug.com/26312
IN_PROC_BROWSER_TEST_F(
    DevToolsSanityTest,
    TestScriptsTabIsPopulatedOnInspectedPageRefresh) {
  RunTest("testScriptsTabIsPopulatedOnInspectedPageRefresh",
          kDebuggerTestPage);
}

// Tests that chrome.devtools extension is correctly exposed.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       TestDevToolsExtensionAPI) {
  LoadExtension("devtools_extension");
  RunTest("waitForTestResultsInConsole", std::string());
}

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
  ASSERT_TRUE(embedded_test_server()->Start());

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
  SwitchToExtensionPanel(window_, extension, "iframe_panel");
  content::WaitForLoadStop(main_web_contents());

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
  EXPECT_EQ(7U, rfhs.size());

  // This test creates a page with the following frame tree:
  // - DevTools
  //   - devtools_page from DevTools extension
  //   - Panel (DevTools extension)
  //     - iframe (DevTools extension)
  //       - about:blank
  //       - data:
  //       - web URL

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();
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

  ASSERT_TRUE(content::ExecuteScript(web_frame_rfh, about_blank_javascript));

  web_about_blank_manager.WaitForNavigationFinished();

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

  ASSERT_TRUE(content::ExecuteScript(web_frame_rfh, renavigation_javascript));

  renavigation_manager.WaitForNavigationFinished();

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
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       HttpIframeInDevToolsExtensionSideBarPane) {
  ASSERT_TRUE(embedded_test_server()->Start());

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
  SwitchToPanel(window_, "iframe_pane");
  web_manager.WaitForNavigationFinished();

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
  EXPECT_EQ(4U, rfhs.size());

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();
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
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install the dynamically-generated extension.
  const Extension* extension =
      LoadExtensionForTest("Devtools Extension", "web_devtools_page.html",
                           "" /* panel_iframe_src */);
  ASSERT_TRUE(extension);

  // Wait for a 'DONE' message sent from popup_iframe.html, indicating that it
  // loaded successfully.
  content::DOMMessageQueue message_queue;
  std::string message;
  OpenDevToolsWindow(kDebuggerTestPage, false);

  while (true) {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    if (message == "\"DONE\"")
      break;
  }

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
  EXPECT_EQ(3U, rfhs.size());

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();
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
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       NonDevToolsExtensionInDevToolsExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

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
  SwitchToExtensionPanel(window_, devtools_extension, "iframe_panel");
  non_devtools_manager.WaitForNavigationFinished();

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
  EXPECT_EQ(4U, rfhs.size());

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();
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
  EXPECT_EQ(non_dt_extension_test_url.GetOrigin(),
            non_devtools_extension_rfh->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(devtools_instance, non_devtools_extension_rfh->GetSiteInstance());
  EXPECT_NE(extensions_instance, non_devtools_extension_rfh->GetSiteInstance());
}

// Tests that if a devtools extension's devtools panel page has a subframe to a
// page for another devtools extension, the subframe is rendered in the devtools
// process as well.  http://crbug.com/570483
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DevToolsExtensionInDevToolsExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

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
  SwitchToExtensionPanel(window_, devtools_a_extension, "iframe_panel");
  extension_b_manager.WaitForNavigationFinished();

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
  EXPECT_EQ(5U, rfhs.size());

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();

  RenderFrameHost* devtools_extension_a_devtools_rfh =
      content::FrameMatchingPredicate(
          main_web_contents(), base::Bind(&content::FrameHasSourceUrl,
                                          devtools_a_extension->GetResourceURL(
                                              "/panel_devtools_page.html")));
  EXPECT_TRUE(devtools_extension_a_devtools_rfh);
  RenderFrameHost* devtools_extension_b_devtools_rfh =
      content::FrameMatchingPredicate(
          main_web_contents(), base::Bind(&content::FrameHasSourceUrl,
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
  ASSERT_TRUE(embedded_test_server()->Start());

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
  SwitchToExtensionPanel(window_, extension, "iframe_panel");
  test_page_manager.WaitForNavigationFinished();

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
  EXPECT_EQ(4U, rfhs.size());

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();
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
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, DevtoolsInDevTools) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL devtools_url = GURL(chrome::kChromeUIDevToolsURL);

  OpenDevToolsWindow(kDebuggerTestPage, false);

  std::string javascript =
      "var devtoolsFrame = document.createElement('iframe');"
      "document.body.appendChild(devtoolsFrame);"
      "devtoolsFrame.src = '" +
      devtools_url.spec() + "';";

  RenderFrameHost* main_devtools_rfh = main_web_contents()->GetMainFrame();

  content::TestNavigationManager manager(main_web_contents(), devtools_url);
  ASSERT_TRUE(content::ExecuteScript(main_devtools_rfh, javascript));
  manager.WaitForNavigationFinished();

  std::vector<RenderFrameHost*> rfhs = main_web_contents()->GetAllFrames();
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

  std::string message;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      devtools_iframe_rfh, "domAutomationController.send(self.origin)",
      &message));
  EXPECT_EQ(devtools_url.GetOrigin().spec(), message + "/");
}

// Some web features, when used from an extension, are subject to browser-side
// security policy enforcement. Make sure they work properly from inside a
// devtools extension.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DevToolsExtensionSecurityPolicyGrants) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto dir = std::make_unique<extensions::TestExtensionDir>();

  extensions::DictionaryBuilder manifest;
  dir->WriteManifest(extensions::DictionaryBuilder()
                         .Set("name", "Devtools Panel")
                         .Set("version", "1")
                         // Whitelist the script we stuff into the 'blob:' URL:
                         .Set("content_security_policy",
                              "script-src 'self' "
                              "'sha256-uv9gxBEOFchPzak3TK6O39RdKxJeZvfha9zOHGam"
                              "TB4='; "
                              "object-src 'none'")
                         .Set("manifest_version", 2)
                         .Set("devtools_page", "devtools.html")
                         .ToJSON());

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
  content::DOMMessageQueue message_queue;
  SwitchToExtensionPanel(window_, extension, "the_panel_name");
  std::string message;
  while (true) {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    if (message == "\"xhr blob contents\"")
      break;
  }
  while (true) {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    if (message == "\"iframe blob contents\"")
      break;
  }
}

// Disabled on Windows due to flakiness. http://crbug.com/183649
#if defined(OS_WIN)
#define MAYBE_TestDevToolsExtensionMessaging DISABLED_TestDevToolsExtensionMessaging
#else
#define MAYBE_TestDevToolsExtensionMessaging TestDevToolsExtensionMessaging
#endif

// Tests that chrome.devtools extension can communicate with background page
// using extension messaging.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_TestDevToolsExtensionMessaging) {
  LoadExtension("devtools_messaging");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests that chrome.experimental.devtools extension is correctly exposed
// when the extension has experimental permission.
IN_PROC_BROWSER_TEST_F(DevToolsExperimentalExtensionTest,
                       TestDevToolsExperimentalExtensionAPI) {
  LoadExtension("devtools_experimental");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests that a content script is in the scripts list.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       TestContentScriptIsPresent) {
  LoadExtension("simple_content_script");
  RunTest("testContentScriptIsPresent", kPageWithContentScript);
}

// Tests that console selector shows correct context names.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       TestConsoleContextNames) {
  LoadExtension("simple_content_script");
  RunTest("testConsoleContextNames", kPageWithContentScript);
}

// Tests that scripts are not duplicated after Scripts Panel switch.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestNoScriptDuplicatesOnPanelSwitch) {
  RunTest("testNoScriptDuplicatesOnPanelSwitch", kDebuggerTestPage);
}

// Tests that debugger works correctly if pause event occurs when DevTools
// frontend is being loaded.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestPauseWhenLoadingDevTools) {
  RunTest("testPauseWhenLoadingDevTools", kPauseWhenLoadingDevTools);
}

// Tests that pressing 'Pause' will pause script execution if the script
// is already running.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on linux ARM bot: https://crbug/238453
#define MAYBE_TestPauseWhenScriptIsRunning DISABLED_TestPauseWhenScriptIsRunning
#else
#define MAYBE_TestPauseWhenScriptIsRunning TestPauseWhenScriptIsRunning
#endif
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       MAYBE_TestPauseWhenScriptIsRunning) {
  RunTest("testPauseWhenScriptIsRunning", kPauseWhenScriptIsRunning);
}

// Tests network timing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkTiming) {
  RunTest("testNetworkTiming", kSlowTestPage);
}

// Tests network size.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSize) {
  RunTest("testNetworkSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSyncSize) {
  RunTest("testNetworkSyncSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkRawHeadersText) {
  RunTest("testNetworkRawHeadersText", kChunkedTestPage);
}

namespace {

bool InterceptURLLoad(content::URLLoaderInterceptor::RequestParams* params) {
  const GURL& url = params->url_request.url;
  if (!base::EndsWith(url.path(), kPushTestResource,
                      base::CompareCase::SENSITIVE)) {
    return false;
  }

  network::ResourceResponseHead response;

  response.headers = new net::HttpResponseHeaders("200 OK\r\n\r\n");

  auto start_time =
      base::TimeTicks::Now() - base::TimeDelta::FromMilliseconds(10);
  response.request_start = start_time;
  response.response_start = base::TimeTicks::Now();
  response.request_time =
      base::Time::Now() - base::TimeDelta::FromMilliseconds(10);
  response.response_time = base::Time::Now();

  auto& load_timing = response.load_timing;
  load_timing.request_start = start_time;
  load_timing.request_start_time = response.request_time;
  load_timing.send_start = start_time;
  load_timing.send_end = base::TimeTicks::Now();
  load_timing.receive_headers_end = base::TimeTicks::Now();
  load_timing.push_start = start_time - base::TimeDelta::FromMilliseconds(100);
  if (url.query() != kPushUseNullEndTime)
    load_timing.push_end = base::TimeTicks::Now();

  params->client->OnReceiveResponse(response);
  params->client->OnComplete(network::URLLoaderCompletionStatus());
  return true;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkPushTime) {
  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(InterceptURLLoad));

  OpenDevToolsWindow(kPushTestPage, false);
  GURL push_url = spawned_test_server()->GetURL(kPushTestResource);

  DispatchOnTestSuite(window_, "testPushTimes", push_url.spec().c_str());

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDOMWarnings) {
  RunTest("testDOMWarnings", kDOMWarningsTestPage);
}

// Tests that console messages are not duplicated on navigation back.
#if defined(OS_WIN)
// Flaking on windows swarm try runs: crbug.com/409285.
#define MAYBE_TestConsoleOnNavigateBack DISABLED_TestConsoleOnNavigateBack
#else
#define MAYBE_TestConsoleOnNavigateBack TestConsoleOnNavigateBack
#endif
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestConsoleOnNavigateBack) {
  RunTest("testConsoleOnNavigateBack", kNavigateBackTestPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDeviceEmulation) {
  RunTest("testDeviceMetricsOverrides", "about:blank");
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDispatchKeyEventDoesNotCrash) {
  RunTest("testDispatchKeyEventDoesNotCrash", "about:blank");
}

class AutofillManagerTestDelegateDevtoolsImpl
    : public autofill::AutofillManagerTestDelegate {
 public:
  explicit AutofillManagerTestDelegateDevtoolsImpl(
      WebContents* inspectedContents)
      : inspected_contents_(inspectedContents) {}
  ~AutofillManagerTestDelegateDevtoolsImpl() override {}

  void DidPreviewFormData() override {}

  void DidFillFormData() override {}

  void DidShowSuggestions() override {
    ASSERT_TRUE(content::ExecuteScript(inspected_contents_,
                                       "console.log('didShowSuggestions');"));
  }

  void OnTextFieldChanged() override {}

 private:
  WebContents* inspected_contents_;

  DISALLOW_COPY_AND_ASSIGN(AutofillManagerTestDelegateDevtoolsImpl);
};

// Disabled. Failing on MacOS MSAN. See https://crbug.com/849129.
#if defined(OS_MACOSX)
#define MAYBE_TestDispatchKeyEventShowsAutoFill \
  DISABLED_TestDispatchKeyEventShowsAutoFill
#else
#define MAYBE_TestDispatchKeyEventShowsAutoFill \
  TestDispatchKeyEventShowsAutoFill
#endif
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       MAYBE_TestDispatchKeyEventShowsAutoFill) {
  OpenDevToolsWindow(kDispatchKeyEventShowsAutoFill, false);

  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriverFactory::FromWebContents(GetInspectedTab())
          ->DriverForFrame(GetInspectedTab()->GetMainFrame());
  autofill::AutofillManager* autofill_manager =
      autofill_driver->autofill_manager();
  AutofillManagerTestDelegateDevtoolsImpl autoFillTestDelegate(
      GetInspectedTab());
  autofill_manager->SetTestDelegate(&autoFillTestDelegate);

  RunTestFunction(window_, "testDispatchKeyEventShowsAutoFill");
  CloseDevToolsWindow();
}

// Tests that settings are stored in profile correctly.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestSettings) {
  OpenDevToolsWindow("about:blank", true);
  RunTestFunction(window_, "testSettings");
  CloseDevToolsWindow();
}

// Tests that external navigation from inspector page is always handled by
// DevToolsWindow and results in inspected page navigation.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDevToolsExternalNavigation) {
  OpenDevToolsWindow(kDebuggerTestPage, true);
  GURL url = spawned_test_server()->GetURL(kNavigateBackTestPage);
  ui_test_utils::UrlLoadObserver observer(url,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(
      main_web_contents(),
      std::string("window.location = \"") + url.spec() + "\""));
  observer.Wait();

  ASSERT_TRUE(main_web_contents()->GetURL().
                  SchemeIs(content::kChromeDevToolsScheme));
  ASSERT_EQ(url, GetInspectedTab()->GetURL());
  CloseDevToolsWindow();
}

// Tests that toolbox window is loaded when DevTools window is undocked.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestToolboxLoadedUndocked) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  ASSERT_TRUE(toolbox_web_contents());
  DevToolsWindow* on_self =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), false);
  ASSERT_FALSE(DevToolsWindowTesting::Get(on_self)->toolbox_web_contents());
  DevToolsWindowTesting::CloseDevToolsWindowSync(on_self);
  CloseDevToolsWindow();
}

// Tests that toolbox window is not loaded when DevTools window is docked.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestToolboxNotLoadedDocked) {
  OpenDevToolsWindow(kDebuggerTestPage, true);
  ASSERT_FALSE(toolbox_web_contents());
  DevToolsWindow* on_self =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), false);
  ASSERT_FALSE(DevToolsWindowTesting::Get(on_self)->toolbox_web_contents());
  DevToolsWindowTesting::CloseDevToolsWindowSync(on_self);
  CloseDevToolsWindow();
}

// Tests that inspector will reattach to inspected page when it is reloaded
// after a crash. See http://crbug.com/101952
// Disabled. it doesn't check anything right now: http://crbug.com/461790
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, DISABLED_TestReattachAfterCrash) {
  RunTest("testReattachAfterCrash", std::string());
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPageWithNoJavaScript) {
  OpenDevToolsWindow("about:blank", false);
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      main_web_contents(),
      "window.domAutomationController.send("
      "    '' + (window.uiTests && (typeof uiTests.dispatchOnTestSuite)));",
      &result));
  ASSERT_EQ("function", result) << "DevTools front-end is broken.";
  CloseDevToolsWindow();
}

class DevToolsAutoOpenerTest : public DevToolsSanityTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAutoOpenDevToolsForTabs);
    observer_ = std::make_unique<DevToolsWindowCreationObserver>();
  }
 protected:
  std::unique_ptr<DevToolsWindowCreationObserver> observer_;
};

IN_PROC_BROWSER_TEST_F(DevToolsAutoOpenerTest, TestAutoOpenForTabs) {
  {
    DevToolsWindowCreationObserver observer;
    AddTabAtIndexToBrowser(browser(), 0, GURL("about:blank"),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
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
    AddTabAtIndexToBrowser(new_browser, 0, GURL("about:blank"),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    observer.WaitForLoad();
  }
  observer_->CloseAllSync();
}

class DevToolsReattachAfterCrashTest : public DevToolsSanityTest {
 protected:
  void RunTestWithPanel(const char* panel_name) {
    OpenDevToolsWindow("about:blank", false);
    SwitchToPanel(window_, panel_name);
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

    content::RenderProcessHostWatcher crash_observer(
        GetInspectedTab(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));
    crash_observer.Wait();
    content::TestNavigationObserver navigation_observer(GetInspectedTab(), 1);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    navigation_observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsReattachAfterCrashTest,
                       TestReattachAfterCrashOnTimeline) {
  RunTestWithPanel("timeline");
}

IN_PROC_BROWSER_TEST_F(DevToolsReattachAfterCrashTest,
                       TestReattachAfterCrashOnNetwork) {
  RunTestWithPanel("network");
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, AutoAttachToWindowOpen) {
  OpenDevToolsWindow(kWindowOpenTestPage, false);
  DevToolsWindowTesting::Get(window_)->SetOpenNewWindowForPopups(true);
  DevToolsWindowCreationObserver observer;
  ASSERT_TRUE(content::ExecuteScript(
      GetInspectedTab(), "window.open('window_open.html', '_blank');"));
  observer.WaitForLoad();
  DispatchOnTestSuite(observer.devtools_window(), "waitForDebuggerPaused");
  DevToolsWindowTesting::CloseDevToolsWindowSync(observer.devtools_window());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, SecondTabAfterDevTools) {
  OpenDevToolsWindow(kDebuggerTestPage, true);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), spawned_test_server()->GetURL(kDebuggerTestPage),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WebContents* second = browser()->tab_strip_model()->GetActiveWebContents();

  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(second));
  EXPECT_EQ("page", agent->GetType());

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest, InspectSharedWorker) {
  ASSERT_TRUE(spawned_test_server()->Start());
  GURL url = spawned_test_server()->GetURL(kSharedWorkerTestPage);
  ui_test_utils::NavigateToURL(browser(), url);

  scoped_refptr<DevToolsAgentHost> host =
      WaitForFirstSharedWorker(kSharedWorkerTestWorker);
  OpenDevToolsWindow(host);
  RunTestFunction(window_, "testSharedWorker");
  CloseDevToolsWindow();
}

// Flaky on multiple platforms. See http://crbug.com/432444
IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest,
                       PauseInSharedWorkerInitialization) {
  ASSERT_TRUE(spawned_test_server()->Start());
  GURL url = spawned_test_server()->GetURL(kReloadSharedWorkerTestPage);
  ui_test_utils::NavigateToURL(browser(), url);

  scoped_refptr<DevToolsAgentHost> host =
      WaitForFirstSharedWorker(kReloadSharedWorkerTestWorker);
  OpenDevToolsWindow(host);

  // We should make sure that the worker inspector has loaded before
  // terminating worker.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization1");

  host->Close();

  // Reload page to restart the worker.
  ui_test_utils::NavigateToURL(browser(), url);

  // Wait until worker script is paused on the debugger statement.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization2");
  CloseDevToolsWindow();
}

class DevToolsAgentHostTest : public InProcessBrowserTest {};

// Tests DevToolsAgentHost retention by its target.
IN_PROC_BROWSER_TEST_F(DevToolsAgentHostTest, TestAgentHostReleased) {
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WebContents* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  DevToolsAgentHost* agent_raw =
      DevToolsAgentHost::GetOrCreateFor(web_contents).get();
  const std::string agent_id = agent_raw->GetId();
  ASSERT_EQ(agent_raw, DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost cannot be found by id";
  browser()->tab_strip_model()->
      CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  ASSERT_FALSE(DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost is not released when the tab is closed";
}

class RemoteDebuggingTest : public extensions::ExtensionApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "9222");

    // Override the extension root path.
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("devtools");
  }
};

// Fails on CrOS. crbug.com/431399
#if defined(OS_CHROMEOS)
#define MAYBE_RemoteDebugger DISABLED_RemoteDebugger
#else
#define MAYBE_RemoteDebugger RemoteDebugger
#endif
IN_PROC_BROWSER_TEST_F(RemoteDebuggingTest, MAYBE_RemoteDebugger) {
  ASSERT_TRUE(RunExtensionTest("target_list")) << message_;
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, PolicyDisallowed) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow::OpenDevToolsWindow(web_contents);
  auto agent_host = content::DevToolsAgentHost::GetOrCreateFor(web_contents);
  ASSERT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

class DevToolsSanityExtensionTest : public extensions::ExtensionBrowserTest {
 public:
  // Installs an extensions, emulating that it has been force-installed by
  // policy.
  // Contains assertions - callers should wrap calls of this method in
  // |ASSERT_NO_FATAL_FAILURE|.
  void ForceInstallExtension(std::string* extension_id) {
    base::FilePath crx_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &crx_path);
    crx_path = crx_path.AppendASCII("devtools")
                   .AppendASCII("extensions")
                   .AppendASCII("options.crx");
    const Extension* extension = InstallExtension(
        crx_path, 1, extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD);
    ASSERT_TRUE(extension);
    *extension_id = extension->id();
  }

  // Same as above, but also fills |*out_web_contents| with a |WebContents|
  // that has been navigated to the force-installed extension.
  void ForceInstallExtensionAndOpen(content::WebContents** out_web_contents) {
    std::string extension_id;
    ForceInstallExtension(&extension_id);
    GURL url("chrome-extension://" + extension_id + "/options.html");
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    *out_web_contents = web_contents;
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsSanityExtensionTest,
                       PolicyDisallowedForForceInstalledExtensions) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsPolicyHandler::Availability::
                           kDisallowedForForceInstalledExtensions));

  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(ForceInstallExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents);
  auto agent_host = content::DevToolsAgentHost::GetOrCreateFor(web_contents);
  ASSERT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

IN_PROC_BROWSER_TEST_F(
    DevToolsSanityExtensionTest,
    PolicyDisallowedForForceInstalledExtensionsAfterNavigation) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsPolicyHandler::Availability::
                           kDisallowedForForceInstalledExtensions));

  std::string extension_id;
  ASSERT_NO_FATAL_FAILURE(ForceInstallExtension(&extension_id));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // It's possible to open DevTools for about:blank.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  DevToolsWindow::OpenDevToolsWindow(web_contents);
  auto agent_host = content::DevToolsAgentHost::GetOrCreateFor(web_contents);
  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));

  // Navigating to extension page should close DevTools.
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-extension://" + extension_id + "/options.html"));
  ASSERT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
}

class DevToolsAllowedByCommandLineSwitch : public DevToolsSanityExtensionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    // Same as |chromeos::switches::kForceDevToolsAvailable|, but used as a
    // literal here so it's possible to verify that the switch does not apply on
    // non-ChromeOS platforms.
    const std::string kForceDevToolsAvailableBase = "force-devtools-available";
#if defined(OS_CHROMEOS)
    ASSERT_EQ(kForceDevToolsAvailableBase,
              chromeos::switches::kForceDevToolsAvailable);
#endif
    command_line->AppendSwitch("--" + kForceDevToolsAvailableBase);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsAllowedByCommandLineSwitch,
                       SwitchOverridesPolicyOnChromeOS) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsPolicyHandler::Availability::
                           kDisallowedForForceInstalledExtensions));

  content::WebContents* web_contents = nullptr;
  ASSERT_NO_FATAL_FAILURE(ForceInstallExtensionAndOpen(&web_contents));

  DevToolsWindow::OpenDevToolsWindow(web_contents);
  auto agent_host = content::DevToolsAgentHost::GetOrCreateFor(web_contents);
#if defined(OS_CHROMEOS)
  ASSERT_TRUE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
#else
  ASSERT_FALSE(DevToolsWindow::FindDevToolsWindow(agent_host.get()));
#endif
}

class DevToolsPixelOutputTests : public DevToolsSanityTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }
};

// This test enables switches::kUseGpuInTests which causes false positives
// with MemorySanitizer. This is also flakey on many configurations.
// See https://crbug.com/510291
IN_PROC_BROWSER_TEST_F(DevToolsPixelOutputTests,
                       DISABLED_TestScreenshotRecording) {
  RunTest("testScreenshotRecording", std::string());
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
    SimulateMouseEvent(web_contents, blink::WebInputEvent::kMouseMove,
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

class DevToolsNetInfoTest : public DevToolsSanityTest {
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
  ~StaticURLDataSource() override = default;

  // content::URLDataSource:
  std::string GetSource() const override { return source_; }
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const GotDataCallback& callback) override {
    std::string data(content_);
    callback.Run(base::RefCountedString::TakeString(&data));
  }
  std::string GetMimeType(const std::string& path) const override {
    return "text/html";
  }
  bool ShouldAddContentSecurityPolicy() const override { return false; }

 private:
  const std::string source_;
  const std::string content_;

  DISALLOW_COPY_AND_ASSIGN(StaticURLDataSource);
};

class MockWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MockWebUIProvider(const std::string& source, const std::string& content)
      : source_(source), content_(content) {}
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
  DISALLOW_COPY_AND_ASSIGN(MockWebUIProvider);
};

// This tests checks that window is correctly initialized when DevTools is
// opened while navigation through history with forward and back actions.
// (crbug.com/627407)
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestWindowInitializedOnNavigateBack) {
  TestChromeWebUIControllerFactory test_factory;
  MockWebUIProvider mock_provider("dummyurl",
                                  "<script>\n"
                                  "  window.abc = 239;\n"
                                  "  console.log(abc);\n"
                                  "</script>");
  test_factory.AddFactoryOverride(GURL("chrome://dummyurl").host(),
                                  &mock_provider);
  content::WebUIControllerFactory::RegisterFactory(&test_factory);

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://dummyurl"));
  DevToolsWindow* window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), true);
  chrome::DuplicateTab(browser());
  chrome::SelectPreviousTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  RunTestFunction(window, "testWindowInitializedOnNavigateBack");

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
  content::WebUIControllerFactory::UnregisterFactoryForTesting(&test_factory);
}

void AddHSTSHost(scoped_refptr<net::URLRequestContextGetter> context,
                 std::string host) {
  net::TransportSecurityState* transport_security_state =
      context->GetURLRequestContext()->transport_security_state();
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  bool include_subdomains = false;
  transport_security_state->AddHSTS(host, expiry, include_subdomains);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestRawHeadersWithRedirectAndHSTS) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_test_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_test_server.Start());
  GURL https_url = https_test_server.GetURL("localhost", "/devtools/image.png");
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            AddHSTSHost,
            base::RetainedRef(browser()->profile()->GetRequestContext()),
            https_url.host()));
  } else {
    base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
    bool include_subdomains = false;
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    base::RunLoop run_loop;
    partition->GetNetworkContext()->AddHSTS(
        https_url.host(), expiry, include_subdomains, run_loop.QuitClosure());
    run_loop.Run();
  }
  ASSERT_TRUE(embedded_test_server()->Start());

  OpenDevToolsWindow(std::string(), false);
  GURL::Replacements replace_scheme;
  replace_scheme.SetSchemeStr("http");
  GURL http_url = https_url.ReplaceComponents(replace_scheme);
  GURL redirect_url =
      embedded_test_server()->GetURL("/server-redirect?" + http_url.spec());

  DispatchOnTestSuite(window_, "testRawHeadersWithHSTS",
                      redirect_url.spec().c_str());
  CloseDevToolsWindow();
}

// Tests that OpenInNewTab filters URLs.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestOpenInNewTabFilter) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  DevToolsUIBindings::Delegate* bindings_delegate_ =
      static_cast<DevToolsUIBindings::Delegate*>(window_);
  std::string test_url =
      spawned_test_server()->GetURL(kDebuggerTestPage).spec();
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
  for (const std::pair<const std::string, const std::string> pair : tests) {
    bindings_delegate_->OpenInNewTab(pair.first);
    i++;

    std::string opened_url = tabs->GetWebContentsAt(i)->GetVisibleURL().spec();
    SCOPED_TRACE(
        base::StringPrintf("while testing URL: %s", pair.first.c_str()));
    EXPECT_EQ(opened_url, pair.second);
  }

  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, LoadNetworkResourceForFrontend) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/"));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/hello.html"));
  window_ =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), false);
  RunTestMethod("testLoadResourceForFrontend", url.spec().c_str());
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, CreateBrowserContext) {
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/devtools/empty.html"));
  window_ = DevToolsWindowTesting::OpenDiscoveryDevToolsWindowSync(
      browser()->profile());
  RunTestMethod("testCreateBrowserContext", url.spec().c_str());
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, DisposeEmptyBrowserContext) {
  window_ = DevToolsWindowTesting::OpenDiscoveryDevToolsWindowSync(
      browser()->profile());
  RunTestMethod("testDisposeEmptyBrowserContext");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsSanityTest, InspectElement) {
  GURL url(embedded_test_server()->GetURL("a.com", "/devtools/oopif.html"));
  GURL iframe_url(
      embedded_test_server()->GetURL("b.com", "/devtools/oopif_frame.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager(tab, url);
  content::TestNavigationManager navigation_manager_iframe(tab, iframe_url);

  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());

  navigation_manager.WaitForNavigationFinished();
  navigation_manager_iframe.WaitForNavigationFinished();
  content::WaitForLoadStop(tab);

  std::vector<RenderFrameHost*> frames = GetInspectedTab()->GetAllFrames();
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

// Flaky on Mus. See https://crbug.com/819285.
IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsSanityTest,
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

  navigation_manager.WaitForNavigationFinished();
  navigation_manager_iframe.WaitForNavigationFinished();
  content::WaitForLoadStop(tab);

  for (auto* frame : GetInspectedTab()->GetAllFrames()) {
    content::WaitForHitTestDataOrChildSurfaceReady(frame);
  }
  DevToolsWindow* window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(), false);
  RunTestFunction(window, "testInputDispatchEventsToOOPIF");
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}
