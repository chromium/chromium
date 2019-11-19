// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugin_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/download_item.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/viz/common/features.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/dump_accessibility_test_helper.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf_features.h"
#include "services/network/public/cpp/features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#endif

using content::WebContents;
using extensions::ExtensionsAPIClient;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

const int kNumberLoadTestParts = 10;

#if defined(OS_MACOSX)
const int kDefaultKeyModifier = blink::WebInputEvent::kMetaKey;
#else
const int kDefaultKeyModifier = blink::WebInputEvent::kControlKey;
#endif

// Calling PluginService::GetPlugins ensures that LoadPlugins is called
// internally. This is an asynchronous task and this method uses a run loop to
// wait for the loading task to complete.
void WaitForPluginServiceToLoad() {
  base::RunLoop run_loop;
  content::PluginService::GetPluginsCallback callback = base::BindOnce(
      [](base::RepeatingClosure quit,
         const std::vector<content::WebPluginInfo>& unused) { quit.Run(); },
      run_loop.QuitClosure());
  content::PluginService::GetInstance()->GetPlugins(std::move(callback));
  run_loop.Run();
}

// Check if the |actual| string matches the string or the string pattern in
// |pattern| and print a readable message if it does not match.
#define ASSERT_MULTILINE_STR_MATCHES(pattern, actual) \
  ASSERT_TRUE(base::MatchPattern(actual, pattern))    \
      << "Expected match pattern:\n"                  \
      << pattern << "\n\nActual:\n"                   \
      << actual

bool GetGuestCallback(WebContents** guest_out, WebContents* guest) {
  EXPECT_FALSE(*guest_out);
  *guest_out = guest;
  // Return false so that we iterate through all the guests and verify there is
  // only one.
  return false;
}

class PDFExtensionTest : public extensions::ExtensionApiTest {
 public:
  ~PDFExtensionTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  // Serve paths prefixed with _test_resources/ from chrome/test/data/pdf.
  base::FilePath GetTestResourcesParentDir() override {
    base::FilePath test_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
    return test_root_path.AppendASCII("pdf");
  }

  bool PdfIsExpectedToLoad(const std::string& pdf_file) {
    const char* const kFailingPdfs[] = {
        "pdf_private/accessibility_crash_1.pdf",
        "pdf_private/cfuzz5.pdf",
        "pdf_private/js.pdf",
        "pdf_private/segv-ecx.pdf",
        "pdf_private/tests.pdf",
    };
    for (const char* failing_pdf : kFailingPdfs) {
      if (failing_pdf == pdf_file)
        return false;
    }
    return true;
  }

  // Runs the extensions test at chrome/test/data/pdf/<filename> on the PDF file
  // at chrome/test/data/pdf/<pdf_filename>, where |filename| is loaded as a JS
  // module.
  void RunTestsInJsModule(const std::string& filename,
                          const std::string& pdf_filename) {
    extensions::ResultCatcher catcher;

    GURL url(embedded_test_server()->GetURL("/pdf/" + pdf_filename));

    // It should be good enough to just navigate to the URL. But loading up the
    // BrowserPluginGuest seems to happen asynchronously as there was flakiness
    // being seen due to the BrowserPluginGuest not being available yet (see
    // crbug.com/498077). So instead use LoadPdf() which ensures that the PDF is
    // loaded before continuing.
    WebContents* guest_contents = LoadPdfGetGuestContents(url);
    ASSERT_TRUE(guest_contents);

    constexpr char kModuleLoaderTemplate[] =
        R"(var s = document.createElement('script');
           s.type = 'module';
           s.src = '_test_resources/%s';
           document.body.appendChild(s);)";

    ASSERT_TRUE(content::ExecuteScript(
        guest_contents,
        base::StringPrintf(kModuleLoaderTemplate, filename.c_str())));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  // Load the PDF at the given URL and ensure it has finished loading. Return
  // true if it loads successfully or false if it fails. If it doesn't finish
  // loading the test will hang. This is done from outside of the BrowserPlugin
  // guest to ensure sending messages to/from the plugin works correctly from
  // there, since the PDFScriptingAPI relies on doing this as well.
  bool LoadPdf(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    WebContents* web_contents = GetActiveWebContents();
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
  }

  // Same as LoadPDF(), but loads into a new tab.
  bool LoadPdfInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    WebContents* web_contents = GetActiveWebContents();
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
  }

  // Same as LoadPdf(), but also returns a pointer to the guest WebContents for
  // the loaded PDF. Returns nullptr if the load fails.
  WebContents* LoadPdfGetGuestContents(const GURL& url) {
    if (!LoadPdf(url))
      return nullptr;

    WebContents* contents = GetActiveWebContents();
    content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
    WebContents* guest_contents = guest_manager->GetFullPageGuest(contents);
    return guest_contents;
  }

  // Load all the PDFs contained in chrome/test/data/<dir_name>. This only runs
  // the test if base::Hash(filename) mod kNumberLoadTestParts == k in order
  // to shard the files evenly across values of k in [0, kNumberLoadTestParts).
  void LoadAllPdfsTest(const std::string& dir_name, int k) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FileEnumerator file_enumerator(test_data_dir.AppendASCII(dir_name),
                                         false, base::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.pdf"));

    size_t count = 0;
    for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
         file_path = file_enumerator.Next()) {
      std::string filename = file_path.BaseName().MaybeAsASCII();
      ASSERT_FALSE(filename.empty());

      std::string pdf_file = dir_name + "/" + filename;
      SCOPED_TRACE(pdf_file);
      if (static_cast<int>(base::Hash(filename) % kNumberLoadTestParts) == k) {
        LOG(INFO) << "Loading: " << pdf_file;
        bool success = LoadPdf(embedded_test_server()->GetURL("/" + pdf_file));
        if (pdf_file == "pdf_private/cfuzz5.pdf")
          continue;
        EXPECT_EQ(PdfIsExpectedToLoad(pdf_file), success) << pdf_file;
      }
      ++count;
    }
    // Assume that there is at least 1 pdf in the directory to guard against
    // someone deleting the directory and silently making the test pass.
    ASSERT_GE(count, 1u);
  }

  void TestGetSelectedTextReply(GURL url, bool expect_success) {
    ASSERT_TRUE(LoadPdf(url));

    // Reach into the guest and hook into it such that it posts back a 'flush'
    // message after every getSelectedTextReply message sent.
    WebContents* web_contents = GetActiveWebContents();
    content::BrowserPluginGuestManager* guest_manager =
        web_contents->GetBrowserContext()->GetGuestManager();
    WebContents* guest_contents = nullptr;
    ASSERT_NO_FATAL_FAILURE(guest_manager->ForEachGuest(
        web_contents, base::BindRepeating(&GetGuestCallback, &guest_contents)));
    ASSERT_TRUE(guest_contents);
    ASSERT_TRUE(content::ExecuteScript(
        guest_contents,
        "var oldSendScriptingMessage = "
        "    PDFViewer.prototype.sendScriptingMessage_;"
        "PDFViewer.prototype.sendScriptingMessage_ = function(message) {"
        "  try {"
        "    oldSendScriptingMessage.bind(this)(message);"
        "  } finally {"
        "    if (message.type == 'getSelectedTextReply')"
        "      this.parentWindow_.postMessage('flush', '*');"
        "  }"
        "}"));

    // Add an event listener for flush messages and request the selected text.
    // If we get a flush message without receiving getSelectedText we know that
    // the message didn't come through.
    bool success = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "window.addEventListener('message', function(event) {"
        "  if (event.data == 'flush')"
        "    window.domAutomationController.send(false);"
        "  if (event.data.type == 'getSelectedTextReply')"
        "    window.domAutomationController.send(true);"
        "});"
        "document.getElementsByTagName('embed')[0].postMessage("
        "    {type: 'getSelectedText'});",
        &success));
    ASSERT_EQ(expect_success, success);
  }

  void ConvertPageCoordToScreenCoord(WebContents* contents, gfx::Point* point) {
    ASSERT_TRUE(contents);
    ASSERT_TRUE(content::ExecuteScript(
        contents,
        "var visiblePage = viewer.viewport.getMostVisiblePage();"
        "var visiblePageDimensions ="
        "    viewer.viewport.getPageScreenRect(visiblePage);"
        "var viewportPosition = viewer.viewport.position;"
        "var screenOffsetX = visiblePageDimensions.x - viewportPosition.x;"
        "var screenOffsetY = visiblePageDimensions.y - viewportPosition.y;"
        "var linkScreenPositionX ="
        "    Math.floor(" +
            base::NumberToString(point->x()) +
            " + screenOffsetX);"
            "var linkScreenPositionY ="
            "    Math.floor(" +
            base::NumberToString(point->y()) +
            " +"
            "    screenOffsetY);"));

    int x;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        contents,
        "window.domAutomationController.send(linkScreenPositionX);",
        &x));

    int y;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        contents,
        "window.domAutomationController.send(linkScreenPositionY);",
        &y));

    point->SetPoint(x, y);
  }

  WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  int CountPDFProcesses() {
    int result = -1;
    base::RunLoop run_loop;
    base::PostTaskAndReply(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&PDFExtensionTest::CountPDFProcessesOnIOThread,
                       base::Unretained(this), base::Unretained(&result)),
        run_loop.QuitClosure());
    run_loop.Run();
    return result;
  }

  void CountPDFProcessesOnIOThread(int* result) {
    auto* service = content::PluginService::GetInstance();
    *result = service->CountPpapiPluginProcessesForProfile(
        base::FilePath(ChromeContentClient::kPDFPluginPath),
        browser()->profile()->GetPath());
  }

  // Installs the specified service worker and tests navigating to a PDF in its
  // scope.
  void RunServiceWorkerTest(const std::string& worker_path) {
    // Install the service worker.
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/service_worker/create_service_worker.html"));
    EXPECT_EQ("DONE",
              EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "register('" + worker_path + "', '/pdf');"));

    // Navigate to a PDF in the service worker's scope. It should load.
    RunTestsInJsModule("basic_test.js", "test.pdf");
    // Ensure it loaded in a PPAPI process.
    EXPECT_EQ(1, CountPDFProcesses());
  }
};

class PDFExtensionTestWithTestGuestViewManager : public PDFExtensionTest {
 public:
  PDFExtensionTestWithTestGuestViewManager() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

 protected:
  TestGuestViewManager* GetGuestViewManager() {
    // TODO(wjmaclean): Re-implement FromBrowserContext in the
    // TestGuestViewManager class to avoid all callers needing this cast.
    auto* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated can and will get called
    // before a guest is created. Since GuestViewManager is usually not created
    // until the first guest is created, this means that |manager| will be
    // nullptr if trying to use the manager to wait for the first guest. Because
    // of this, the manager must be created here if it does not already exist.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

 private:
  TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionTestWithTestGuestViewManager,
                       LoadingPdfDoesNotStealFocus) {
  // Load test HTML, and verify the text area has focus.
  GURL main_url(embedded_test_server()->GetURL("/pdf/two_iframes.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  auto* embedder_web_contents = GetActiveWebContents();

  // Make sure we can see the iframe's document.
  ASSERT_TRUE(
      content::EvalJs(embedder_web_contents,
                      "new Promise((resolve) => {"
                      "  var iframe1 = document.getElementById('iframe1');"
                      "  var iframe1doc = iframe1.contentDocument;"
                      "  resolve(iframe1doc != null);"
                      "});")
          .ExtractBool());

  // Make sure the text area is focused. First, we must explicitly focus the
  // child iframe containing the text area.
  content::RenderFrameHost* main_frame = embedder_web_contents->GetMainFrame();
  content::RenderFrameHost* child_text_area = ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(content::ExecJs(child_text_area, "window.focus();"));
  ASSERT_TRUE(content::EvalJs(
                  embedder_web_contents,
                  "new Promise((resolve) => {"
                  "  iframe1doc = "
                  "      document.getElementById('iframe1').contentDocument;"
                  "  resolve(iframe1doc.hasFocus());"
                  "});")
                  .ExtractBool());

  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(content::ExecJs(
      embedder_web_contents,
      content::JsReplace("document.getElementById('iframe2').src = $1;",
                         pdf_url.spec())));

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);

  // Make sure the load has started, before waiting for it to stop.
  // This is a little hacky, but will unjank the test for now.
  // TODO(wjmaclean): Make this less hacky.
  while (!guest_web_contents->IsLoading() &&
         !guest_web_contents->GetController().GetLastCommittedEntry()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Make sure the text area still has focus.
  ASSERT_TRUE(
      content::EvalJs(
          embedder_web_contents,
          "new Promise((resolve) => {"
          "  iframe1doc = "
          "      document.getElementById('iframe1').contentDocument;"
          "  text_area = iframe1doc.getElementById('text_area');"
          "  text_area_is_active = iframe1doc.activeElement == text_area;"
          "  resolve(iframe1doc.hasFocus() && text_area_is_active);"
          "});")
          .ExtractBool());
}

// This test is a re-implementation of
// WebPluginContainerTest.PluginDocumentPluginIsFocused, which was introduced
// for https://crbug.com/536637. The original implementation checked that the
// BrowserPlugin hosting the pdf extension was focused; in this re-write, we
// make sure the guest view's WebContents has focus.
IN_PROC_BROWSER_TEST_F(PDFExtensionTestWithTestGuestViewManager,
                       PdfInMainFrameHasFocus) {
  // Load test HTML, and verify the text area has focus.
  GURL main_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  auto* embedder_web_contents = GetActiveWebContents();

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Make sure the guest WebContents has focus.
  EXPECT_EQ(guest_web_contents,
            content::GetFocusedWebContents(embedder_web_contents));
}

// This test verifies that when a PDF is loaded, that (i) the embedder
// WebContents' html consists of a single <embed> tag with appropriate
// properties, and (ii) that the guest WebContents finishes loading and
// has the correct URL for the PDF extension.
// TODO(wjmaclean): Are there any attributes we can/should test with respect to
// the extension's loaded html?
IN_PROC_BROWSER_TEST_F(PDFExtensionTestWithTestGuestViewManager,
                       PdfExtensionLoadedInGuest) {
  // Load test HTML, and verify the text area has focus.
  GURL main_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  auto* embedder_web_contents = GetActiveWebContents();

  // Verify the pdf has loaded.
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(guest_web_contents);
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_web_contents));

  // Verify we loaded the extension.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url, guest_web_contents->GetURL());
  EXPECT_EQ(main_url, embedder_web_contents->GetURL());

  // Make sure the embedder has the correct html boilerplate.
  EXPECT_EQ(1, content::EvalJs(embedder_web_contents,
                               "document.body.children.length;")
                   .ExtractInt());
  EXPECT_EQ("EMBED", content::EvalJs(embedder_web_contents,
                                     "document.body.firstChild.tagName;")
                         .ExtractString());
  EXPECT_EQ("application/pdf", content::EvalJs(embedder_web_contents,
                                               "document.body.firstChild.type;")
                                   .ExtractString());
  EXPECT_EQ("about:blank", content::EvalJs(embedder_web_contents,
                                           "document.body.firstChild.src;")
                               .ExtractString());
  EXPECT_TRUE(
      content::EvalJs(embedder_web_contents,
                      "document.body.firstChild.hasAttribute('internalid');")
          .ExtractBool());
}

class PDFExtensionLoadTest : public PDFExtensionTest,
                             public testing::WithParamInterface<int> {
 public:
  PDFExtensionLoadTest() {}
};

using PDFExtensionHitTestTest = PDFExtensionTest;

class PDFAnnotationsTest : public PDFExtensionTest {
 public:
  PDFAnnotationsTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(chrome_pdf::features::kPDFAnnotations);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Disabled because it's flaky.
// See the issue for details: https://crbug.com/826055.
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_Load DISABLED_Load
#else
#define MAYBE_Load Load
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionLoadTest, MAYBE_Load) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Load private PDFs.
  LoadAllPdfsTest("pdf_private", GetParam());
#endif
  // Load public PDFs.
  LoadAllPdfsTest("pdf", GetParam());
}

class DownloadAwaiter : public content::DownloadManager::Observer {
 public:
  DownloadAwaiter() {}
  ~DownloadAwaiter() override {}

  const GURL& GetLastUrl() {
    // Wait until the download has been created.
    download_run_loop_.Run();
    return last_url_;
  }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    last_url_ = item->GetURL();
    download_run_loop_.Quit();
  }

 private:
  base::RunLoop download_run_loop_;
  GURL last_url_;
};

// Tests behavior when the PDF plugin is disabled in preferences.
class PDFPluginDisabledTest : public PDFExtensionTest {
 public:
  PDFPluginDisabledTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kEnablePluginPlaceholderTesting);
  }

  void SetUpOnMainThread() override {
    PDFExtensionTest::SetUpOnMainThread();

    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    profile->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                    true);

    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser_context);
    download_awaiter_ = std::make_unique<DownloadAwaiter>();
    download_manager->AddObserver(download_awaiter_.get());
  }

  void TearDownOnMainThread() override {
    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser_context);
    download_manager->RemoveObserver(download_awaiter_.get());

    // Cancel all downloads to shut down cleanly.
    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    for (auto* item : downloads) {
      item->Cancel(false);
    }

    PDFExtensionTest::TearDownOnMainThread();
  }

  void ClickOpenButtonInIframe() {
    int iframes_found = 0;
    for (auto* host : GetActiveWebContents()->GetAllFrames()) {
      if (host != GetActiveWebContents()->GetMainFrame()) {
        ASSERT_TRUE(content::ExecJs(
            host, "document.getElementById('open-button').click();"));
        ++iframes_found;
      }
    }
    ASSERT_EQ(1, iframes_found);
  }

  void ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch() {
    // Validate that we downloaded a single PDF and didn't launch the PDF
    // plugin.
    GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
    EXPECT_EQ(pdf_url, AwaitAndGetLastDownloadedUrl());
    EXPECT_EQ(1u, GetNumberOfDownloads());
    EXPECT_EQ(0, CountPDFProcesses());
  }

 private:
  size_t GetNumberOfDownloads() {
    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser_context);

    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    return downloads.size();
  }

  const GURL& AwaitAndGetLastDownloadedUrl() {
    return download_awaiter_->GetLastUrl();
  }

  std::unique_ptr<DownloadAwaiter> download_awaiter_;
};

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest, DirectNavigationToPDF) {
  // Navigate to a PDF and test that it is downloaded.
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), pdf_url);

  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest, EmbedPdfPlaceholderWithCSP) {
  // Navigate to a page with CSP that uses <embed> to embed a PDF as a plugin.
  GURL embed_page_url =
      embedded_test_server()->GetURL("/pdf/pdf_embed_csp.html");
  ui_test_utils::NavigateToURL(browser(), embed_page_url);
  PluginTestUtils::WaitForPlaceholderReady(GetActiveWebContents(), "pdf_embed");

  // Fake a click on the <embed>, then press Enter to trigger the download.
  gfx::Point point_in_pdf(100, 100);
  content::SimulateRoutedMouseClickAt(
      GetActiveWebContents(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, point_in_pdf);
  content::SimulateKeyPress(GetActiveWebContents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest, IframePdfPlaceholderWithCSP) {
  // Navigate to a page that uses <iframe> to embed a PDF as a plugin.
  GURL iframe_page_url =
      embedded_test_server()->GetURL("/pdf/pdf_iframe_csp.html");
  ui_test_utils::NavigateToURL(browser(), iframe_page_url);

  ClickOpenButtonInIframe();
  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest,
                       IframePlaceholderInjectedIntoNewWindow) {
  // This is an unusual test to verify crbug.com/924823. We are injecting the
  // HTML for a PDF IFRAME into a newly created popup with an undefined URL.
  ASSERT_TRUE(
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "new Promise((resolve) => {"
              "  var popup = window.open();"
              "  popup.document.writeln("
              "      '<iframe id=\"pdf_iframe\" src=\"' + $1 + '\"></iframe>');"
              "  var iframe = popup.document.getElementById('pdf_iframe');"
              "  iframe.onload = () => resolve(true);"
              "});",
              embedded_test_server()->GetURL("/pdf/test.pdf").spec()))
          .ExtractBool());

  ClickOpenButtonInIframe();
  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

// We break PDFExtensionLoadTest up into kNumberLoadTestParts.
INSTANTIATE_TEST_SUITE_P(PDFTestFiles,
                         PDFExtensionLoadTest,
                         testing::Range(0, kNumberLoadTestParts));

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Basic) {
  RunTestsInJsModule("basic_test.js", "test.pdf");

  // Ensure it loaded in a PPAPI process.
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BasicPlugin) {
  RunTestsInJsModule("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Viewport) {
  RunTestsInJsModule("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Layout3) {
  RunTestsInJsModule("layout_test.js", "test-layout3.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Layout4) {
  RunTestsInJsModule("layout_test.js", "test-layout4.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Bookmark) {
  RunTestsInJsModule("bookmarks_test.js", "test-bookmarks-with-zoom.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Navigator) {
  RunTestsInJsModule("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ParamsParser) {
  RunTestsInJsModule("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ZoomManager) {
  RunTestsInJsModule("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, GestureDetector) {
  RunTestsInJsModule("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TouchHandling) {
  RunTestsInJsModule("touch_handling_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ToolbarManager) {
  RunTestsInJsModule("toolbar_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Title) {
  RunTestsInJsModule("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, WhitespaceTitle) {
  RunTestsInJsModule("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Beep) {
  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
}

#if defined(OS_CHROMEOS)
// TODO(https://crbug.com/920684): Test times out.
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(_DEBUG)
#define MAYBE_AnnotationsFeatureEnabled DISABLED_AnnotationsFeatureEnabled
#else
#define MAYBE_AnnotationsFeatureEnabled AnnotationsFeatureEnabled
#endif
IN_PROC_BROWSER_TEST_F(PDFAnnotationsTest, MAYBE_AnnotationsFeatureEnabled) {
  RunTestsInJsModule("annotations_feature_enabled_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, AnnotationsFeatureDisabled) {
  RunTestsInJsModule("annotations_feature_disabled_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Printing) {
  RunTestsInJsModule("printing_icon_test.js", "test.pdf");
}
#endif

// TODO(tsepez): See https://crbug.com/696650.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DISABLED_NoBeep) {
  // Block the exact query from pdf/main.js while still allowing enough
  // JavaScript to run in the extension for this test harness to complete
  // its work.
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(
          "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai"),
      ContentSettingsType::JAVASCRIPT, std::string(), CONTENT_SETTING_BLOCK);

  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PageChange) {
  RunTestsInJsModule("page_change_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Metrics) {
  RunTestsInJsModule("metrics_test.js", "test.pdf");
}

// Ensure that the internal PDF plugin application/x-google-chrome-pdf won't be
// loaded if it's not loaded in the chrome extension page.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureInternalPluginDisabled) {
  std::string url = embedded_test_server()->GetURL("/pdf/test.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><body>"
      "<embed type=\"application/x-google-chrome-pdf\" src=\"" +
      url +
      "\">"
      "</body></html>";
  ui_test_utils::NavigateToURL(browser(), GURL(data_url));
  WebContents* web_contents = GetActiveWebContents();
  bool plugin_loaded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "var plugin_loaded = "
      "    document.getElementsByTagName('embed')[0].postMessage !== undefined;"
      "window.domAutomationController.send(plugin_loaded);",
      &plugin_loaded));
  ASSERT_FALSE(plugin_loaded);
}

// Ensure cross-origin replies won't work for getSelectedText.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureCrossOriginRepliesBlocked) {
  std::string url = embedded_test_server()->GetURL("/pdf/test.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><body>"
      "<embed type=\"application/pdf\" src=\"" +
      url +
      "\">"
      "</body></html>";
  TestGetSelectedTextReply(GURL(data_url), false);
}

// Ensure same-origin replies do work for getSelectedText.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureSameOriginRepliesAllowed) {
  TestGetSelectedTextReply(embedded_test_server()->GetURL("/pdf/test.pdf"),
                           true);
}

// TODO(crbug.com/1004425): Should be allowed?
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureOpaqueOriginRepliesBlocked) {
  TestGetSelectedTextReply(
      embedded_test_server()->GetURL("/pdf/data_url_rectangles.html"), false);
}

// Ensure that the PDF component extension cannot be loaded directly.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BlockDirectAccess) {
  WebContents* web_contents = GetActiveWebContents();

  std::unique_ptr<content::ConsoleObserverDelegate> console_delegate(
      new content::ConsoleObserverDelegate(
          web_contents,
          "*Streams are only available from a mime handler view guest.*"));
  web_contents->SetDelegate(console_delegate.get());
  GURL forbiddenUrl(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html?"
      "https://example.com/notrequested.pdf");
  ui_test_utils::NavigateToURL(browser(), forbiddenUrl);

  console_delegate->Wait();

  // Didn't launch a PPAPI process.
  EXPECT_EQ(0, CountPDFProcesses());
}

// This test ensures that PDF can be loaded from local file
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsurePDFFromLocalFileLoads) {
  GURL test_pdf_url;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("pdf"));
    base::FilePath test_data_file = test_data_dir.AppendASCII("test.pdf");
    ASSERT_TRUE(PathExists(test_data_file));
    test_pdf_url = GURL("file://" + test_data_file.MaybeAsASCII());
  }
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // Did launch a PPAPI process.
  EXPECT_EQ(1, CountPDFProcesses());
}

// Tests that PDF with no filename extension can be loaded from local file.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ExtensionlessPDFLocalFileLoads) {
  GURL test_pdf_url;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.AppendASCII("pdf");
    base::FilePath test_data_file = test_data_dir.AppendASCII("imgpdf");
    ASSERT_TRUE(PathExists(test_data_file));
    test_pdf_url = GURL("file://" + test_data_file.MaybeAsASCII());
  }
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // Did launch a PPAPI process.
  EXPECT_EQ(1, CountPDFProcesses());
}

// This test ensures that link permissions are enforced properly in PDFs.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkPermissions) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // chrome://favicon links should be allowed for PDFs, while chrome://settings
  // links should not.
  GURL valid_link_url(std::string(chrome::kChromeUIFaviconURL) +
                      "https://www.google.ca/");
  GURL invalid_link_url(chrome::kChromeUISettingsURL);

  GURL unfiltered_valid_link_url(valid_link_url);
  content::RenderProcessHost* rph =
      guest_contents->GetMainFrame()->GetProcess();
  rph->FilterURL(true, &valid_link_url);
  rph->FilterURL(true, &invalid_link_url);

  // Invalid link URLs should be changed to "about:blank#blocked" when filtered.
  EXPECT_EQ(unfiltered_valid_link_url, valid_link_url);
  EXPECT_EQ(GURL(content::kBlockedURL), invalid_link_url);
}

// This test ensures that titles are set properly for PDFs without /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithNoTitle) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(base::ASCIIToUTF16("test.pdf"), guest_contents->GetTitle());
  EXPECT_EQ(base::ASCIIToUTF16("test.pdf"), GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for PDFs with /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithTitle) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-title.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(base::ASCIIToUTF16("PDF title test"), guest_contents->GetTitle());
  EXPECT_EQ(base::ASCIIToUTF16("PDF title test"),
            GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for embedded PDFs with /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithEmbeddedPdf) {
  std::string url =
      embedded_test_server()->GetURL("/pdf/test-title.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><head><title>TabTitleWithEmbeddedPdf</title></head><body>"
      "<embed type=\"application/pdf\" src=\"" +
      url +
      "\"></body></html>";
  ASSERT_TRUE(LoadPdf(GURL(data_url)));
  EXPECT_EQ(base::ASCIIToUTF16("TabTitleWithEmbeddedPdf"),
            GetActiveWebContents()->GetTitle());
}

// Flaky, http://crbug.com/767427
#if defined(OS_WIN)
#define MAYBE_PdfZoomWithoutBubble DISABLED_PdfZoomWithoutBubble
#else
#define MAYBE_PdfZoomWithoutBubble PdfZoomWithoutBubble
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, MAYBE_PdfZoomWithoutBubble) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  WebContents* web_contents = GetActiveWebContents();

  // The PDF viewer always starts at default zoom, which for tests is 100% or
  // zoom level 0.0. Here we look at the presets to find the next zoom level
  // above 0. Ideally we should look at the zoom levels from the PDF viewer
  // javascript, but we assume they'll always match the browser presets, which
  // are easier to access.
  std::vector<double> preset_zoom_levels = zoom::PageZoom::PresetZoomLevels(0);
  auto it = std::find(preset_zoom_levels.begin(), preset_zoom_levels.end(), 0);
  ASSERT_TRUE(it != preset_zoom_levels.end());
  it++;
  ASSERT_TRUE(it != preset_zoom_levels.end());
  double new_zoom_level = *it;

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // We expect a ZoomChangedEvent with can_show_bubble == false if the PDF
  // extension behaviour is properly picked up. The test times out otherwise.
  zoom::ZoomChangedWatcher watcher(
      zoom_controller, zoom::ZoomController::ZoomChangedEventData(
                           web_contents, 0, new_zoom_level,
                           zoom::ZoomController::ZOOM_MODE_MANUAL, false));

  // Zoom PDF via script.
#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
  EXPECT_EQ(nullptr, ZoomBubbleView::GetZoomBubble());
#endif
  ASSERT_TRUE(
      content::ExecuteScript(guest_contents, "viewer.viewport.zoomIn();"));

  watcher.Wait();
#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
  EXPECT_EQ(nullptr, ZoomBubbleView::GetZoomBubble());
#endif
}

static std::string DumpPdfAccessibilityTree(const ui::AXTreeUpdate& ax_tree) {
  // Create a string representation of the tree starting with the embedded
  // object.
  std::string ax_tree_dump;
  std::map<int32_t, int> id_to_indentation;
  bool found_embedded_object = false;
  for (auto& node : ax_tree.nodes) {
    if (node.role == ax::mojom::Role::kEmbeddedObject)
      found_embedded_object = true;
    if (!found_embedded_object)
      continue;

    int indent = id_to_indentation[node.id];
    ax_tree_dump += std::string(2 * indent, ' ');
    ax_tree_dump += ui::ToString(node.role);

    std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    base::ReplaceChars(name, "\r\n", "", &name);
    if (!name.empty())
      ax_tree_dump += " '" + name + "'";
    ax_tree_dump += "\n";
    for (size_t j = 0; j < node.child_ids.size(); ++j)
      id_to_indentation[node.child_ids[j]] = indent + 1;
  }

  return ax_tree_dump;
}

// This is a pattern with a few wildcards due to a PDF bug where the
// fi ligature is not parsed correctly on some systems.
// http://crbug.com/701427

static const char kExpectedPDFAXTreePattern[] =
    "embeddedObject\n"
    "  document\n"
    "    region 'Page 1'\n"
    "      paragraph\n"
    "        staticText '1 First Section'\n"
    "          inlineTextBox '1 '\n"
    "          inlineTextBox 'First Section'\n"
    "      paragraph\n"
    "        staticText 'This is the *rst section.'\n"
    "          inlineTextBox 'This is the *rst section.'\n"
    "      paragraph\n"
    "        staticText '1'\n"
    "          inlineTextBox '1'\n"
    "    region 'Page 2'\n"
    "      paragraph\n"
    "        staticText '1.1 First Subsection'\n"
    "          inlineTextBox '1.1 '\n"
    "          inlineTextBox 'First Subsection'\n"
    "      paragraph\n"
    "        staticText 'This is the *rst subsection.'\n"
    "          inlineTextBox 'This is the *rst subsection.'\n"
    "      paragraph\n"
    "        staticText '2'\n"
    "          inlineTextBox '2'\n"
    "    region 'Page 3'\n"
    "      paragraph\n"
    "        staticText '2 Second Section'\n"
    "          inlineTextBox '2 '\n"
    "          inlineTextBox 'Second Section'\n"
    "      paragraph\n"
    "        staticText '3'\n"
    "          inlineTextBox '3'\n";

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibility) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);

  ASSERT_MULTILINE_STR_MATCHES(kExpectedPDFAXTreePattern, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityEnableLater) {
  // In this test, load the PDF file first, with accessibility off.
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // Now enable accessibility globally, and assert that the PDF accessibility
  // tree loads.
  EnableAccessibilityForWebContents(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STR_MATCHES(kExpectedPDFAXTreePattern, ax_tree_dump);
}

bool RetrieveGuestContents(WebContents** out_guest_contents,
                           WebContents* in_guest_contents) {
  *out_guest_contents = in_guest_contents;
  return true;
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityInIframe) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_iframe_url(embedded_test_server()->GetURL("/pdf/test-iframe.html"));
  ui_test_utils::NavigateToURL(browser(), test_iframe_url);
  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  WebContents* guest_contents = nullptr;
  content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
  guest_manager->ForEachGuest(contents,
                              base::Bind(&RetrieveGuestContents,
                                         &guest_contents));
  ASSERT_TRUE(guest_contents);

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STR_MATCHES(kExpectedPDFAXTreePattern, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityInOOPIF) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_iframe_url(embedded_test_server()->GetURL(
      "/pdf/test-cross-site-iframe.html"));
  ui_test_utils::NavigateToURL(browser(), test_iframe_url);
  WebContents* contents = GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  WebContents* guest_contents = nullptr;
  content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
  guest_manager->ForEachGuest(contents,
                              base::Bind(&RetrieveGuestContents,
                                         &guest_contents));
  ASSERT_TRUE(guest_contents);

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STR_MATCHES(kExpectedPDFAXTreePattern, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityWordBoundaries) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);

  bool found = false;
  for (auto& node : ax_tree.nodes) {
    std::string name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    if (node.role == ax::mojom::Role::kInlineTextBox &&
        name == "First Section\r\n") {
      found = true;
      std::vector<int32_t> word_starts =
          node.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
      std::vector<int32_t> word_ends =
          node.GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
      ASSERT_EQ(2U, word_starts.size());
      ASSERT_EQ(2U, word_ends.size());
      EXPECT_EQ(0, word_starts[0]);
      EXPECT_EQ(5, word_ends[0]);
      EXPECT_EQ(6, word_starts[1]);
      EXPECT_EQ(13, word_ends[1]);
    }
  }
  ASSERT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilitySelection) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WebContents* web_contents = GetActiveWebContents();
  CHECK(content::ExecuteScript(
      web_contents,
      "document.getElementsByTagName('embed')[0].postMessage("
      "{type: 'selectAll'});"));

  EnableAccessibilityForWebContents(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree_update =
      GetAccessibilityTreeSnapshot(guest_contents);
  ui::AXTree ax_tree(ax_tree_update);

  // Ensure that the selection spans the beginning of the first text
  // node to the end of the last one.
  ui::AXNode* sel_start_node =
      ax_tree.GetFromId(ax_tree.data().sel_anchor_object_id);
  ASSERT_TRUE(sel_start_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, sel_start_node->data().role);
  std::string start_node_name = sel_start_node->data().GetStringAttribute(
      ax::mojom::StringAttribute::kName);
  EXPECT_EQ("1 First Section\r\n", start_node_name);
  EXPECT_EQ(0, ax_tree.data().sel_anchor_offset);
  ui::AXNode* para = sel_start_node->parent();
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->data().role);
  ui::AXNode* region = para->parent();
  EXPECT_EQ(ax::mojom::Role::kRegion, region->data().role);

  ui::AXNode* sel_end_node =
      ax_tree.GetFromId(ax_tree.data().sel_focus_object_id);
  ASSERT_TRUE(sel_end_node);
  std::string end_node_name = sel_end_node->data().GetStringAttribute(
      ax::mojom::StringAttribute::kName);
  EXPECT_EQ("3", end_node_name);
  EXPECT_EQ(static_cast<int>(end_node_name.size()),
            ax_tree.data().sel_focus_offset);
  para = sel_end_node->parent();
  EXPECT_EQ(ax::mojom::Role::kParagraph, para->data().role);
  region = para->parent();
  EXPECT_EQ(ax::mojom::Role::kRegion, region->data().role);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Test a particular PDF encountered in the wild that triggered a crash
// when accessibility is enabled.  (http://crbug.com/668724)
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityTextRunCrash) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_pdf_url(embedded_test_server()->GetURL(
      "/pdf_private/accessibility_crash_2.pdf"));

  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");
}
#endif

// Test that if the plugin tries to load a URL that redirects then it will fail
// to load. This is to avoid the source origin of the document changing during
// the redirect, which can have security implications. https://crbug.com/653749.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, RedirectsFailInPlugin) {
  RunTestsInJsModule("redirects_fail_test.js", "test.pdf");
}

// Test that even if a different tab is selected when a navigation occurs,
// the correct tab still gets navigated (see crbug.com/672563).
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, NavigationOnCorrectTab) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  WebContents* web_contents = GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  content::TestNavigationObserver active_navigation_observer(
      active_web_contents);
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::ExecuteScript(
      guest_contents,
      "viewer.navigator_.navigate("
      "    'www.example.com',"
      "    PdfNavigator.WindowOpenDisposition.CURRENT_TAB);"));
  navigation_observer.Wait();

  EXPECT_FALSE(navigation_observer.last_navigation_url().is_empty());
  EXPECT_TRUE(active_navigation_observer.last_navigation_url().is_empty());
  EXPECT_FALSE(active_web_contents->GetController().GetPendingEntry());
}

// Flaky: https://crbug.com/851805
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DISABLED_MultipleDomains) {
  for (const auto& url :
       {embedded_test_server()->GetURL("a.com", "/pdf/test.pdf"),
        embedded_test_server()->GetURL("b.com", "/pdf/test.pdf"),
        embedded_test_server()->GetURL("c.com", "/pdf/test.pdf"),
        embedded_test_server()->GetURL("d.com", "/pdf/test.pdf")}) {
    ASSERT_TRUE(LoadPdfInNewTab(url));
  }
  EXPECT_EQ(4, CountPDFProcesses());
}

class PDFExtensionLinkClickTest : public PDFExtensionTest {
 public:
  PDFExtensionLinkClickTest() : guest_contents_(nullptr) {}
  ~PDFExtensionLinkClickTest() override {}

 protected:
  void LoadTestLinkPdfGetGuestContents() {
    GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-link.pdf"));
    guest_contents_ = LoadPdfGetGuestContents(test_pdf_url);
    ASSERT_TRUE(guest_contents_);
  }

  // The rectangle of the link in test-link.pdf is [72 706 164 719] in PDF user
  // space. To calculate a position inside this rectangle, several
  // transformations have to be applied:
  // [a] (110, 110) in Blink page coordinates ->
  // [b] (219, 169) in Blink screen coordinates ->
  // [c] (115, 169) in PDF Device space coordinates ->
  // [d] (82.5, 709.5) in PDF user space coordinates.
  // This performs the [a] to [b] transformation, since that is the coordinate
  // space content::SimulateMouseClickAt() needs.
  gfx::Point GetLinkPosition() {
    gfx::Point link_position(110, 110);
    ConvertPageCoordToScreenCoord(guest_contents_, &link_position);
    return link_position;
  }

  void SetGuestContents(WebContents* guest_contents) {
    ASSERT_TRUE(guest_contents);
    guest_contents_ = guest_contents;
  }

  content::WebContents* GetWebContentsForInputRouting() {
    return content::MimeHandlerViewMode::UsesCrossProcessFrame()
               ? guest_contents_
               : GetActiveWebContents();
  }

 private:
  WebContents* guest_contents_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, CtrlLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, Middle) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(GetWebContentsForInputRouting(), 0,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, CtrlShiftLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  const int modifiers = blink::WebInputEvent::kShiftKey | kDefaultKeyModifier;

  content::SimulateMouseClickAt(GetWebContentsForInputRouting(), modifiers,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, ShiftMiddle) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kShiftKey,
      blink::WebMouseEvent::Button::kMiddle, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, ShiftLeft) {
  LoadTestLinkPdfGetGuestContents();

  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kShiftKey,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::WaitForBrowserToOpen();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

// This test opens a PDF by clicking a link via javascript and verifies that
// the PDF is loaded and functional by clicking a link in the PDF. The link
// click in the PDF opens a new tab. The main page handles the pageShow event
// and updates the history state.
IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, OpenPDFWithReplaceState) {
  // Navigate to the main page.
  GURL test_url(
      embedded_test_server()->GetURL("/pdf/pdf_href_replace_state.html"));
  ui_test_utils::NavigateToURL(browser(), test_url);
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Click on the link which opens the PDF via JS.
  content::TestNavigationObserver navigation_observer(web_contents);
  const char kPdfLinkClick[] = "document.getElementById('link').click();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, kPdfLinkClick));
  navigation_observer.Wait();
  const GURL& current_url = web_contents->GetURL();
  ASSERT_EQ("/pdf/test-link.pdf", current_url.path());

  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  // Now click on the link to example.com in the PDF. This should open up a new
  // tab.
  content::BrowserPluginGuestManager* guest_manager =
      web_contents->GetBrowserContext()->GetGuestManager();
  SetGuestContents(guest_manager->GetFullPageGuest(web_contents));

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  // We should have two tabs now. One with the PDF and the second for
  // example.com
  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

class PDFExtensionInternalLinkClickTest : public PDFExtensionTest {
 public:
  PDFExtensionInternalLinkClickTest() : guest_contents_(nullptr) {}
  ~PDFExtensionInternalLinkClickTest() override {}

 protected:
  void LoadTestLinkPdfGetGuestContents() {
    GURL test_pdf_url(
        embedded_test_server()->GetURL("/pdf/test-internal-link.pdf"));
    guest_contents_ = LoadPdfGetGuestContents(test_pdf_url);
    ASSERT_TRUE(guest_contents_);
  }

  gfx::Point GetLinkPosition() {
    // The whole first page is a link.
    gfx::Point link_position(100, 100);
    ConvertPageCoordToScreenCoord(guest_contents_, &link_position);
    return link_position;
  }

  content::WebContents* GetWebContentsForInputRouting() {
    return content::MimeHandlerViewMode::UsesCrossProcessFrame()
               ? guest_contents_
               : GetActiveWebContents();
  }

 private:
  WebContents* guest_contents_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, CtrlLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), kDefaultKeyModifier,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, Middle) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(GetWebContentsForInputRouting(), 0,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, ShiftLeft) {
  LoadTestLinkPdfGetGuestContents();

  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  WebContents* web_contents = GetActiveWebContents();

  content::SimulateMouseClickAt(
      GetWebContentsForInputRouting(), blink::WebInputEvent::kShiftKey,
      blink::WebMouseEvent::Button::kLeft, GetLinkPosition());
  ui_test_utils::WaitForBrowserToOpen();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

class PDFExtensionClipboardTest : public PDFExtensionTest {
 public:
  PDFExtensionClipboardTest() : guest_contents_(nullptr) {}
  ~PDFExtensionClipboardTest() override {}

  void SetUpOnMainThread() override {
    PDFExtensionTest::SetUpOnMainThread();
    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDownOnMainThread() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    PDFExtensionTest::TearDownOnMainThread();
  }

  void LoadTestComboBoxPdfGetGuestContents() {
    GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/combobox_form.pdf"));
    guest_contents_ = LoadPdfGetGuestContents(test_pdf_url);
    ASSERT_TRUE(guest_contents_);
  }

  // Returns a point near the left edge of the editable combo box in
  // combobox_form.pdf, inside the combo box rect. The point is in Blink screen
  // coordinates.
  //
  // The combo box's rect is [100 50 200 80] in PDF user space. (136, 318) in
  // Blink page coordinates corresponds to approximately (102, 62) in PDF user
  // space coordinates. See PDFExtensionLinkClickTest::GetLinkPosition() for
  // more information on all the coordinate systems involved.
  gfx::Point GetEditableComboBoxLeftPosition() {
    gfx::Point position(136, 318);
    ConvertPageCoordToScreenCoord(guest_contents_, &position);
    return position;
  }

  void ClickLeftSideOfEditableComboBox() {
    content::SimulateMouseClickAt(GetWebContentsForInputRouting(), 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  GetEditableComboBoxLeftPosition());
  }

  void TypeHello() {
    auto* web_contents = GetWebContentsForInputRouting();
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('H'),
                              ui::DomCode::US_H, ui::VKEY_H, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('E'),
                              ui::DomCode::US_E, ui::VKEY_E, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('L'),
                              ui::DomCode::US_L, ui::VKEY_L, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('L'),
                              ui::DomCode::US_L, ui::VKEY_L, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                              ui::DomCode::US_O, ui::VKEY_O, false, false,
                              false, false);
  }

  // Presses the left arrow key.
  void PressLeftArrow() {
    content::SimulateKeyPressWithoutChar(
        GetWebContentsForInputRouting(), ui::DomKey::ARROW_LEFT,
        ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT, false, false, false, false);
  }

  // Presses down shift, presses the left arrow, and lets go of shift.
  void PressShiftLeftArrow() {
    content::SimulateKeyPressWithoutChar(GetWebContentsForInputRouting(),
                                         ui::DomKey::ARROW_LEFT,
                                         ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT,
                                         false, /*shift=*/true, false, false);
  }

  // Presses the right arrow key.
  void PressRightArrow() {
    content::SimulateKeyPressWithoutChar(
        GetWebContentsForInputRouting(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, false, false, false);
  }

  // Presses down shift, presses the right arrow, and lets go of shift.
  void PressShiftRightArrow() {
    content::SimulateKeyPressWithoutChar(
        GetWebContentsForInputRouting(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, /*shift=*/true, false,
        false);
  }

  // Checks the Linux selection clipboard by polling.
  void CheckSelectionClipboard(const std::string& expected) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    CheckClipboard(ui::ClipboardBuffer::kSelection, expected);
#endif
  }

  // Sends a copy command and checks the copy/paste clipboard by
  // polling. Note: Trying to send ctrl+c does not work correctly with
  // SimulateKeyPress(). Using IDC_COPY does not work on Mac in browser_tests.
  void SendCopyCommandAndCheckCopyPasteClipboard(const std::string& expected) {
    content::RunAllPendingInMessageLoop();
    GetWebContentsForInputRouting()->Copy();
    CheckClipboard(ui::ClipboardBuffer::kCopyPaste, expected);
  }

  content::WebContents* GetWebContentsForInputRouting() {
    return content::MimeHandlerViewMode::UsesCrossProcessFrame()
               ? guest_contents_
               : GetActiveWebContents();
  }

 private:
  // Waits and polls the clipboard of a given |clipboard_buffer| until its
  // contents reaches the length of |expected|. Then checks and see if the
  // clipboard contents matches |expected|.
  // TODO(thestig): Change this to avoid polling after https://crbug.com/755826
  // has been fixed.
  void CheckClipboard(ui::ClipboardBuffer clipboard_buffer,
                      const std::string& expected) {
    auto* clipboard = ui::Clipboard::GetForCurrentThread();
    std::string clipboard_data;
    const std::string& last_data = last_clipboard_data_[clipboard_buffer];
    if (last_data.size() == expected.size()) {
      DCHECK_EQ(last_data, expected);
      clipboard->ReadAsciiText(clipboard_buffer, &clipboard_data);
      EXPECT_EQ(expected, clipboard_data);
      return;
    }

    const bool expect_increase = last_data.size() < expected.size();
    while (true) {
      clipboard->ReadAsciiText(clipboard_buffer, &clipboard_data);
      if (expect_increase) {
        if (clipboard_data.size() >= expected.size())
          break;
      } else {
        if (clipboard_data.size() <= expected.size())
          break;
      }

      content::RunAllPendingInMessageLoop();
    }
    EXPECT_EQ(expected, clipboard_data);

    last_clipboard_data_[clipboard_buffer] = clipboard_data;
  }

  std::map<ui::ClipboardBuffer, std::string> last_clipboard_data_;
  WebContents* guest_contents_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       IndividualShiftRightArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  // Press shift + right arrow 3 times. Letting go of shift in between.
  PressShiftRightArrow();
  CheckSelectionClipboard("H");
  PressShiftRightArrow();
  CheckSelectionClipboard("HE");
  PressShiftRightArrow();
  CheckSelectionClipboard("HEL");
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");
}

// TODO: test is flaky. https://crbug.com/897801
IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       DISABLED_IndividualShiftLeftArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  for (int i = 0; i < 3; ++i)
    PressRightArrow();

  // Press shift + left arrow 2 times. Letting go of shift in between.
  PressShiftLeftArrow();
  CheckSelectionClipboard("L");
  PressShiftLeftArrow();
  CheckSelectionClipboard("EL");
  SendCopyCommandAndCheckCopyPasteClipboard("EL");

  // Press shift + left arrow 2 times. Letting go of shift in between.
  PressShiftLeftArrow();
  CheckSelectionClipboard("HEL");
  PressShiftLeftArrow();
  CheckSelectionClipboard("HEL");
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       CombinedShiftRightArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  // Press shift + right arrow 3 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        GetWebContentsForInputRouting(), false, true, false, false);
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                   ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    CheckSelectionClipboard("H");
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                   ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    CheckSelectionClipboard("HE");
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                   ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    CheckSelectionClipboard("HEL");
  }
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest, CombinedShiftArrowPresses) {
  LoadTestComboBoxPdfGetGuestContents();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox();

  TypeHello();

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox();

  for (int i = 0; i < 3; ++i)
    PressRightArrow();

  // Press shift + left arrow 3 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        GetWebContentsForInputRouting(), false, true, false, false);
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_LEFT,
                                   ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT);
    CheckSelectionClipboard("L");
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_LEFT,
                                   ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT);
    CheckSelectionClipboard("EL");
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_LEFT,
                                   ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT);
    CheckSelectionClipboard("HEL");
  }
  SendCopyCommandAndCheckCopyPasteClipboard("HEL");

  // Press shift + right arrow 2 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        GetWebContentsForInputRouting(), false, true, false, false);
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                   ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    CheckSelectionClipboard("EL");
    hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                   ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    CheckSelectionClipboard("L");
  }
  SendCopyCommandAndCheckCopyPasteClipboard("L");
}

// Verifies that an <embed> of size zero will still instantiate a guest and post
// message to the <embed> is correctly forwarded to the extension. This is for
// catching future regression in docs/ and slides/ pages (see
// https://crbug.com/763812).
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PostMessageForZeroSizedEmbed) {
  content::DOMMessageQueue queue;
  GURL url(embedded_test_server()->GetURL(
      "/pdf/post_message_zero_sized_embed.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  std::string message;
  EXPECT_TRUE(queue.WaitForMessage(&message));
  EXPECT_EQ("\"POST_MESSAGE_OK\"", message);
}

// In response to the events sent in |send_events|, ensures the PDF viewer zooms
// in and that the viewer's custom pinch zooming mechanism is used to do so.
void EnsureCustomPinchZoomInvoked(WebContents* guest_contents,
                                  WebContents* contents,
                                  base::OnceClosure send_events) {
  ASSERT_TRUE(content::ExecuteScript(
      guest_contents,
      "var gestureDetector = new GestureDetector(viewer.plugin_); "
      "var updatePromise = new Promise(function(resolve) { "
      "  gestureDetector.addEventListener('pinchupdate', resolve); "
      "});"));

  zoom::ZoomChangedWatcher zoom_watcher(
      contents,
      base::BindRepeating(
          [](const zoom::ZoomController::ZoomChangedEventData& event) {
            return event.new_zoom_level > event.old_zoom_level &&
                   event.zoom_mode == zoom::ZoomController::ZOOM_MODE_MANUAL &&
                   !event.can_show_bubble;
          }));

  std::move(send_events).Run();

  bool got_update;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      guest_contents,
      "updatePromise.then(function(update) { "
      "  window.domAutomationController.send(!!update); "
      "});",
      &got_update));
  EXPECT_TRUE(got_update);

  zoom_watcher.Wait();

  // Check that the browser's native pinch zoom was prevented.
  double scale_factor;
  ASSERT_TRUE(content::ExecuteScriptAndExtractDouble(
      contents,
      "window.domAutomationController.send(window.visualViewport.scale);",
      &scale_factor));
  EXPECT_DOUBLE_EQ(1.0, scale_factor);
}

// Ensure that touchpad pinch events are handled by the PDF viewer.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TouchpadPinchInvokesCustomZoom) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  base::OnceClosure send_pinch = base::BindOnce(
      [](WebContents* guest_contents) {
        const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
        const gfx::Point mouse_position(guest_rect.width() / 2,
                                        guest_rect.height() / 2);
        content::SimulateGesturePinchSequence(
            guest_contents, mouse_position, 1.23,
            blink::WebGestureDevice::kTouchpad);
      },
      guest_contents);

  EnsureCustomPinchZoomInvoked(guest_contents, GetActiveWebContents(),
                               std::move(send_pinch));
}

#if !defined(OS_MACOSX)
// Ensure that ctrl-wheel events are handled by the PDF viewer.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, CtrlWheelInvokesCustomZoom) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  base::OnceClosure send_ctrl_wheel = base::BindOnce(
      [](WebContents* guest_contents) {
        const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
        const gfx::Point mouse_position(guest_rect.width() / 2,
                                        guest_rect.height() / 2);
        content::SimulateMouseWheelCtrlZoomEvent(
            guest_contents, mouse_position, true,
            blink::WebMouseWheelEvent::kPhaseBegan);
      },
      guest_contents);

  EnsureCustomPinchZoomInvoked(guest_contents, GetActiveWebContents(),
                               std::move(send_ctrl_wheel));
}

// Flaky on ChromeOS (https://crbug.com/922974)
#if defined(OS_CHROMEOS)
#define MAYBE_TouchscreenPinchInvokesCustomZoom \
  DISABLED_TouchscreenPinchInvokesCustomZoom
#else
#define MAYBE_TouchscreenPinchInvokesCustomZoom \
  TouchscreenPinchInvokesCustomZoom
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionTest,
                       MAYBE_TouchscreenPinchInvokesCustomZoom) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  base::OnceClosure send_touchscreen_pinch = base::BindOnce(
      [](WebContents* guest_contents) {
        const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
        const gfx::PointF anchor_position(guest_rect.width() / 2,
                                          guest_rect.height() / 2);
        base::RunLoop run_loop;
        content::SimulateTouchscreenPinch(guest_contents, anchor_position, 1.2f,
                                          run_loop.QuitClosure());
        run_loop.Run();
      },
      guest_contents);

  EnsureCustomPinchZoomInvoked(guest_contents, GetActiveWebContents(),
                               std::move(send_touchscreen_pinch));
}

#endif  // !defined(OS_MACOSX)

// Flaky in nearly all configurations; see https://crbug.com/856169.
IN_PROC_BROWSER_TEST_F(PDFExtensionHitTestTest, DISABLED_MouseLeave) {
  GURL url = embedded_test_server()->GetURL("/pdf/pdf_embed.html");

  // Load page with embedded PDF and make sure it succeeds.
  ASSERT_TRUE(LoadPdf(url));
  WebContents* guest_contents = nullptr;
  WebContents* embedder_contents = GetActiveWebContents();
  content::BrowserPluginGuestManager* guest_manager =
      embedder_contents->GetBrowserContext()->GetGuestManager();
  ASSERT_NO_FATAL_FAILURE(guest_manager->ForEachGuest(
      embedder_contents, base::Bind(&GetGuestCallback, &guest_contents)));
  ASSERT_NE(nullptr, guest_contents);
  content::WaitForHitTestData(guest_contents);

  gfx::Point point_in_parent(250, 25);
  gfx::Point point_in_pdf(250, 250);

  // Inject script to count MouseLeaves in the PDF.
  ASSERT_TRUE(content::ExecuteScript(
      guest_contents,
      "var enter_count = 0;\n"
      "var leave_count = 0;\n"
      "document.addEventListener('mouseenter', function (){\n"
      "  enter_count++;"
      "});\n"
      "document.addEventListener('mouseleave', function (){\n"
      "  leave_count++;"
      "});"));

  // Inject some MouseMoves to invoke a MouseLeave in the PDF.
  content::SimulateRoutedMouseEvent(
      embedder_contents, blink::WebInputEvent::kMouseMove, point_in_parent);
  content::SimulateRoutedMouseEvent(
      embedder_contents, blink::WebInputEvent::kMouseMove, point_in_pdf);
  content::SimulateRoutedMouseEvent(
      embedder_contents, blink::WebInputEvent::kMouseMove, point_in_parent);

  // Verify MouseEnter, MouseLeave received.
  int leave_count = 0;
  do {
    ASSERT_TRUE(ExecuteScriptAndExtractInt(
        guest_contents, "window.domAutomationController.send(leave_count);",
        &leave_count));
  } while (!leave_count);
  int enter_count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      guest_contents, "window.domAutomationController.send(enter_count);",
      &enter_count));
  EXPECT_EQ(1, enter_count);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionHitTestTest, ContextMenuCoordinates) {
  GURL url = embedded_test_server()->GetURL("/pdf/pdf_embed.html");

  // Load page with embedded PDF and make sure it succeeds.
  ASSERT_TRUE(LoadPdf(url));
  WebContents* guest_contents = nullptr;
  WebContents* embedder_contents = GetActiveWebContents();
  content::BrowserPluginGuestManager* guest_manager =
      embedder_contents->GetBrowserContext()->GetGuestManager();
  ASSERT_NO_FATAL_FAILURE(guest_manager->ForEachGuest(
      embedder_contents, base::Bind(&GetGuestCallback, &guest_contents)));
  ASSERT_NE(nullptr, guest_contents);
  content::WaitForHitTestData(guest_contents);

  content::RenderProcessHost* guest_process_host =
      guest_contents->GetMainFrame()->GetProcess();

  // Get coords for mouse event.
  content::RenderWidgetHostView* guest_view =
      guest_contents->GetRenderWidgetHostView();
  gfx::Point local_context_menu_position(30, 80);
  gfx::Point root_context_menu_position =
      guest_view->TransformPointToRootCoordSpace(local_context_menu_position);

  auto context_menu_filter = base::MakeRefCounted<content::ContextMenuFilter>();
  guest_process_host->AddFilter(context_menu_filter.get());

  ContextMenuWaiter menu_observer;
  // Send mouse right-click to activate context menu.
  content::SimulateRoutedMouseClickAt(embedder_contents, kDefaultKeyModifier,
                                      blink::WebMouseEvent::Button::kRight,
                                      root_context_menu_position);

  // We expect the context menu, invoked via the RenderFrameHost, to be using
  // root view coordinates.
  menu_observer.WaitForMenuOpenAndClose();
  ASSERT_EQ(root_context_menu_position.x(), menu_observer.params().x);
  ASSERT_EQ(root_context_menu_position.y(), menu_observer.params().y);

  // We expect the IPC, received from the renderer, to be using local coords.
  context_menu_filter->Wait();
  content::ContextMenuParams params = context_menu_filter->get_params();
  EXPECT_EQ(local_context_menu_position.x(), params.x);
  EXPECT_EQ(local_context_menu_position.y(), params.y);

  // TODO(wjmaclean): If it ever becomes possible to filter outgoing IPCs
  // from the RenderProcessHost, we should verify the ViewMsg_PluginActionAt
  // message is sent with the same coordinates as in the ContextMenuParams.
}

// The plugin document and the mime handler should both use the same background
// color.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BackgroundColor) {
  if (content::MimeHandlerViewMode::UsesCrossProcessFrame()) {
    // The background color for plugins is injected when the first response
    // is intercepted, at which point not all the plugins have loaded. This line
    // ensures that the PDF plugin has loaded and the right background color is
    // beign used.
    WaitForPluginServiceToLoad();
  }
  WebContents* guest_contents =
      LoadPdfGetGuestContents(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_contents);
  const std::string script =
      "window.domAutomationController.send("
      "    window.getComputedStyle(document.body, null)."
      "    getPropertyValue('background-color'))";
  std::string outer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(GetActiveWebContents(),
                                                     script, &outer));
  std::string inner;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(guest_contents, script, &inner));
  EXPECT_EQ(inner, outer);
}

// Service worker tests are regression tests for
// https://crbug.com/916514.

// Test navigating to a PDF in the scope of a service worker with no fetch event
// handler.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ServiceWorkerNoFetchHandler) {
  RunServiceWorkerTest("empty.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// then falls back to network by not calling FetchEvent.respondWith().
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ServiceWorkerNetworkFallback) {
  RunServiceWorkerTest("network_fallback_worker.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// provides a response.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ServiceWorkerInterception) {
  RunServiceWorkerTest("respond_with_fetch_worker.js");
}

// Flaky on Windows. https://crbug.com/952066
#if defined(OS_WIN)
#define MAYBE_EmbeddedPdfGetsFocus DISABLED_EmbeddedPdfGetsFocus
#else
#define MAYBE_EmbeddedPdfGetsFocus EmbeddedPdfGetsFocus
#endif

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, MAYBE_EmbeddedPdfGetsFocus) {
  if (content::MimeHandlerViewMode::UsesCrossProcessFrame()) {
    // This test verifies focus for a BrowserPlugin and is not relevant with
    // MHVICPF since no BrowserPlugin is created with this flag.
    return;
  }
  GURL test_iframe_url(embedded_test_server()->GetURL(
      "/pdf/test-offset-cross-site-iframe.html"));
  ui_test_utils::NavigateToURL(browser(), test_iframe_url);
  WebContents* contents = GetActiveWebContents();

  // Get BrowserPluginGuest for the PDF.
  WebContents* guest_contents = nullptr;
  content::BrowserPluginGuestManager* guest_manager =
      contents->GetBrowserContext()->GetGuestManager();
  guest_manager->ForEachGuest(
      contents, base::BindRepeating(&RetrieveGuestContents, &guest_contents));
  ASSERT_TRUE(guest_contents);
  EXPECT_NE(contents, guest_contents);
  // Wait for the guest's view to be created.
  while (!guest_contents->GetRenderWidgetHostView()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  WaitForHitTestData(guest_contents);

  // Verify it's not focused.
  EXPECT_FALSE(IsWebContentsBrowserPluginFocused(guest_contents));

  // Send mouse-click.
  gfx::Point point_in_pdf(10, 10);
  gfx::Point point_in_root =
      guest_contents->GetRenderWidgetHostView()->TransformPointToRootCoordSpace(
          point_in_pdf);
  EXPECT_NE(point_in_pdf, point_in_root);
  content::SimulateRoutedMouseClickAt(contents, kDefaultKeyModifier,
                                      blink::WebMouseEvent::Button::kLeft,
                                      point_in_root);

  // Wait for the BPG to get focus. This test will timeout if the focus fails
  // to occur. Alternatively, we could add an IPC filter to the guest's
  // RenderProcessHost.
  while (!IsWebContentsBrowserPluginFocused(guest_contents)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

// A helper for waiting for the first request for |url_to_intercept|.
class RequestWaiter {
 public:
  // Start intercepting requests to |url_to_intercept|.
  explicit RequestWaiter(const GURL& url_to_intercept)
      : url_to_intercept_(url_to_intercept),
        interceptor_(base::BindRepeating(&RequestWaiter::InterceptorCallback,
                                         base::Unretained(this))) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(url_to_intercept.is_valid());
  }

  void WaitForRequest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!IsAlreadyIntercepted())
      run_loop_.Run();
    DCHECK(IsAlreadyIntercepted());
  }

 private:
  bool InterceptorCallback(
      content::URLLoaderInterceptor::RequestParams* params) {
    // This method may be called either on the IO or UI thread.
    DCHECK(params);

    base::AutoLock lock(lock_);
    if (url_to_intercept_ != params->url_request.url || already_intercepted_)
      return false;

    already_intercepted_ = true;
    run_loop_.Quit();
    return false;
  }

  bool IsAlreadyIntercepted() {
    base::AutoLock lock(lock_);
    return already_intercepted_;
  }

  const GURL url_to_intercept_;
  content::URLLoaderInterceptor interceptor_;
  base::RunLoop run_loop_;

  base::Lock lock_;
  bool already_intercepted_ GUARDED_BY(lock_) = false;

  DISALLOW_COPY_AND_ASSIGN(RequestWaiter);
};

// This is a regression test for a problem where DidStopLoading didn't get
// propagated from a remote frame into the main frame.  See also
// https://crbug.com/964364.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DidStopLoading) {
  // Prepare to wait for requests for the main page of the MimeHandlerView for
  // PDFs.
  RequestWaiter interceptor(
      GURL("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html"));

  // Navigate to a page with:
  //   <embed type="application/pdf" src="test.pdf"></embed>
  //   <iframe src="/hung"></iframe>
  // Afterwards, the main page should be still loading because of the hung
  // subframe (but the subframe for the OOPIF-based PDF MimeHandlerView might or
  // might not be created at this point).
  GURL url = embedded_test_server()->GetURL(
      "/pdf/pdf_embed_with_hung_sibling_subframe.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);  // Don't wait for completion.

  // Wait for the request for the MimeHandlerView extension.  Afterwards, the
  // main page should be still loading because of
  // 1) the MimeHandlerView frame is loading
  // 2) the hung subframe is loading.
  interceptor.WaitForRequest();

  // Remove the hung subframe.  Afterwards the main page should stop loading as
  // soon as the MimeHandlerView frame stops loading (assumming we have not bugs
  // similar to https://crbug.com/964364).
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents, "document.getElementById('hung_subframe').remove();"));

  // MAIN VERIFICATION: Wait for the main frame to report that is has stopped
  // loading.
  content::WaitForLoadStop(web_contents);
}

// This test suite does a simple text-extraction based on the accessibility
// internals, breaking lines & paragraphs where appropriate.  Unlike
// TreeDumpTests, this allows us to verify the kNextOnLine and kPreviousOnLine
// relationships.
class PDFExtensionAccessibilityTextExtractionTest : public PDFExtensionTest {
 public:
  PDFExtensionAccessibilityTextExtractionTest() = default;
  ~PDFExtensionAccessibilityTextExtractionTest() override = default;

  void RunTextExtractionTest(const base::FilePath::CharType* pdf_file) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath pdf_path = test_path.Append(pdf_file);

    RunTest(pdf_path, "pdf/accessibility");
  }

 private:
  class TestExpectationsLocator
      : public content::AccessibilityTestExpectationsLocator {
   public:
    TestExpectationsLocator() = default;
    ~TestExpectationsLocator() override = default;

    base::FilePath::StringType GetExpectedFileSuffix() override {
      return FILE_PATH_LITERAL("-expected.txt");
    }
    base::FilePath::StringType GetVersionSpecificExpectedFileSuffix() override {
      return FILE_PATH_LITERAL("");
    }
  };

  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    // Load the expectation file.
    TestExpectationsLocator locator;
    content::DumpAccessibilityTestHelper test_helper(&locator);
    base::Optional<base::FilePath> expected_file_path =
        test_helper.GetExpectationFilePath(test_file_path);
    ASSERT_TRUE(expected_file_path) << "No expectation file present.";

    base::Optional<std::vector<std::string>> expected_lines =
        test_helper.LoadExpectationFile(*expected_file_path);
    ASSERT_TRUE(expected_lines) << "Couldn't load expectation file.";

    // Enable accessibility and load the test file.
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
    GURL test_pdf_url(embedded_test_server()->GetURL(
        "/" + std::string(file_dir) + "/" +
        test_file_path.BaseName().MaybeAsASCII()));
    WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
    ASSERT_TRUE(guest_contents);
    WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");

    // Extract the text content.
    ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
    auto actual_lines = CollectLines(ax_tree);

    // Validate the dump against the expectation file.
    EXPECT_TRUE(test_helper.ValidateAgainstExpectation(
        test_file_path, *expected_file_path, actual_lines, *expected_lines));
  }

 private:
  std::vector<std::string> CollectLines(ui::AXTreeUpdate ax_tree) {
    std::vector<std::string> lines;

    int previous_node_id = 0;
    int previous_node_next_id = 0;
    std::string line;
    bool found_embedded_object = false;
    for (const auto& node : ax_tree.nodes) {
      // Ignore everything before the embedded object (the root of the PDF).
      if (node.role == ax::mojom::Role::kEmbeddedObject)
        found_embedded_object = true;
      if (!found_embedded_object)
        continue;

      // StaticText begins a new paragraph.
      if (node.role == ax::mojom::Role::kStaticText && !line.empty()) {
        lines.push_back(line);
        lines.push_back("\u00b6");  // pilcrow/paragraph mark, Alt+0182
        line.clear();
      }

      // We collect all inline text boxes within the paragraph.
      if (node.role != ax::mojom::Role::kInlineTextBox)
        continue;

      std::string name =
          node.GetStringAttribute(ax::mojom::StringAttribute::kName);
      base::StringPiece trimmed_name =
          base::TrimString(name, "\r\n", base::TRIM_TRAILING);
      int prev_id =
          node.GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId);
      if (previous_node_next_id == node.id) {
        // Previous node pointed to us, so we are part of the same line.
        EXPECT_EQ(previous_node_id, prev_id)
            << "Expect this node to point to previous node.";
        trimmed_name.AppendToString(&line);
      } else {
        // Not linked with the previous node; this is a new line.
        EXPECT_EQ(previous_node_next_id, 0)
            << "Previous node pointed to something unexpected.";
        EXPECT_EQ(prev_id, 0)
            << "Our back pointer points to something unexpected.";
        if (!line.empty())
          lines.push_back(line);
        line = trimmed_name.as_string();
      }

      previous_node_id = node.id;
      previous_node_next_id =
          node.GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId);
    }
    if (!line.empty())
      lines.push_back(line);
    return lines;
  }
};

// Test that Previous/NextOnLineId attributes are present and properly linked on
// InlineTextBoxes within a line.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       NextOnLine) {
  RunTextExtractionTest(FILE_PATH_LITERAL("next-on-line.pdf"));
}

// Test that a drop-cap is grouped with the correct line.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest, DropCap) {
  RunTextExtractionTest(FILE_PATH_LITERAL("drop-cap.pdf"));
}

// Test that simulated superscripts and subscripts don't cause a line break.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       SuperscriptSubscript) {
  RunTextExtractionTest(FILE_PATH_LITERAL("superscript-subscript.pdf"));
}

// Test that simple font and font-size changes in the middle of a line don't
// cause line breaks.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       FontChange) {
  RunTextExtractionTest(FILE_PATH_LITERAL("font-change.pdf"));
}

// Test one property of pdf_private/accessibility_crash_2.pdf, where a page has
// only whitespace characters.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       OnlyWhitespaceText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("whitespace.pdf"));
}

// Test data of inline text boxes for PDF with weblinks.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest, WebLinks) {
  RunTextExtractionTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

// Test data of inline text boxes for PDF with multi-line and various font-sized
// text.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       ParagraphsAndHeadingUntagged) {
  RunTextExtractionTest(
      FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

// Test data of inline text boxes for PDF with text, weblinks, images and
// annotation links.
IN_PROC_BROWSER_TEST_F(PDFExtensionAccessibilityTextExtractionTest,
                       LinksImagesAndText) {
  RunTextExtractionTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

class PDFExtensionAccessibilityTreeDumpTest
    : public PDFExtensionTest,
      public ::testing::WithParamInterface<size_t> {
 public:
  PDFExtensionAccessibilityTreeDumpTest()
      : test_pass_(
            content::AccessibilityTreeFormatter::GetTestPass(GetParam())) {}
  ~PDFExtensionAccessibilityTreeDumpTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);

    // Each test pass might require custom command-line setup
    if (test_pass_.set_up_command_line)
      test_pass_.set_up_command_line(command_line);
  }

 protected:
  void RunPDFTest(const base::FilePath::CharType* pdf_file) {
    base::FilePath test_path = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("pdf")),
        base::FilePath(FILE_PATH_LITERAL("accessibility")));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath pdf_path = test_path.Append(pdf_file);

    RunTest(pdf_path, "pdf/accessibility");
  }

 private:
  using PropertyFilter = content::AccessibilityTreeFormatter::PropertyFilter;

  //  See chrome/test/data/pdf/accessibility/readme.md for more info.
  void ParsePdfForExtraDirectives(
      const std::string& pdf_contents,
      content::AccessibilityTreeFormatter* formatter,
      std::vector<PropertyFilter>* property_filters) {
    const char kCommentMark = '%';
    for (const std::string& line : base::SplitString(
             pdf_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (line.size() > 1 && line[0] == kCommentMark) {
        // Remove first character since it's the comment mark.
        std::string trimmed_line = line.substr(1);
        const std::string& allow_str = formatter->GetAllowString();
        if (base::StartsWith(trimmed_line, allow_str,
                             base::CompareCase::SENSITIVE)) {
          property_filters->push_back(PropertyFilter(
              base::UTF8ToUTF16(trimmed_line.substr(allow_str.size())),
              PropertyFilter::ALLOW));
        }
      }
    }
  }

  void RunTest(const base::FilePath& test_file_path, const char* file_dir) {
    std::string pdf_contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(test_file_path, &pdf_contents));
    }

    // Set up the tree formatter. Parse filters and other directives in the test
    // file.
    std::unique_ptr<content::AccessibilityTreeFormatter> formatter =
        test_pass_.create_formatter();
    std::vector<PropertyFilter> property_filters;
    formatter->AddDefaultFilters(&property_filters);
    AddDefaultFilters(&property_filters);
    ParsePdfForExtraDirectives(pdf_contents, formatter.get(),
                               &property_filters);
    formatter->SetPropertyFilters(property_filters);

    // Exit without running the test if we can't find an expectation file or if
    // the expectation file contains a skip marker.
    // This is used to skip certain tests on certain platforms.
    content::DumpAccessibilityTestHelper test_helper(formatter.get());
    base::FilePath expected_file_path =
        test_helper.GetExpectationFilePath(test_file_path);
    if (expected_file_path.empty()) {
      LOG(INFO) << "No expectation file present, ignoring test on this "
                   "platform.";
      return;
    }

    base::Optional<std::vector<std::string>> expected_lines =
        test_helper.LoadExpectationFile(expected_file_path);
    if (!expected_lines) {
      LOG(INFO) << "Skipping this test on this platform.";
      return;
    }

    // Enable accessibility and load the test file.
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
    GURL test_pdf_url(embedded_test_server()->GetURL(
        "/" + std::string(file_dir) + "/" +
        test_file_path.BaseName().MaybeAsASCII()));
    WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
    ASSERT_TRUE(guest_contents);
    WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");

    // Find the embedded PDF and dump the accessibility tree.
    content::FindAccessibilityNodeCriteria find_criteria;
    find_criteria.role = ax::mojom::Role::kEmbeddedObject;
    content::BrowserAccessibility* pdf_root =
        content::FindAccessibilityNode(guest_contents, find_criteria);
    CHECK(pdf_root);

    base::string16 actual_contents_utf16;
    formatter->FormatAccessibilityTree(pdf_root, &actual_contents_utf16);
    std::string actual_contents = base::UTF16ToUTF8(actual_contents_utf16);

    std::vector<std::string> actual_lines =
        base::SplitString(actual_contents, "\n", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // Validate the dump against the expectation file.
    EXPECT_TRUE(test_helper.ValidateAgainstExpectation(
        test_file_path, expected_file_path, actual_lines, *expected_lines));
  }

  void AddDefaultFilters(std::vector<PropertyFilter>* property_filters) {
    AddPropertyFilter(property_filters, "value='*'");
    // The value attribute on the document object contains the URL of the
    // current page which will not be the same every time the test is run.
    // The PDF plugin uses the 'chrome-extension' protocol, so block that as
    // well.
    AddPropertyFilter(property_filters, "value='http*'", PropertyFilter::DENY);
    AddPropertyFilter(property_filters, "value='chrome-extension*'",
                      PropertyFilter::DENY);
    // Object attributes.value
    AddPropertyFilter(property_filters, "layout-guess:*",
                      PropertyFilter::ALLOW);

    AddPropertyFilter(property_filters, "select*");
    AddPropertyFilter(property_filters, "descript*");
    AddPropertyFilter(property_filters, "check*");
    AddPropertyFilter(property_filters, "horizontal");
    AddPropertyFilter(property_filters, "multiselectable");
    AddPropertyFilter(property_filters, "isPageBreakingObject*");

    // Deny most empty values
    AddPropertyFilter(property_filters, "*=''", PropertyFilter::DENY);
    // After denying empty values, because we want to allow name=''
    AddPropertyFilter(property_filters, "name=*", PropertyFilter::ALLOW_EMPTY);
  }

  void AddPropertyFilter(std::vector<PropertyFilter>* property_filters,
                         std::string filter,
                         PropertyFilter::Type type = PropertyFilter::ALLOW) {
    property_filters->push_back(
        PropertyFilter(base::ASCIIToUTF16(filter), type));
  }

  content::AccessibilityTreeFormatter::TestPass test_pass_;
};

// Parameterize the tests so that each test-pass is run independently.
struct DumpAccessibilityTreeTestPassToString {
  std::string operator()(const ::testing::TestParamInfo<size_t>& i) const {
    return content::AccessibilityTreeFormatter::GetTestPass(i.param).name;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PDFExtensionAccessibilityTreeDumpTest,
    ::testing::Range(
        size_t{0},
        content::AccessibilityTreeFormatter::GetTestPasses().size()),
    DumpAccessibilityTreeTestPassToString());

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, HelloWorld) {
  RunPDFTest(FILE_PATH_LITERAL("hello-world.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       ParagraphsAndHeadingUntagged) {
  RunPDFTest(FILE_PATH_LITERAL("paragraphs-and-heading-untagged.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, MultiPage) {
  RunPDFTest(FILE_PATH_LITERAL("multi-page.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       DirectionalTextRuns) {
  RunPDFTest(FILE_PATH_LITERAL("directional-text-runs.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextDirection) {
  RunPDFTest(FILE_PATH_LITERAL("text-direction.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, WebLinks) {
  RunPDFTest(FILE_PATH_LITERAL("weblinks.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, Images) {
  RunPDFTest(FILE_PATH_LITERAL("image_alt_text.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       LinksImagesAndText) {
  RunPDFTest(FILE_PATH_LITERAL("text-image-link.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest,
                       TextRunStyleHeuristic) {
  RunPDFTest(FILE_PATH_LITERAL("text-run-style-heuristic.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionAccessibilityTreeDumpTest, TextStyle) {
  RunPDFTest(FILE_PATH_LITERAL("text-style.pdf"));
}
