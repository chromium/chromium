// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/download_item.h"
#include "components/viz/common/features.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/test_clipboard.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#endif

using content::WebContents;

const int kNumberLoadTestParts = 10;

#if defined(OS_MACOSX)
const int kDefaultKeyModifier = blink::WebInputEvent::kMetaKey;
#else
const int kDefaultKeyModifier = blink::WebInputEvent::kControlKey;
#endif

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
  // at chrome/test/data/pdf/<pdf_filename>.
  void RunTestsInFile(const std::string& filename,
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
    std::string test_util_js;
    std::string mock_interactions_js;

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FilePath test_data_dir;
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
      test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("pdf"));
      base::FilePath test_util_path = test_data_dir.AppendASCII("test_util.js");
      ASSERT_TRUE(base::ReadFileToString(test_util_path, &test_util_js));

      base::FilePath source_root_dir;
      base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
      base::FilePath mock_interactions_path = source_root_dir.Append(
          FILE_PATH_LITERAL("third_party/polymer/v1_0/components-chromium/"
                            "iron-test-helpers/mock-interactions.js"));
      ASSERT_TRUE(base::ReadFileToString(mock_interactions_path,
                                         &mock_interactions_js));
      test_util_js.append(mock_interactions_js);

      base::FilePath test_file_path = test_data_dir.AppendASCII(filename);
      std::string test_js;
      ASSERT_TRUE(base::ReadFileToString(test_file_path, &test_js));

      test_util_js.append(test_js);
    }

    ASSERT_TRUE(content::ExecuteScript(guest_contents, test_util_js));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  // Load the PDF at the given URL and use the PDFScriptingAPI to ensure it has
  // finished loading. Return true if it loads successfully or false if it
  // fails. If it doesn't finish loading the test will hang. This is done from
  // outside of the BrowserPlugin guest to ensure the PDFScriptingAPI works
  // correctly from there.
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
        if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
          // This file has races in loading correctly or not.
          if (pdf_file == "pdf_private/cfuzz5.pdf")
            continue;
        }
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
        "  oldSendScriptingMessage.bind(this)(message);"
        "  if (message.type == 'getSelectedTextReply')"
        "    this.parentWindow_.postMessage('flush', '*');"
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
    ASSERT_TRUE(content::ExecuteScript(contents,
        "var visiblePage = viewer.viewport.getMostVisiblePage();"
        "var visiblePageDimensions ="
        "    viewer.viewport.getPageScreenRect(visiblePage);"
        "var viewportPosition = viewer.viewport.position;"
        "var screenOffsetX = visiblePageDimensions.x - viewportPosition.x;"
        "var screenOffsetY = visiblePageDimensions.y - viewportPosition.y;"
        "var linkScreenPositionX ="
        "    Math.floor(" + base::IntToString(point->x()) + " + screenOffsetX);"
        "var linkScreenPositionY ="
        "    Math.floor(" + base::IntToString(point->y()) + " +"
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
    base::PostTaskWithTraitsAndReply(
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
};

class PDFExtensionLoadTest : public PDFExtensionTest,
                             public testing::WithParamInterface<int> {
 public:
  PDFExtensionLoadTest() {}
};

class PDFExtensionHitTestTest : public PDFExtensionTest,
                                public testing::WithParamInterface<bool> {
 public:
  PDFExtensionHitTestTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kEnableVizHitTestDrawQuad);
    } else {
      feature_list_.InitAndDisableFeature(features::kEnableVizHitTestDrawQuad);
    }
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
#if defined(GOOGLE_CHROME_BUILD)
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
  content::NotificationRegistrar registrar_;
  base::RunLoop download_run_loop_;
  GURL last_url_;
};

// Tests behavior when the PDF plugin is disabled in preferences.
class PDFPluginDisabledTest : public PDFExtensionTest {
 public:
  PDFPluginDisabledTest() {}

 protected:
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

 private:
  std::unique_ptr<DownloadAwaiter> download_awaiter_;
};

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest, DirectNavigationToPDF) {
  // Navigate to a PDF and test that it is downloaded.
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), pdf_url);

  // Validate that we downloaded a single PDF and didn't launch the PDF plugin.
  EXPECT_EQ(pdf_url, AwaitAndGetLastDownloadedUrl());
  EXPECT_EQ(1u, GetNumberOfDownloads());
  EXPECT_EQ(0, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest, IframePdfPlaceholderWithCSP) {
  // Navigate to a page that uses <iframe> to embed a PDF as a plugin.
  GURL iframe_page_url =
      embedded_test_server()->GetURL("/pdf/pdf_iframe_csp.html");
  ui_test_utils::NavigateToURL(browser(), iframe_page_url);

  // Pass an Enter keystroke to the child <iframe>.
  int keys_passed = 0;
  for (auto* host : GetActiveWebContents()->GetAllFrames()) {
    if (host != GetActiveWebContents()->GetMainFrame()) {
      content::NativeWebKeyboardEvent key_event(
          blink::WebKeyboardEvent::kChar, blink::WebInputEvent::kNoModifiers,
          blink::WebInputEvent::GetStaticTimeStampForTests());
      key_event.windows_key_code = ui::VKEY_RETURN;
      key_event.native_key_code =
          ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ENTER);
      key_event.dom_code = static_cast<int>(ui::DomCode::ENTER);
      key_event.dom_key = ui::DomKey::ENTER;
      host->GetView()->GetRenderWidgetHost()->ForwardKeyboardEvent(key_event);
      keys_passed++;
    }
  }
  ASSERT_EQ(1, keys_passed);

  // Validate that we downloaded a single PDF and didn't launch the PDF plugin.
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  EXPECT_EQ(pdf_url, AwaitAndGetLastDownloadedUrl());
  EXPECT_EQ(1u, GetNumberOfDownloads());
  EXPECT_EQ(0, CountPDFProcesses());
}

// We break PDFExtensionLoadTest up into kNumberLoadTestParts.
INSTANTIATE_TEST_CASE_P(PDFTestFiles,
                        PDFExtensionLoadTest,
                        testing::Range(0, kNumberLoadTestParts));

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        PDFExtensionHitTestTest,
                        testing::Bool());

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Basic) {
  RunTestsInFile("basic_test.js", "test.pdf");

  // Ensure it loaded in a PPAPI process.
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BasicPlugin) {
  RunTestsInFile("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Viewport) {
  RunTestsInFile("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Bookmark) {
  RunTestsInFile("bookmarks_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Navigator) {
  RunTestsInFile("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ParamsParser) {
  RunTestsInFile("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ZoomManager) {
  RunTestsInFile("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, GestureDetector) {
  RunTestsInFile("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TouchHandling) {
  RunTestsInFile("touch_handling_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInFile("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ToolbarManager) {
  RunTestsInFile("toolbar_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Title) {
  RunTestsInFile("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, WhitespaceTitle) {
  RunTestsInFile("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Beep) {
  RunTestsInFile("beep_test.js", "test-beep.pdf");
}

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
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(), CONTENT_SETTING_BLOCK);

  RunTestsInFile("nobeep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PageChange) {
  RunTestsInFile("page_change_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Metrics) {
  RunTestsInFile("metrics_test.js", "test.pdf");
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

// This test ensures that link permissions are enforced properly in PDFs.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkPermissions) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // chrome://favicon links should be allowed for PDFs, while chrome://settings
  // links should not.
  GURL valid_link_url("chrome://favicon/https://www.google.ca/");
  GURL invalid_link_url("chrome://settings");

  GURL unfiltered_valid_link_url(valid_link_url);
  content::RenderProcessHost* rph =
      guest_contents->GetMainFrame()->GetProcess();
  rph->FilterURL(true, &valid_link_url);
  rph->FilterURL(true, &invalid_link_url);

  // Invalid link URLs should be changed to "about:blank" when filtered.
  EXPECT_EQ(unfiltered_valid_link_url, valid_link_url);
  EXPECT_EQ(GURL("about:blank"), invalid_link_url);
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
    "  group\n"
    "    region 'Page 1'\n"
    "      paragraph\n"
    "        staticText '1 First Section'\n"
    "          inlineTextBox '1 '\n"
    "          inlineTextBox 'First Section'\n"
    "      paragraph\n"
    "        staticText 'This is the *rst section.1'\n"
    "          inlineTextBox 'This is the *rst section.'\n"
    "          inlineTextBox '1'\n"
    "    region 'Page 2'\n"
    "      paragraph\n"
    "        staticText '1.1 First Subsection'\n"
    "          inlineTextBox '1.1 '\n"
    "          inlineTextBox 'First Subsection'\n"
    "      paragraph\n"
    "        staticText 'This is the *rst subsection.2'\n"
    "          inlineTextBox 'This is the *rst subsection.'\n"
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
  CHECK(content::ExecuteScript(web_contents,
                               "var scriptingAPI = new PDFScriptingAPI(window, "
                               "    document.getElementsByTagName('embed')[0]);"
                               "scriptingAPI.selectAll();"));

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

#if defined(GOOGLE_CHROME_BUILD)
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
  RunTestsInFile("redirects_fail_test.js", "test.pdf");
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
      "    'www.example.com', Navigator.WindowOpenDisposition.CURRENT_TAB);"));
  navigation_observer.Wait();

  EXPECT_FALSE(navigation_observer.last_navigation_url().is_empty());
  EXPECT_TRUE(active_navigation_observer.last_navigation_url().is_empty());
  EXPECT_FALSE(active_web_contents->GetController().GetPendingEntry());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, OpenFromFTP) {
  net::SpawnedTestServer ftp_server(
      net::SpawnedTestServer::TYPE_FTP,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data/pdf")));
  ASSERT_TRUE(ftp_server.Start());

  GURL url(ftp_server.GetURL("/test.pdf"));
  ASSERT_TRUE(LoadPdf(url));
  EXPECT_EQ(base::ASCIIToUTF16("test.pdf"), GetActiveWebContents()->GetTitle());
}

// For both PDFExtensionTest and PDFIsolatedExtensionTest, MultipleDomains case
// is flaky.
// https://crbug.com/825038
// https://crbug.com/851805
#define MAYBE_MultipleDomains DISABLED_MultipleDomains

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, MAYBE_MultipleDomains) {
  for (const auto& url :
       {embedded_test_server()->GetURL("a.com", "/pdf/test.pdf"),
        embedded_test_server()->GetURL("b.com", "/pdf/test.pdf"),
        embedded_test_server()->GetURL("c.com", "/pdf/test.pdf"),
        embedded_test_server()->GetURL("d.com", "/pdf/test.pdf")}) {
    ASSERT_TRUE(LoadPdfInNewTab(url));
  }
  EXPECT_EQ(1, CountPDFProcesses());
}

class PDFIsolatedExtensionTest : public PDFExtensionTest {
 public:
  PDFIsolatedExtensionTest() {}
  ~PDFIsolatedExtensionTest() override {}

  void SetUp() override {
    features_.InitAndEnableFeature(features::kPdfIsolation);
    PDFExtensionTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

// See MAYBE_MultipleDomains definition, above.
// https://crbug.com/825038 and https://crbug.com/851805
IN_PROC_BROWSER_TEST_F(PDFIsolatedExtensionTest, MAYBE_MultipleDomains) {
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

 private:
  WebContents* guest_contents_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, CtrlLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, modifiers,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, blink::WebInputEvent::kShiftKey,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, blink::WebInputEvent::kShiftKey,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  observer.Wait();

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

 private:
  WebContents* guest_contents_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, CtrlLeft) {
  LoadTestLinkPdfGetGuestContents();

  WebContents* web_contents = GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kMiddle,
                                GetLinkPosition());
  observer.Wait();

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

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, blink::WebInputEvent::kShiftKey,
                                blink::WebMouseEvent::Button::kLeft,
                                GetLinkPosition());
  observer.Wait();

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
    content::SimulateMouseClickAt(GetActiveWebContents(), 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  GetEditableComboBoxLeftPosition());
  }

  void TypeHello() {
    auto* web_contents = GetActiveWebContents();
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
        GetActiveWebContents(), ui::DomKey::ARROW_LEFT, ui::DomCode::ARROW_LEFT,
        ui::VKEY_LEFT, false, false, false, false);
  }

  // Presses down shift, presses the left arrow, and lets go of shift.
  void PressShiftLeftArrow() {
    content::SimulateKeyPressWithoutChar(
        GetActiveWebContents(), ui::DomKey::ARROW_LEFT, ui::DomCode::ARROW_LEFT,
        ui::VKEY_LEFT, false, /*shift=*/true, false, false);
  }

  // Presses the right arrow key.
  void PressRightArrow() {
    content::SimulateKeyPressWithoutChar(
        GetActiveWebContents(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, false, false, false);
  }

  // Presses down shift, presses the right arrow, and lets go of shift.
  void PressShiftRightArrow() {
    content::SimulateKeyPressWithoutChar(
        GetActiveWebContents(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, /*shift=*/true, false,
        false);
  }

  // Checks the Linux selection clipboard by polling.
  void CheckSelectionClipboard(const std::string& expected) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    CheckClipboard(ui::CLIPBOARD_TYPE_SELECTION, expected);
#endif
  }

  // Sends a copy command and checks the copy/paste clipboard by
  // polling. Note: Trying to send ctrl+c does not work correctly with
  // SimulateKeyPress(). Using IDC_COPY does not work on Mac in browser_tests.
  void SendCopyCommandAndCheckCopyPasteClipboard(const std::string& expected) {
    content::RunAllPendingInMessageLoop();
    GetActiveWebContents()->Copy();
    CheckClipboard(ui::CLIPBOARD_TYPE_COPY_PASTE, expected);
  }

 private:
  // Waits and polls the clipboard of a given |clipboard_type| until its
  // contents reaches the length of |expected|. Then checks and see if the
  // clipboard contents matches |expected|.
  // TODO(thestig): Change this to avoid polling after https://crbug.com/755826
  // has been fixed.
  void CheckClipboard(ui::ClipboardType clipboard_type,
                      const std::string& expected) {
    auto* clipboard = ui::Clipboard::GetForCurrentThread();
    std::string clipboard_data;
    const std::string& last_data = last_clipboard_data_[clipboard_type];
    if (last_data.size() == expected.size()) {
      DCHECK_EQ(last_data, expected);
      clipboard->ReadAsciiText(clipboard_type, &clipboard_data);
      EXPECT_EQ(expected, clipboard_data);
      return;
    }

    const bool expect_increase = last_data.size() < expected.size();
    while (true) {
      clipboard->ReadAsciiText(clipboard_type, &clipboard_data);
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

    last_clipboard_data_[clipboard_type] = clipboard_data;
  }

  std::map<ui::ClipboardType, std::string> last_clipboard_data_;
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
        GetActiveWebContents(), false, true, false, false);
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
        GetActiveWebContents(), false, true, false, false);
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
        GetActiveWebContents(), false, true, false, false);
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
        content::SimulateGesturePinchSequence(guest_contents, mouse_position,
                                              1.23,
                                              blink::kWebGestureDeviceTouchpad);
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
#endif  // !defined(OS_MACOSX)

#if (defined(OS_WIN) && defined(ADDRESS_SANITIZER)) || \
    (defined(OS_CHROME) && defined(MEMORY_SANITIZER))
// https://crbug.com/856169, https://crbug.com/892484
#define MAYBE_MouseLeave DISABLED_MouseLeave
#else
#define MAYBE_MouseLeave MouseLeave
#endif
IN_PROC_BROWSER_TEST_P(PDFExtensionHitTestTest, MAYBE_MouseLeave) {
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
  content::WaitForHitTestDataOrGuestSurfaceReady(guest_contents);

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

IN_PROC_BROWSER_TEST_P(PDFExtensionHitTestTest, ContextMenuCoordinates) {
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
  content::WaitForHitTestDataOrGuestSurfaceReady(guest_contents);

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
