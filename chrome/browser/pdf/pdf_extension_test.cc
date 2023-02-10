// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_timeouts.h"
#include "base/test/with_feature_override.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/input/scroll_utils.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/plugins/plugin_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#endif

namespace {

using ::content::WebContents;
using ::extensions::MimeHandlerViewGuest;
using ::extensions::TestMimeHandlerViewGuest;
using ::guest_view::GuestViewManager;
using ::guest_view::TestGuestViewManager;
using ::guest_view::TestGuestViewManagerFactory;
using ::pdf_extension_test_util::ConvertPageCoordToScreenCoord;
using ::pdf_extension_test_util::GetPdfPluginFrames;
using ::pdf_extension_test_util::SetInputFocusOnPlugin;
using ::testing::IsEmpty;
using ::testing::StartsWith;

const int kNumberLoadTestParts = 10;

#if BUILDFLAG(IS_MAC)
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

}  // namespace

using PDFExtensionTest = PDFExtensionTestBase;

class PDFExtensionTestWithPartialLoading : public PDFExtensionTest {
 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionTest::GetEnabledFeatures();
    enabled.push_back(chrome_pdf::features::kPdfIncrementalLoading);
    enabled.push_back(chrome_pdf::features::kPdfPartialLoading);
    return enabled;
  }
};

// This test is a re-implementation of
// WebPluginContainerTest.PluginDocumentPluginIsFocused, which was introduced
// for https://crbug.com/536637. The original implementation checked that the
// BrowserPlugin hosting the pdf extension was focused; in this re-write, we
// make sure the guest view's WebContents has focus.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfInMainFrameHasFocus) {
  // Load test HTML, and verify the text area has focus.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  auto* primary_main_frame = GetActiveWebContents()->GetPrimaryMainFrame();

  // Verify the pdf has loaded.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  ASSERT_NE(primary_main_frame, guest_view->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Make sure the guest WebContents has focus.
  ASSERT_EQ(GetActiveWebContents()->GetFocusedFrame(),
            guest_view->GetGuestMainFrame());
}

// This test verifies that when a PDF is loaded, that (i) the embedder
// WebContents' html consists of a single <embed> tag with appropriate
// properties, and (ii) that the guest WebContents finishes loading and
// has the correct URL for the PDF extension.
// TODO(wjmaclean): Are there any attributes we can/should test with respect to
// the extension's loaded html?
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfExtensionLoadedInGuest) {
  // Load test HTML, and verify the text area has focus.
  const GURL main_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* primary_main_frame = GetActiveWebContents()->GetPrimaryMainFrame();

  // Verify the pdf has loaded.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  EXPECT_NE(primary_main_frame, guest_view->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Verify we loaded the extension.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url,
            guest_view->GetGuestMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(main_url, primary_main_frame->GetLastCommittedURL());

  // Make sure the embedder has the correct html boilerplate.
  EXPECT_EQ(
      1, content::EvalJs(primary_main_frame, "document.body.children.length;")
             .ExtractInt());
  EXPECT_EQ("EMBED", content::EvalJs(primary_main_frame,
                                     "document.body.firstChild.tagName;")
                         .ExtractString());
  EXPECT_EQ("application/pdf", content::EvalJs(primary_main_frame,
                                               "document.body.firstChild.type;")
                                   .ExtractString());
  EXPECT_EQ("about:blank",
            content::EvalJs(primary_main_frame, "document.body.firstChild.src;")
                .ExtractString());
  EXPECT_TRUE(
      content::EvalJs(primary_main_frame,
                      "document.body.firstChild.hasAttribute('internalid');")
          .ExtractBool());
}

// Helper class to allow pausing the asynchronous attachment of an inner
// WebContents between MimeHandlerViewAttachHelper's AttachToOuterWebContents()
// and ResumeAttachOrDestroy().  This corresponds to the point where the inner
// WebContents has been created but not yet attached or navigated.
class InnerWebContentsAttachDelayer {
 public:
  explicit InnerWebContentsAttachDelayer(
      content::RenderFrameHost* outer_frame) {
    auto* mime_handler_view_helper =
        extensions::MimeHandlerViewAttachHelper::Get(
            outer_frame->GetProcess()->GetID());
    mime_handler_view_helper->set_resume_attach_callback_for_testing(
        base::BindOnce(&InnerWebContentsAttachDelayer::ResumeAttachCallback,
                       base::Unretained(this)));
  }

  void ResumeAttachCallback(base::OnceClosure resume_closure) {
    // Called to continue in the test while the attachment is paused. The
    // attachment will continue when the test calls ResumeAttach.
    resume_closure_ = std::move(resume_closure);
    run_loop_.Quit();
  }

  void WaitForAttachStart() { run_loop_.Run(); }

  void ResumeAttach() {
    ASSERT_TRUE(resume_closure_);
    std::move(resume_closure_).Run();
  }

 private:
  base::OnceClosure resume_closure_;
  base::RunLoop run_loop_;
};

// Ensure that when the only other PDF instance closes in the middle of
// attaching an inner WebContents for a PDF, the inner WebContents can still
// successfully complete its attachment and subsequent navigation.  See
// https://crbug.com/1295431.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfExtensionLoadedWhileOldPdfCloses) {
  // Load test PDF in first tab.
  const GURL main_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  auto* primary_main_frame = GetActiveWebContents()->GetPrimaryMainFrame();

  // Verify the PDF has loaded.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  EXPECT_NE(primary_main_frame, guest_view->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Verify we loaded the extension.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url,
            guest_view->GetGuestMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(main_url, primary_main_frame->GetLastCommittedURL());

  // Open another tab and navigate it to a same-site non-PDF URL.
  ui_test_utils::TabAddedWaiter add_tab1(browser());
  chrome::NewTab(browser());
  add_tab1.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_EQ(new_web_contents, GetActiveWebContents());
  const GURL non_pdf_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_pdf_url));

  // Start loading another PDF in the new tab, but pause during guest attach.
  // It is important that the PDF navigation uses the same RFH as `delayer`.
  InnerWebContentsAttachDelayer delayer(
      new_web_contents->GetPrimaryMainFrame());
  content::TestNavigationObserver navigation_observer(new_web_contents);
  new_web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(main_url));
  delayer.WaitForAttachStart();

  // Close the first tab, destroying the first PDF while the second PDF is in
  // the middle of initialization. In https://crbug.com/1295431, the extension
  // process exited here and caused a crash when the second PDF resumed.
  EXPECT_EQ(2U, GetGuestViewManager()->GetNumGuestsActive());
  ASSERT_TRUE(browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE));
  // `TestGuestViewManager` manages the guests by the order of creation.
  GetGuestViewManager()->WaitForFirstGuestDeleted();
  EXPECT_EQ(1U, GetGuestViewManager()->GetNumGuestsActive());
  primary_main_frame = new_web_contents->GetPrimaryMainFrame();

  // Now resume the guest attachment and ensure the second PDF loads without
  // crashing.
  delayer.ResumeAttach();
  navigation_observer.Wait();
  auto* guest_view2 = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view2);
  EXPECT_NE(primary_main_frame, guest_view2->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view2);

  // Verify we loaded the extension.
  EXPECT_EQ(extension_url,
            guest_view2->GetGuestMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(main_url, primary_main_frame->GetLastCommittedURL());
}

// This test verifies that when a PDF is served with a restrictive
// Content-Security-Policy, the embed tag is still sized correctly.
// Regression test for https://crbug.com/271452.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, CSPDoesNotBlockEmbedStyles) {
  const GURL main_url(embedded_test_server()->GetURL("/pdf/test-csp.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_TRUE(GetActiveWebContents());
  auto* primary_main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(primary_main_frame);

  // Verify the pdf has loaded.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  auto* guest_main_frame = guest_view->GetGuestMainFrame();
  EXPECT_NE(primary_main_frame, guest_main_frame);
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Verify the extension was loaded.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url, guest_main_frame->GetLastCommittedURL());
  EXPECT_EQ(main_url, primary_main_frame->GetLastCommittedURL());

  // Verify that the plugin occupies all of the page area.
  const gfx::Rect embedder_rect =
      primary_main_frame->GetView()->GetViewBounds();
  const gfx::Rect guest_rect = guest_main_frame->GetView()->GetViewBounds();
  EXPECT_EQ(embedder_rect, guest_rect);
}

// This test verifies that when a PDF is served with
// Content-Security-Policy: sandbox, this is ignored and the PDF is displayed.
// Regression test for https://crbug.com/1187122.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, CSPWithSandboxDoesNotBlockPDF) {
  const GURL main_url(
      embedded_test_server()->GetURL("/pdf/test-csp-sandbox.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_TRUE(GetActiveWebContents());
  auto* primary_main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(primary_main_frame);

  // Verify the pdf has loaded.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  EXPECT_NE(primary_main_frame, guest_view->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Verify the extension was loaded.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url,
            guest_view->GetGuestMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(main_url, primary_main_frame->GetLastCommittedURL());
}

// This test verifies that Content-Security-Policy's frame-ancestors 'none'
// directive is effective on a PDF response.
// Regression test for https://crbug.com/1107535.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, CSPFrameAncestorsCanBlockEmbedding) {
  WebContents* web_contents = GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(
      "*because an ancestor violates the following Content Security Policy "
      "directive: \"frame-ancestors 'none'*");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/pdf/frame-test-csp-frame-ancestors-none.html")));

  ASSERT_TRUE(console_observer.Wait());

  EXPECT_EQ(0, CountPDFProcesses());
}

// This test verifies that Content-Security-Policy's frame-ancestors directive
// overrides an X-Frame-Options header on a PDF response.
// Regression test for https://crbug.com/1107535.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest,
                       CSPFrameAncestorsOverridesXFrameOptions) {
  const GURL main_url(
      embedded_test_server()->GetURL("/pdf/frame-test-csp-and-xfo.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  ASSERT_TRUE(GetActiveWebContents());
  auto* primary_main_frame = GetActiveWebContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(primary_main_frame);

  // Verify the pdf has loaded.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  EXPECT_NE(primary_main_frame, guest_view->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Verify the extension was loaded.
  const GURL extension_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  EXPECT_EQ(extension_url,
            guest_view->GetGuestMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(main_url, primary_main_frame->GetLastCommittedURL());
}

class PDFExtensionLoadTest : public PDFExtensionTest,
                             public testing::WithParamInterface<int> {
 protected:
  // Load all the PDFs contained in chrome/test/data/<dir_name>. The files are
  // sharded across kNumberLoadTestParts using base::PersistentHash(filename).
  void LoadAllPdfsTest(const std::string& dir_name) {
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
      if (base::PersistentHash(filename) % kNumberLoadTestParts == GetParam()) {
        LOG(INFO) << "Loading: " << pdf_file;
        testing::AssertionResult success =
            LoadPdf(embedded_test_server()->GetURL("/" + pdf_file));
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
};

IN_PROC_BROWSER_TEST_P(PDFExtensionLoadTest, Load) {
  LoadAllPdfsTest("pdf");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_P(PDFExtensionLoadTest, LoadPrivate) {
  LoadAllPdfsTest("pdf_private");
}
#endif

// We break PDFExtensionLoadTest up into kNumberLoadTestParts.
INSTANTIATE_TEST_SUITE_P(PDFTestFiles,
                         PDFExtensionLoadTest,
                         testing::Range(0, kNumberLoadTestParts));

using PDFExtensionBlobNavigationTest = PDFExtensionTest;

IN_PROC_BROWSER_TEST_F(PDFExtensionBlobNavigationTest, NewTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/blob_navigation_new_tab.html")));

  // Calling `window.open` without a user gesture will be blocked by the popup
  // blocker. `ExecJs()` emulates a user gesture which bypasses the restriction.
  content::TestNavigationObserver navigation_observer(nullptr);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "openBlobPdfInNewTab()"));
  navigation_observer.Wait();

  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  WebContents* new_tab_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(new_tab_contents));
}

IN_PROC_BROWSER_TEST_F(PDFExtensionBlobNavigationTest, SameTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/pdf/blob_navigation_same_tab.html"),
      /*number_of_navigations=*/2));
  EXPECT_TRUE(
      pdf_extension_test_util::EnsurePDFHasLoaded(GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LoadInPlatformApp) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(R"(
    {
      "name": "PDFExtensionTest App",
      "version": "1.0",
      "manifest_version": 2,
      "app": {
        "background": {
          "scripts": ["background_script.js"]
        }
      }
    }
  )");

  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.app.runtime.onLaunched.addListener(() => {
      chrome.app.window.create('test.pdf', () => {
        chrome.test.notifyPass();
      });
    });
  )");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string pdf_contents;
    ASSERT_TRUE(base::ReadFileToString(
        GetTestResourcesParentDir().AppendASCII("pdf").AppendASCII("test.pdf"),
        &pdf_contents));
    dir.WriteFile(FILE_PATH_LITERAL("test.pdf"), pdf_contents);
  }

  extensions::ResultCatcher result_catcher;
  ASSERT_TRUE(LoadAndLaunchApp(dir.UnpackedPath(), /*uses_guest_view=*/true));
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  auto* app_registry = extensions::AppWindowRegistry::Get(browser()->profile());
  ASSERT_TRUE(app_registry);
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      app_registry->app_windows();
  ASSERT_EQ(app_windows.size(), 1u);
  extensions::AppWindow* window = app_windows.front();
  ASSERT_TRUE(window);
  content::WebContents* app_contents = window->web_contents();

  ASSERT_TRUE(content::WaitForLoadStop(app_contents));
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(app_contents));
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
        browser_context->GetDownloadManager();
    download_awaiter_ = std::make_unique<DownloadAwaiter>();
    download_manager->AddObserver(download_awaiter_.get());
  }

  void TearDownOnMainThread() override {
    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    content::DownloadManager* download_manager =
        browser_context->GetDownloadManager();
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
    content::RenderFrameHost* iframe_render_frame_host =
        ChildFrameAt(GetActiveWebContents(), 0);
    ASSERT_TRUE(iframe_render_frame_host);
    ASSERT_TRUE(
        content::ExecJs(iframe_render_frame_host,
                        "document.getElementById('open-button').click();"));
  }

  void ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch() {
    // Validate that we downloaded a single PDF and didn't launch the PDF
    // plugin.
    EXPECT_EQ(embedded_test_server()->GetURL("/pdf/test.pdf"),
              AwaitAndGetLastDownloadedUrl());
    EXPECT_EQ(1u, GetNumberOfDownloads());
    EXPECT_EQ(0, CountPDFProcesses());
  }

 private:
  size_t GetNumberOfDownloads() {
    content::BrowserContext* browser_context =
        GetActiveWebContents()->GetBrowserContext();
    content::DownloadManager* download_manager =
        browser_context->GetDownloadManager();

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));

  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

// TODO(crbug.com/1201401): fix flakiness and reenable. Also, that test
// became flaky on Windows, see crbug.com/1323701.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#define MAYBE_EmbedPdfPlaceholderWithCSP DISABLED_EmbedPdfPlaceholderWithCSP
#else
#define MAYBE_EmbedPdfPlaceholderWithCSP EmbedPdfPlaceholderWithCSP
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest,
                       MAYBE_EmbedPdfPlaceholderWithCSP) {
  // Navigate to a page with CSP that uses <embed> to embed a PDF as a plugin.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/pdf_embed_csp.html")));
  PluginTestUtils::WaitForPlaceholderReady(GetActiveWebContents(), "pdf_embed");

  // Fake a click on the <embed>, then press Enter to trigger the download.
  gfx::Point point_in_pdf(100, 100);
  content::SimulateMouseClickAt(GetActiveWebContents(), kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kLeft,
                                point_in_pdf);
  content::SimulateKeyPress(GetActiveWebContents(), ui::DomKey::ENTER,
                            ui::DomCode::ENTER, ui::VKEY_RETURN, false, false,
                            false, false);

  ValidateSingleSuccessfulDownloadAndNoPDFPluginLaunch();
}

IN_PROC_BROWSER_TEST_F(PDFPluginDisabledTest, IframePdfPlaceholderWithCSP) {
  // Navigate to a page that uses <iframe> to embed a PDF as a plugin.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/pdf_iframe_csp.html")));

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

// Test that if the plugin tries to load a URL that redirects then it will fail
// to load. This is to avoid the source origin of the document changing during
// the redirect, which can have security implications. https://crbug.com/653749.
//
// Note that this can happen only during partial loading, as the initial URL
// load is handled by MimeHandlerView, and the plugin only gets the response.
IN_PROC_BROWSER_TEST_F(PDFExtensionTestWithPartialLoading,
                       PartialRedirectsFailInPlugin) {
  // Should match values used by `chrome_pdf::DocumentLoaderImpl`.
  constexpr size_t kDefaultRequestSize = 65536;
  constexpr size_t kChunkCloseDistance = 10;

  std::string pdf_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    ASSERT_TRUE(
        base::ReadFileToString(test_data_dir.AppendASCII("pdf").AppendASCII(
                                   "test-ranges-linearized.pdf"),
                               &pdf_contents));
  }
  ASSERT_GT(pdf_contents.size(),
            kDefaultRequestSize * (kChunkCloseDistance + 2));

  // Use an additional test server, to allow customizing the response handling.
  net::test_server::EmbeddedTestServer test_server;

  constexpr char kActualPdf[] = "/pdf/test-ranges-linearized.pdf";
  constexpr char kSimulatedPdf[] = "/simulated/test-ranges-linearized.pdf";
  net::test_server::ControllableHttpResponse initial_response(&test_server,
                                                              kSimulatedPdf);
  net::test_server::ControllableHttpResponse followup_response(&test_server,
                                                               kSimulatedPdf);
  auto handle = test_server.StartAndReturnHandle();

  WebContents* contents = GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(contents);
  contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          test_server.GetURL(kSimulatedPdf)));

  {
    SCOPED_TRACE("Waiting for initial request");
    initial_response.WaitForRequest();
  }
  initial_response.Send("HTTP/1.1 200 OK\r\n");
  initial_response.Send("Accept-Ranges: bytes\r\n");
  initial_response.Send(
      base::StringPrintf("Content-Length: %zu\r\n", pdf_contents.size()));
  initial_response.Send("Content-Type: application/pdf\r\n");
  initial_response.Send("\r\n");
  initial_response.Send(pdf_contents.substr(0, kDefaultRequestSize));

  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());

  {
    SCOPED_TRACE("Waiting for follow-up request");
    followup_response.WaitForRequest();
  }
  followup_response.Send("HTTP/1.1 301 Moved Permanently\r\n");
  followup_response.Send(base::StringPrintf(
      "Location: %s\r\n",
      embedded_test_server()->GetURL(kActualPdf).spec().c_str()));
  followup_response.Send("\r\n");
  followup_response.Done();

  // TODO(crbug.com/1228987): Load success or failure is non-deterministic
  // currently, due to races between viewport messages and loading. For this
  // test, we only care that loading terminated, not about success or failure.
  std::ignore = pdf_extension_test_util::EnsurePDFHasLoaded(contents);
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(data_url)));
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

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(
      "*Streams are only available from a mime handler view guest.*");
  GURL forbidden_url(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html?"
      "https://example.com/notrequested.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), forbidden_url));

  ASSERT_TRUE(console_observer.Wait());

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
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(test_pdf_url);
  ASSERT_TRUE(guest_view);

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
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(test_pdf_url);
  ASSERT_TRUE(guest_view);

  EXPECT_EQ(1, CountPDFProcesses());
}

// This test ensures that link permissions are enforced properly in PDFs.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkPermissions) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_view);

  // chrome://favicon links should be allowed for PDFs, while chrome://settings
  // links should not.
  GURL valid_link_url(std::string(chrome::kChromeUIFaviconURL) +
                      "https://www.google.ca/");
  GURL invalid_link_url(chrome::kChromeUISettingsURL);

  GURL unfiltered_valid_link_url(valid_link_url);
  content::RenderProcessHost* rph =
      guest_view->GetGuestMainFrame()->GetProcess();
  rph->FilterURL(true, &valid_link_url);
  rph->FilterURL(true, &invalid_link_url);

  // Invalid link URLs should be changed to "about:blank#blocked" when filtered.
  EXPECT_EQ(unfiltered_valid_link_url, valid_link_url);
  EXPECT_EQ(GURL(content::kBlockedURL), invalid_link_url);
}

// This test ensures that titles are set properly for PDFs without /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithNoTitle) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_view);
  EXPECT_EQ(u"test.pdf", guest_view->GetController()
                             .GetLastCommittedEntry()
                             ->GetTitleForDisplay());
  EXPECT_EQ(u"test.pdf", GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for PDFs with /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithTitle) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-title.pdf"));
  ASSERT_TRUE(guest_view);
  EXPECT_EQ(u"PDF title test", guest_view->GetController()
                                   .GetLastCommittedEntry()
                                   ->GetTitleForDisplay());
  EXPECT_EQ(u"PDF title test", GetActiveWebContents()->GetTitle());
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
  EXPECT_EQ(u"TabTitleWithEmbeddedPdf", GetActiveWebContents()->GetTitle());
}

// Flaky, http://crbug.com/767427
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_PdfZoomWithoutBubble DISABLED_PdfZoomWithoutBubble
#else
#define MAYBE_PdfZoomWithoutBubble PdfZoomWithoutBubble
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, MAYBE_PdfZoomWithoutBubble) {
  MimeHandlerViewGuest* guest_view = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest_view);
  WebContents* web_contents = GetActiveWebContents();

  // Here we look at the presets to find the next zoom level above 0. Ideally
  // we should look at the zoom levels from the PDF viewer javascript, but we
  // assume they'll always match the browser presets, which are easier to
  // access. In the script below, we zoom to 100% (0), then wait for this to be
  // picked up by the browser zoom, then zoom to the next zoom level. This
  // ensures the test passes regardless of the initial default zoom level.
  std::vector<double> preset_zoom_levels = zoom::PageZoom::PresetZoomLevels(0);
  auto it = base::ranges::find(preset_zoom_levels, 0);
  ASSERT_NE(it, preset_zoom_levels.end());
  it++;
  ASSERT_NE(it, preset_zoom_levels.end());
  double new_zoom_level = *it;

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  // We expect a ZoomChangedEvent with can_show_bubble == false if the PDF
  // extension behaviour is properly picked up. The test times out otherwise.
  zoom::ZoomChangedWatcher watcher(
      zoom_controller, zoom::ZoomController::ZoomChangedEventData(
                           web_contents, 0, new_zoom_level,
                           zoom::ZoomController::ZOOM_MODE_MANUAL, false));

  // Zoom PDF via script.
#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
#endif
  ASSERT_TRUE(content::ExecuteScript(guest_view->GetGuestMainFrame(),
                                     "while (viewer.viewport.getZoom() < 1) {"
                                     "  viewer.viewport.zoomIn();"
                                     "}"
                                     "setTimeout(() => {"
                                     "  viewer.viewport.zoomIn();"
                                     "}, 1);"));

  watcher.Wait();
#if defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_MAC)
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
#endif
}

class PDFExtensionScrollTest : public PDFExtensionTest {
 public:
  void SetUpOnMainThread() override {
    PDFExtensionTest::SetUpOnMainThread();

    GetActiveWebContents()->Resize({0, 0, 1024, 768});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Smooth scrolling confuses the test cases that reads the scroll bar
    // position.
    command_line->AppendSwitch(switches::kDisableSmoothScrolling);
  }

 protected:
  class ScrollEventWaiter {
   public:
    explicit ScrollEventWaiter(content::ToRenderFrameHost guest_main_frame)
        : message_queue_(guest_main_frame.render_frame_host()) {
      content::ExecuteScriptAsync(
          guest_main_frame,
          R"(viewer.shadowRoot.querySelector('#scroller').onscroll = () => {
            if (viewer.viewport.scrollContent_.unackedScrollsToRemote_ === 0) {
              window.domAutomationController.send('dispatchedScrollEvent');
            }
          };
          if (viewer.viewport.scrollContent_.unackedScrollsToRemote_ === 0) {
            window.domAutomationController.send('dispatchedScrollEvent');
          })");

      // Wait for pending scroll-to-remotes to complete.
      Wait();
    }

    void Reset() { message_queue_.ClearQueue(); }

    void Wait() {
      std::string message;
      ASSERT_TRUE(message_queue_.WaitForMessage(&message));
      EXPECT_EQ("\"dispatchedScrollEvent\"", message);
    }

   private:
    content::DOMMessageQueue message_queue_;
  };

  // Scroll increment in CSS pixels. Should match `SCROLL_INCREMENT` in
  // //chrome/browser/resources/pdf/viewport.js.
  static constexpr int kScrollIncrement = 40;

  // Scrolling by a fraction of the viewport height may introduce slight
  // position differences on various platforms due to rounding. Tolerate this
  // difference.
  static constexpr float kScrollPositionEpsilon = 2.0f;

  static int GetViewportHeight(content::ToRenderFrameHost guest_main_frame) {
    int viewport_height = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        guest_main_frame,
        "window.domAutomationController.send(viewer.viewport.size.height);",
        &viewport_height));
    return viewport_height;
  }

  static int GetViewportScrollPositionX(
      content::ToRenderFrameHost guest_main_frame) {
    int position_x = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        guest_main_frame,
        "window.domAutomationController.send(viewer.viewport.position.x);",
        &position_x));
    return position_x;
  }

  static int GetViewportScrollPositionY(
      content::ToRenderFrameHost guest_main_frame) {
    int position_y = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        guest_main_frame,
        "window.domAutomationController.send(viewer.viewport.position.y);",
        &position_y));
    return position_y;
  }
};

IN_PROC_BROWSER_TEST_F(PDFExtensionScrollTest, WithSpace) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  SetInputFocusOnPlugin(guest);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_mainframe));

  // Get the viewport height, since the scroll distance is based on it.
  const int viewport_height = GetViewportHeight(guest_mainframe);
  ASSERT_GT(viewport_height, 0);

  // For web content, page down / page up scrolling only scrolls by a fraction
  // of the viewport height. The PDF Viewer should match that behavior.
  const float scroll_height =
      viewport_height * cc::kMinFractionToStepWhenPaging;

  // Press Space to scroll down.
  ScrollEventWaiter scroll_waiter(guest_mainframe);
  content::SimulateKeyPress(GetActiveWebContents(),
                            ui::DomKey::FromCharacter(' '), ui::DomCode::SPACE,
                            ui::VKEY_SPACE,
                            /*control=*/false, /*shift=*/false, /*alt=*/false,
                            /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_NEAR(scroll_height, GetViewportScrollPositionY(guest_mainframe),
              kScrollPositionEpsilon);

  // Press Space to scroll down again.
  scroll_waiter.Reset();
  content::SimulateKeyPress(GetActiveWebContents(),
                            ui::DomKey::FromCharacter(' '), ui::DomCode::SPACE,
                            ui::VKEY_SPACE,
                            /*control=*/false, /*shift=*/false, /*alt=*/false,
                            /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_NEAR(scroll_height * 2, GetViewportScrollPositionY(guest_mainframe),
              kScrollPositionEpsilon);

  // Press Shift+Space to scroll up.
  scroll_waiter.Reset();
  content::SimulateKeyPress(GetActiveWebContents(),
                            ui::DomKey::FromCharacter(' '), ui::DomCode::SPACE,
                            ui::VKEY_SPACE,
                            /*control=*/false, /*shift=*/true, /*alt=*/false,
                            /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_NEAR(scroll_height, GetViewportScrollPositionY(guest_mainframe),
              kScrollPositionEpsilon);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionScrollTest, WithPageDownUp) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  SetInputFocusOnPlugin(guest);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_mainframe));

  // Get the viewport height, since the scroll distance is based on it.
  const int viewport_height = GetViewportHeight(guest_mainframe);
  ASSERT_GT(viewport_height, 0);

  // For web content, page down / page up scrolling only scrolls by a fraction
  // of the viewport height. The PDF Viewer should match that behavior.
  const float scroll_height =
      viewport_height * cc::kMinFractionToStepWhenPaging;

  // Press PageDown to scroll down.
  ScrollEventWaiter scroll_waiter(guest_mainframe);
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::PAGE_DOWN,
                                       ui::DomCode::PAGE_DOWN, ui::VKEY_NEXT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_NEAR(scroll_height, GetViewportScrollPositionY(guest_mainframe),
              kScrollPositionEpsilon);

  // Press PageDown to scroll down again.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::PAGE_DOWN,
                                       ui::DomCode::PAGE_DOWN, ui::VKEY_NEXT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_NEAR(scroll_height * 2, GetViewportScrollPositionY(guest_mainframe),
              kScrollPositionEpsilon);

  // Press PageUp to scroll up.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(
      GetActiveWebContents(), ui::DomKey::PAGE_UP, ui::DomCode::PAGE_UP,
      ui::VKEY_PRIOR,
      /*control=*/false, /*shift=*/false, /*alt=*/false,
      /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_NEAR(scroll_height, GetViewportScrollPositionY(guest_mainframe),
              kScrollPositionEpsilon);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionScrollTest, WithArrowLeftRight) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf#zoom=200"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  SetInputFocusOnPlugin(guest);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_mainframe));

  // Press ArrowRight to scroll right.
  ScrollEventWaiter scroll_waiter(guest_mainframe);
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::ARROW_RIGHT,
                                       ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionX(guest_mainframe));

  // Press ArrowRight to scroll right again.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::ARROW_RIGHT,
                                       ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement * 2, GetViewportScrollPositionX(guest_mainframe));

  // Press ArrowLeft to scroll left.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::ARROW_LEFT,
                                       ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionX(guest_mainframe));
}

IN_PROC_BROWSER_TEST_F(PDFExtensionScrollTest, WithArrowDownUp) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  SetInputFocusOnPlugin(guest);
  ASSERT_EQ(0, GetViewportScrollPositionY(guest_mainframe));

  // Press ArrowDown to scroll down.
  ScrollEventWaiter scroll_waiter(guest_mainframe);
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::ARROW_DOWN,
                                       ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionY(guest_mainframe));

  // Press ArrowDown to scroll down again.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::ARROW_DOWN,
                                       ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement * 2, GetViewportScrollPositionY(guest_mainframe));

  // Press ArrowUp to scroll up.
  scroll_waiter.Reset();
  content::SimulateKeyPressWithoutChar(GetActiveWebContents(),
                                       ui::DomKey::ARROW_UP,
                                       ui::DomCode::ARROW_UP, ui::VKEY_UP,
                                       /*control=*/false, /*shift=*/false,
                                       /*alt=*/false,
                                       /*command=*/false);
  ASSERT_NO_FATAL_FAILURE(scroll_waiter.Wait());
  EXPECT_EQ(kScrollIncrement, GetViewportScrollPositionY(guest_mainframe));
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, SelectAllShortcut) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* frame = GetPluginFrame(guest);
  ASSERT_TRUE(frame);
  content::RenderWidgetHostView* view = frame->GetView();
  EXPECT_THAT(view->GetSelectedText(), IsEmpty());

  base::RunLoop run_loop;
  content::TextInputManagerTester input_tester(GetActiveWebContents());
  input_tester.SetOnTextSelectionChangedCallback(run_loop.QuitClosure());

  bool control = false;
  bool command = false;
#if BUILDFLAG(IS_MAC)
  command = true;
#else
  control = true;
#endif
  content::SimulateKeyPress(GetActiveWebContents(),
                            ui::DomKey::FromCharacter('a'), ui::DomCode::US_A,
                            ui::VKEY_A, control,
                            /*shift=*/false,
                            /*alt=*/false, command);
  run_loop.Run();

#if BUILDFLAG(IS_WIN)
  constexpr char kExpectedText[] = "this is some text\r\nsome more text";
#else
  constexpr char kExpectedText[] = "this is some text\nsome more text";
#endif
  EXPECT_EQ(base::UTF16ToUTF8(view->GetSelectedText()), kExpectedText);
}

// TODO(crbug.com/1253714): Add tests for using space and shift+space shortcuts
// for scrolling PDFs.

// Test that even if a different tab is selected when a navigation occurs,
// the correct tab still gets navigated (see crbug.com/672563).
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, NavigationOnCorrectTab) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);
  WebContents* web_contents = GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  content::TestNavigationObserver active_navigation_observer(
      active_web_contents);
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(
      content::ExecuteScript(guest->GetGuestMainFrame(),
                             "viewer.navigator_.navigate("
                             "    'www.example.com',"
                             "    WindowOpenDisposition.CURRENT_TAB);"));
  navigation_observer.Wait();

  EXPECT_FALSE(navigation_observer.last_navigation_url().is_empty());
  EXPECT_TRUE(active_navigation_observer.last_navigation_url().is_empty());
  EXPECT_FALSE(active_web_contents->GetController().GetPendingEntry());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, MultipleDomains) {
  ASSERT_TRUE(LoadPdfInNewTab(
      embedded_test_server()->GetURL("a.com", "/pdf/test.pdf")));
  ASSERT_TRUE(LoadPdfInNewTab(
      embedded_test_server()->GetURL("b.com", "/pdf/test.pdf")));
  EXPECT_EQ(2, CountPDFProcesses());
}

namespace {

class PDFExtensionIsolatedContentTest : public base::test::WithFeatureOverride,
                                        public PDFExtensionTest {
 protected:
  PDFExtensionIsolatedContentTest()
      : base::test::WithFeatureOverride(features::kSitePerProcess) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionIsolatedContentTest);

}  // namespace

// Makes sure `PDFExtensionIsolatedContentTest` runs with and without Site
// Isolation enabled (see crbug.com/1298269).
//
// This is a separate test because fatal assertions in `SetUpInMainThread()`
// don't terminate early, so there's no point asserting before every test.
IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, ExpectSiteIsolation) {
  EXPECT_EQ(IsParamFeatureEnabled(),
            content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, PdfAndHtml) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Load a page with an embedded PDF and an HTML iframe, both of the same
  // origin.
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/embed_pdf_and_html.html"));
  ASSERT_TRUE(guest);

  // The PDF plugin frame and the iframe should not share renderer processes
  // even though they share origins.
  std::vector<content::RenderFrameHost*> plugin_frames =
      GetPdfPluginFrames(GetActiveWebContents());
  ASSERT_EQ(plugin_frames.size(), 1u);

  content::RenderFrameHost* iframe = ChildFrameAt(GetActiveWebContents(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/title1.html"));

  EXPECT_EQ(plugin_frames[0]->GetLastCommittedOrigin(),
            iframe->GetLastCommittedOrigin());
  EXPECT_NE(plugin_frames[0]->GetProcess(), iframe->GetProcess());
  EXPECT_FALSE(content::HasOriginKeyedProcess(plugin_frames[0]));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, DataNavigation) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/data_url_rectangles.html"));

  // The PDF plugin frame and the extension main frame should not share renderer
  // processes even though the extension triggers a data: navigation when
  // loading its plugin.
  std::vector<content::RenderFrameHost*> plugin_frames =
      GetPdfPluginFrames(GetActiveWebContents());
  ASSERT_EQ(plugin_frames.size(), 1u);
  EXPECT_NE(plugin_frames[0]->GetProcess(),
            guest->GetGuestMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, HistoryNavigation) {
  // Navigating to a PDF should spawn a PDF renderer process.
  EXPECT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
  EXPECT_EQ(CountPDFProcesses(), 1);

  // Navigating to non-PDF content should remove the PDF renderer process.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(CountPDFProcesses(), 0);

  // Navigating back to the PDF should once again spawn a PDF renderer process.
  WebContents* web_contents = GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  EXPECT_EQ(CountPDFProcesses(), 1);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionIsolatedContentTest, Jitless) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  // PDF content should always be in JIT-less processes.
  std::vector<content::RenderFrameHost*> plugin_frames =
      GetPdfPluginFrames(GetActiveWebContents());
  ASSERT_EQ(plugin_frames.size(), 1u);
  EXPECT_TRUE(plugin_frames[0]->GetProcess()->IsJitDisabled());
}

class PDFExtensionLinkClickTest : public PDFExtensionTest {
 public:
  PDFExtensionLinkClickTest() = default;
  ~PDFExtensionLinkClickTest() override = default;

 protected:
  MimeHandlerViewGuest* LoadTestLinkPdfGetMimeHandlerView() {
    return LoadPdfGetMimeHandlerView(
        embedded_test_server()->GetURL("/pdf/test-link.pdf"));
  }

  // The rectangle of the link in test-link.pdf is [72 706 164 719] in PDF user
  // space. To calculate a position inside this rectangle, several
  // transformations have to be applied:
  // [a] (110, 110) in Blink page coordinates ->
  // [b] (219, 169) in Blink screen coordinates ->
  // [c] (115, 169) in PDF Device space coordinates ->
  // [d] (82.5, 709.5) in PDF user space coordinates.
  // This performs the [a] to [b] transformation, since that is the coordinate
  // space with respect to guest that SimulateMouseClickAt() needs.
  gfx::Point GetLinkPosition(MimeHandlerViewGuest* guest) {
    return ConvertPageCoordToScreenCoord(guest->GetGuestMainFrame(),
                                         {110, 110});
  }
};

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, CtrlLeft) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, kDefaultKeyModifier,
                       blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, Middle) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, 0, blink::WebMouseEvent::Button::kMiddle,
                       GetLinkPosition(guest));
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, CtrlShiftLeft) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  WebContents* web_contents = GetActiveWebContents();

  const int modifiers = blink::WebInputEvent::kShiftKey | kDefaultKeyModifier;

  SimulateMouseClickAt(guest, modifiers, blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, ShiftMiddle) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, blink::WebInputEvent::kShiftKey,
                       blink::WebMouseEvent::Button::kMiddle,
                       GetLinkPosition(guest));
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, ShiftLeft) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, blink::WebInputEvent::kShiftKey,
                       blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));
  ui_test_utils::WaitForBrowserToOpen();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

// This test opens a PDF by clicking a link via javascript and verifies that
// the PDF is loaded and functional by clicking a link in the PDF. The link
// click in the PDF opens a new tab. The main page handles the pageShow event
// and updates the history state.
IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, OpenPDFWithReplaceState) {
  // Navigate to the main page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/pdf/pdf_href_replace_state.html")));
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Click on the link which opens the PDF via JS.
  content::TestNavigationObserver navigation_observer(web_contents);
  const char kPdfLinkClick[] = "document.getElementById('link').click();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, kPdfLinkClick));
  navigation_observer.Wait();
  const GURL& current_url = web_contents->GetLastCommittedURL();
  ASSERT_EQ("/pdf/test-link.pdf", current_url.path());

  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  // Now click on the link to example.com in the PDF. This should open up a new
  // tab.
  MimeHandlerViewGuest* guest = MimeHandlerViewGuest::FromGuestViewBase(
      GetGuestViewManager()->WaitForSingleGuestViewCreated());
  SimulateMouseClickAt(guest, kDefaultKeyModifier,
                       blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));

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

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("http://www.example.com/", url.spec());
}

namespace {
// Fails the test if a navigation is started in the given WebContents.
class FailOnNavigation : public content::WebContentsObserver {
 public:
  explicit FailOnNavigation(WebContents* contents)
      : content::WebContentsObserver(contents) {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ADD_FAILURE() << "Unexpected navigation";
  }
};
}  // namespace

// If the PDF viewer can't navigate the tab using a tab id, make sure it doesn't
// try to navigate the mime handler extension's frame.
// Regression test for https://crbug.com/1158381
IN_PROC_BROWSER_TEST_F(PDFExtensionLinkClickTest, LinkClickInPdfInNonTab) {
  // For ease of testing, we'll still load the PDF in a tab, but we clobber the
  // tab id in the viewer to make it think it's not in a tab.
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();
  ASSERT_TRUE(guest);
  ASSERT_TRUE(
      content::ExecuteScript(guest->GetGuestMainFrame(),
                             "window.viewer.browserApi.getStreamInfo().tabId = "
                             "    chrome.tabs.TAB_ID_NONE;"));

  FailOnNavigation fail_if_mimehandler_navigates(guest->web_contents());
  SimulateMouseClickAt(guest, blink::WebInputEvent::kNoModifiers,
                       blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));

  // Since the guest contents is for a mime handling extension (in this case,
  // the PDF viewer extension), it must not navigate away from the extension. If
  // |fail_if_mimehandler_navigates| doesn't see a navigation, we consider the
  // test to have passed.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

class PDFExtensionInternalLinkClickTest : public PDFExtensionTest {
 public:
  PDFExtensionInternalLinkClickTest() = default;
  ~PDFExtensionInternalLinkClickTest() override = default;

 protected:
  MimeHandlerViewGuest* LoadTestLinkPdfGetMimeHandlerView() {
    return LoadPdfGetMimeHandlerView(
        embedded_test_server()->GetURL("/pdf/test-internal-link.pdf"));
  }

  gfx::Point GetLinkPosition(MimeHandlerViewGuest* guest) {
    // The whole first page is a link.
    return ConvertPageCoordToScreenCoord(guest->GetGuestMainFrame(),
                                         {100, 100});
  }
};

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, CtrlLeft) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, kDefaultKeyModifier,
                       blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, Middle) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, 0, blink::WebMouseEvent::Button::kMiddle,
                       GetLinkPosition(guest));
  ui_test_utils::TabAddedWaiter(browser()).Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  WebContents* active_web_contents = GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetVisibleURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionInternalLinkClickTest, ShiftLeft) {
  MimeHandlerViewGuest* guest = LoadTestLinkPdfGetMimeHandlerView();

  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  WebContents* web_contents = GetActiveWebContents();

  SimulateMouseClickAt(guest, blink::WebInputEvent::kShiftKey,
                       blink::WebMouseEvent::Button::kLeft,
                       GetLinkPosition(guest));
  ui_test_utils::WaitForBrowserToOpen();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetVisibleURL();
  EXPECT_EQ("/pdf/test-internal-link.pdf", url.path());
  EXPECT_EQ("page=2&zoom=100,0,200", url.ref());
}

class PDFExtensionComboBoxTest : public PDFExtensionTest {
 public:
  MimeHandlerViewGuest* LoadTestComboBoxPdfGetMimeHandlerView() {
    return LoadPdfGetMimeHandlerView(
        embedded_test_server()->GetURL("/pdf/combobox_form.pdf"));
  }

  // Returns a point near the left edge of the editable combo box in
  // combobox_form.pdf, inside the combo box rect. The point is in Blink screen
  // coordinates.
  //
  // The combo box's rect is [100 50 200 80] in PDF user space. (136, 318) in
  // Blink page coordinates corresponds to approximately (102, 62) in PDF user
  // space coordinates. See PDFExtensionLinkClickTest::GetLinkPosition() for
  // more information on all the coordinate systems involved.
  gfx::Point GetEditableComboBoxLeftPosition(MimeHandlerViewGuest* guest) {
    return ConvertPageCoordToScreenCoord(guest->GetGuestMainFrame(),
                                         {136, 318});
  }

  void ClickLeftSideOfEditableComboBox(MimeHandlerViewGuest* guest) {
    SimulateMouseClickAt(guest, 0, blink::WebMouseEvent::Button::kLeft,
                         GetEditableComboBoxLeftPosition(guest));

    // Make sure mouse events are sent completely before proceeding, in order to
    // avoid races with subsequent keyboard events.
    content::InputEventAckWaiter mouse_waiter(
        GetPluginFrame(guest)->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseUp);
    mouse_waiter.Wait();
  }

  void TypeHello(MimeHandlerViewGuest* guest) {
    struct KeyData {
      char ch;
      ui::DomCode code;
      ui::KeyboardCode key_code;
    };

    constexpr KeyData kData[] = {
        {'H', ui::DomCode::US_H, ui::VKEY_H},
        {'E', ui::DomCode::US_E, ui::VKEY_E},
        {'L', ui::DomCode::US_L, ui::VKEY_L},
        {'L', ui::DomCode::US_L, ui::VKEY_L},
        {'O', ui::DomCode::US_O, ui::VKEY_O},
    };

    content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
    // Make sure that the plugin frame of guest has focus.
    ASSERT_EQ(GetActiveWebContents()->GetFocusedFrame(), plugin_frame);
    for (const auto& data : kData) {
      content::SimulateKeyPress(guest->embedder_web_contents(),
                                ui::DomKey::FromCharacter(data.ch), data.code,
                                data.key_code, /*control=*/false,
                                /*shift=*/false, /*alt=*/false,
                                /*command=*/false);
      content::InputEventAckWaiter key_waiter(
          plugin_frame->GetRenderWidgetHost(),
          blink::WebInputEvent::Type::kKeyUp);
      key_waiter.Wait();
    }
  }

  // Presses the left arrow key.
  void PressLeftArrow(MimeHandlerViewGuest* guest) {
    // Make sure that the plugin frame of guest has focus.
    ASSERT_EQ(GetActiveWebContents()->GetFocusedFrame(), GetPluginFrame(guest));
    content::SimulateKeyPressWithoutChar(
        guest->embedder_web_contents(), ui::DomKey::ARROW_LEFT,
        ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT, false, false, false, false);
  }

  // Presses down shift, presses the left arrow, and lets go of shift.
  void PressShiftLeftArrow(MimeHandlerViewGuest* guest) {
    // Make sure that the plugin frame of guest has focus.
    ASSERT_EQ(GetActiveWebContents()->GetFocusedFrame(), GetPluginFrame(guest));
    content::SimulateKeyPressWithoutChar(guest->embedder_web_contents(),
                                         ui::DomKey::ARROW_LEFT,
                                         ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT,
                                         false, /*shift=*/true, false, false);
  }

  // Presses the right arrow key.
  void PressRightArrow(MimeHandlerViewGuest* guest) {
    // Make sure that the plugin frame of guest has focus.
    ASSERT_EQ(GetActiveWebContents()->GetFocusedFrame(), GetPluginFrame(guest));
    content::SimulateKeyPressWithoutChar(
        guest->embedder_web_contents(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, false, false, false);
  }

  // Presses down shift, presses the right arrow, and lets go of shift.
  void PressShiftRightArrow(MimeHandlerViewGuest* guest) {
    // Make sure that the plugin frame of guest has focus.
    ASSERT_EQ(GetActiveWebContents()->GetFocusedFrame(), GetPluginFrame(guest));
    content::SimulateKeyPressWithoutChar(
        guest->embedder_web_contents(), ui::DomKey::ARROW_RIGHT,
        ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false, /*shift=*/true, false,
        false);
  }
};

class PDFExtensionSaveTest : public PDFExtensionComboBoxTest {
 public:
  void SetUpOnMainThread() override {
    PDFExtensionComboBoxTest::SetUpOnMainThread();

    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void SaveEditedPdf(MimeHandlerViewGuest* guest) {
    ASSERT_TRUE(content::ExecuteScript(
        guest->GetGuestMainFrame(),
        "var viewer = document.getElementById('viewer');"
        "var toolbar = viewer.shadowRoot.getElementById('toolbar');"
        "var downloads = toolbar.shadowRoot.getElementById('downloads');"
        "downloads.shadowRoot.getElementById('download-edited').click();"));
  }

  void WaitForSavedPdf(const base::FilePath& path) {
    while (!base::PathExists(path))
      content::RunAllTasksUntilIdle();
  }

  base::FilePath GetDownloadDir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Flaky, http://crbug.com/1269103
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Save DISABLED_Save
#else
#define MAYBE_Save Save
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionSaveTest, MAYBE_Save) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath save_path = GetDownloadDir().AppendASCII("edited.pdf");
  ASSERT_FALSE(base::PathExists(save_path));

  using FileChooser = extensions::FileSystemChooseEntryFunction;
  FileChooser::SkipPickerAndAlwaysSelectPathForTest file_picker(save_path);

  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();
  ClickLeftSideOfEditableComboBox(guest);
  TypeHello(guest);
  SaveEditedPdf(guest);
  WaitForSavedPdf(save_path);
}

class PDFExtensionSaveWithPolicyTest : public PDFExtensionSaveTest {
 public:
  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetDownloadPolicyManagedPath(const base::FilePath& path) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDownloadDirectory,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(path.AsUTF8Unsafe()),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  void CreateConflictingFilenames(const base::FilePath& path, int count) {
    for (int i = 0; i < count; ++i) {
      base::FilePath unique_path = base::GetUniquePath(path);
      ASSERT_TRUE(!unique_path.empty());
      ASSERT_TRUE(base::WriteFile(unique_path, ""));
    }
  }

  int CountPdfFilesInDir(const base::FilePath& dir) {
    base::FileEnumerator file_enumerator(dir,
                                         /*recursive=*/false,
                                         base::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.pdf"));

    int count = 0;
    for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
         file_path = file_enumerator.Next()) {
      ++count;
    }
    return count;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Flaky, http://crbug.com/1269103
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SaveWithPolicy DISABLED_SaveWithPolicy
#else
#define MAYBE_SaveWithPolicy SaveWithPolicy
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionSaveWithPolicyTest, MAYBE_SaveWithPolicy) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath save_path = GetDownloadDir().AppendASCII("combobox_form.pdf");
  ASSERT_FALSE(base::PathExists(save_path));

  SetDownloadPolicyManagedPath(GetDownloadDir());
  DownloadPrefs::FromBrowserContext(profile())
      ->SkipSanitizeDownloadTargetPathForTesting();

  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();
  ClickLeftSideOfEditableComboBox(guest);
  TypeHello(guest);
  SaveEditedPdf(guest);
  WaitForSavedPdf(save_path);
}

// Flaky, http://crbug.com/1269103
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SaveWithPolicyUniqueNumberSuffix \
  DISABLED_SaveWithPolicyUniqueNumberSuffix
#else
#define MAYBE_SaveWithPolicyUniqueNumberSuffix SaveWithPolicyUniqueNumberSuffix
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionSaveWithPolicyTest,
                       MAYBE_SaveWithPolicyUniqueNumberSuffix) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CreateConflictingFilenames(GetDownloadDir().AppendASCII("combobox_form.pdf"),
                             5);
  EXPECT_EQ(5, CountPdfFilesInDir(GetDownloadDir()));

  base::FilePath save_path =
      GetDownloadDir().AppendASCII("combobox_form (5).pdf");
  ASSERT_FALSE(base::PathExists(save_path));

  SetDownloadPolicyManagedPath(GetDownloadDir());
  DownloadPrefs::FromBrowserContext(profile())
      ->SkipSanitizeDownloadTargetPathForTesting();

  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();
  ClickLeftSideOfEditableComboBox(guest);
  TypeHello(guest);
  SaveEditedPdf(guest);
  WaitForSavedPdf(save_path);
}

// TODO(crbug.com/1269103): Make this test non-flaky.
IN_PROC_BROWSER_TEST_F(PDFExtensionSaveWithPolicyTest,
                       DISABLED_SaveWithPolicyUniqueTimeSuffix) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CreateConflictingFilenames(GetDownloadDir().AppendASCII("combobox_form.pdf"),
                             101);
  EXPECT_EQ(101, CountPdfFilesInDir(GetDownloadDir()));

  SetDownloadPolicyManagedPath(GetDownloadDir());
  DownloadPrefs::FromBrowserContext(profile())
      ->SkipSanitizeDownloadTargetPathForTesting();

  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();
  ClickLeftSideOfEditableComboBox(guest);
  TypeHello(guest);
  SaveEditedPdf(guest);
  while (CountPdfFilesInDir(GetDownloadDir()) != 102)
    content::RunAllTasksUntilIdle();
}

class PDFExtensionClipboardTest : public PDFExtensionComboBoxTest,
                                  public ui::ClipboardObserver {
 public:
  // PDFExtensionTest:
  void SetUpOnMainThread() override {
    PDFExtensionTest::SetUpOnMainThread();
    ui::TestClipboard::CreateForCurrentThread();
  }
  void TearDownOnMainThread() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    PDFExtensionTest::TearDownOnMainThread();
  }

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override {
    DCHECK(!clipboard_changed_);
    clipboard_changed_ = true;
    std::move(clipboard_quit_closure_).Run();
  }

  // Runs `action` and checks the Linux selection clipboard contains `expected`.
  void DoActionAndCheckSelectionClipboard(base::OnceClosure action,
                                          const std::string& expected) {
    if (ui::Clipboard::IsSupportedClipboardBuffer(
            ui::ClipboardBuffer::kSelection)) {
      DoActionAndCheckClipboard(std::move(action),
                                ui::ClipboardBuffer::kSelection, expected);
    } else {
      // Even though there is no selection clipboard to check, `action` still
      // needs to run.
      std::move(action).Run();
    }
  }

  // Sends a copy command and checks the copy/paste clipboard.
  // Note: Trying to send ctrl+c does not work correctly with
  // SimulateKeyPress(). Using IDC_COPY does not work on Mac in browser_tests.
  void SendCopyCommandAndCheckCopyPasteClipboard(MimeHandlerViewGuest* guest,
                                                 const std::string& expected) {
    DoActionAndCheckClipboard(base::BindLambdaForTesting([&]() {
                                guest->embedder_web_contents()->Copy();
                              }),
                              ui::ClipboardBuffer::kCopyPaste, expected);
  }

 private:
  // Runs `action` and checks `clipboard_buffer` contains `expected`.
  void DoActionAndCheckClipboard(base::OnceClosure action,
                                 ui::ClipboardBuffer clipboard_buffer,
                                 const std::string& expected) {
    ASSERT_FALSE(clipboard_quit_closure_);

    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
    EXPECT_FALSE(clipboard_changed_);
    clipboard_changed_ = false;

    base::RunLoop run_loop;
    clipboard_quit_closure_ = run_loop.QuitClosure();
    std::move(action).Run();
    run_loop.Run();
    EXPECT_FALSE(clipboard_quit_closure_);

    EXPECT_TRUE(clipboard_changed_);
    clipboard_changed_ = false;
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);

    auto* clipboard = ui::Clipboard::GetForCurrentThread();
    std::string clipboard_data;
    clipboard->ReadAsciiText(clipboard_buffer, /* data_dst=*/nullptr,
                             &clipboard_data);
    EXPECT_EQ(expected, clipboard_data);
  }

  base::RepeatingClosure clipboard_quit_closure_;
  bool clipboard_changed_ = false;
};

// TODO(crbug.com/1121446): Fix flakiness.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_IndividualShiftRightArrowPresses \
  DISABLED_IndividualShiftRightArrowPresses
#else
#define MAYBE_IndividualShiftRightArrowPresses IndividualShiftRightArrowPresses
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       MAYBE_IndividualShiftRightArrowPresses) {
  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox(guest);

  TypeHello(guest);

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox(guest);

  // Press shift + right arrow 3 times. Letting go of shift in between.
  auto action =
      base::BindLambdaForTesting([&]() { PressShiftRightArrow(guest); });
  DoActionAndCheckSelectionClipboard(action, "H");
  DoActionAndCheckSelectionClipboard(action, "HE");
  DoActionAndCheckSelectionClipboard(action, "HEL");
  SendCopyCommandAndCheckCopyPasteClipboard(guest, "HEL");
}

// TODO(crbug.com/897801): test is flaky.
IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       DISABLED_IndividualShiftLeftArrowPresses) {
  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox(guest);

  TypeHello(guest);

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox(guest);

  for (int i = 0; i < 3; ++i)
    PressRightArrow(guest);

  // Press shift + left arrow 2 times. Letting go of shift in between.
  auto action =
      base::BindLambdaForTesting([&]() { PressShiftLeftArrow(guest); });
  DoActionAndCheckSelectionClipboard(action, "L");
  DoActionAndCheckSelectionClipboard(action, "EL");
  SendCopyCommandAndCheckCopyPasteClipboard(guest, "EL");

  // Press shift + left arrow 2 times. Letting go of shift in between.
  DoActionAndCheckSelectionClipboard(action, "HEL");
  DoActionAndCheckSelectionClipboard(action, "HEL");
  SendCopyCommandAndCheckCopyPasteClipboard(guest, "HEL");
}

// Flaky, http://crbug.com/1121446, http://crbug.com/1350332
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_CombinedShiftRightArrowPresses \
  DISABLED_CombinedShiftRightArrowPresses
#else
#define MAYBE_CombinedShiftRightArrowPresses CombinedShiftRightArrowPresses
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       MAYBE_CombinedShiftRightArrowPresses) {
  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox(guest);

  TypeHello(guest);

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox(guest);

  // Press shift + right arrow 3 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        guest->embedder_web_contents(), false, true, false, false);
    auto action = base::BindLambdaForTesting([&]() {
      hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                     ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    });
    DoActionAndCheckSelectionClipboard(action, "H");
    DoActionAndCheckSelectionClipboard(action, "HE");
    DoActionAndCheckSelectionClipboard(action, "HEL");
  }
  SendCopyCommandAndCheckCopyPasteClipboard(guest, "HEL");
}

// Flaky on multiple platforms (https://crbug.com/1121446)
IN_PROC_BROWSER_TEST_F(PDFExtensionClipboardTest,
                       DISABLED_CombinedShiftArrowPresses) {
  MimeHandlerViewGuest* guest = LoadTestComboBoxPdfGetMimeHandlerView();

  // Give the editable combo box focus.
  ClickLeftSideOfEditableComboBox(guest);

  TypeHello(guest);

  // Put the cursor back to the left side of the combo box.
  ClickLeftSideOfEditableComboBox(guest);

  for (int i = 0; i < 3; ++i)
    PressRightArrow(guest);

  // Press shift + left arrow 3 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        guest->embedder_web_contents(), false, true, false, false);
    auto action = base::BindLambdaForTesting([&]() {
      hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_LEFT,
                                     ui::DomCode::ARROW_LEFT, ui::VKEY_LEFT);
    });
    DoActionAndCheckSelectionClipboard(action, "L");
    DoActionAndCheckSelectionClipboard(action, "EL");
    DoActionAndCheckSelectionClipboard(action, "HEL");
  }
  SendCopyCommandAndCheckCopyPasteClipboard(guest, "HEL");

  // Press shift + right arrow 2 times. Holding down shift in between.
  {
    content::ScopedSimulateModifierKeyPress hold_shift(
        guest->embedder_web_contents(), false, true, false, false);
    auto action = base::BindLambdaForTesting([&]() {
      hold_shift.KeyPressWithoutChar(ui::DomKey::ARROW_RIGHT,
                                     ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT);
    });
    DoActionAndCheckSelectionClipboard(action, "EL");
    DoActionAndCheckSelectionClipboard(action, "L");
  }
  SendCopyCommandAndCheckCopyPasteClipboard(guest, "L");
}

// Verifies that an <embed> of size zero will still instantiate a guest and post
// message to the <embed> is correctly forwarded to the extension. This is for
// catching future regression in docs/ and slides/ pages (see
// https://crbug.com/763812).
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PostMessageForZeroSizedEmbed) {
  content::DOMMessageQueue queue(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/pdf/post_message_zero_sized_embed.html")));
  std::string message;
  EXPECT_TRUE(queue.WaitForMessage(&message));
  EXPECT_EQ("\"POST_MESSAGE_OK\"", message);
}

// In response to the events sent in |send_events|, ensures the PDF viewer zooms
// in and that the viewer's custom pinch zooming mechanism is used to do so.
void EnsureCustomPinchZoomInvoked(content::RenderFrameHost* guest_mainframe,
                                  WebContents* contents,
                                  base::OnceClosure send_events) {
  constexpr char kListenPinchUpdate[] = R"(
      const gestureDetector = viewer.viewport.getGestureDetectorForTesting();
      const updatePromise = new Promise((resolve) => {
        gestureDetector.getEventTarget().addEventListener('pinchupdate',
                                                          resolve);
      });
  )";
  ASSERT_TRUE(content::ExecuteScript(guest_mainframe, kListenPinchUpdate));

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
      guest_mainframe,
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
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  base::OnceClosure send_pinch = base::BindOnce(
      [](content::RenderFrameHost* guest_mainframe) {
        const gfx::Rect guest_rect =
            guest_mainframe->GetView()->GetViewBounds();
        const gfx::Point mouse_position(guest_rect.width() / 2,
                                        guest_rect.height() / 2);
        content::SimulateGesturePinchSequence(
            guest_mainframe->GetRenderWidgetHost(), mouse_position, 1.23,
            blink::WebGestureDevice::kTouchpad);
      },
      guest_mainframe);

  EnsureCustomPinchZoomInvoked(guest_mainframe, GetActiveWebContents(),
                               std::move(send_pinch));
}

#if !BUILDFLAG(IS_MAC)
// Ensure that ctrl-wheel events are handled by the PDF viewer.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, CtrlWheelInvokesCustomZoom) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  base::OnceClosure send_ctrl_wheel = base::BindOnce(
      [](content::RenderFrameHost* guest_mainframe) {
        const gfx::Rect guest_rect =
            guest_mainframe->GetView()->GetViewBounds();
        const gfx::Point mouse_position(guest_rect.width() / 2,
                                        guest_rect.height() / 2);
        content::SimulateMouseWheelCtrlZoomEvent(
            guest_mainframe->GetRenderWidgetHost(), mouse_position, true,
            blink::WebMouseWheelEvent::kPhaseBegan);
      },
      guest_mainframe);

  EnsureCustomPinchZoomInvoked(guest_mainframe, GetActiveWebContents(),
                               std::move(send_ctrl_wheel));
}

// Flaky on ChromeOS (https://crbug.com/922974)
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TouchscreenPinchInvokesCustomZoom \
  DISABLED_TouchscreenPinchInvokesCustomZoom
#else
#define MAYBE_TouchscreenPinchInvokesCustomZoom \
  TouchscreenPinchInvokesCustomZoom
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionTest,
                       MAYBE_TouchscreenPinchInvokesCustomZoom) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  base::OnceClosure send_touchscreen_pinch = base::BindOnce(
      [](WebContents* web_contents, content::RenderFrameHost* guest_mainframe) {
        const gfx::Rect guest_rect =
            guest_mainframe->GetView()->GetViewBounds();
        const gfx::PointF anchor_position(guest_rect.width() / 2,
                                          guest_rect.height() / 2);
        base::RunLoop run_loop;
        content::SimulateTouchscreenPinch(web_contents, anchor_position, 1.2f,
                                          run_loop.QuitClosure());
        run_loop.Run();
      },
      GetActiveWebContents(), guest_mainframe);

  EnsureCustomPinchZoomInvoked(guest_mainframe, GetActiveWebContents(),
                               std::move(send_touchscreen_pinch));
}

#endif  // !BUILDFLAG(IS_MAC)

using PDFExtensionHitTestTest = PDFExtensionTest;

// Flaky in nearly all configurations; see https://crbug.com/856169.
IN_PROC_BROWSER_TEST_F(PDFExtensionHitTestTest, DISABLED_MouseLeave) {
  // Load page with embedded PDF and make sure it succeeds.
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  gfx::Point point_in_parent(250, 25);
  gfx::Point point_in_pdf(250, 250);

  // Inject script to count MouseLeaves in the PDF.
  ASSERT_TRUE(content::ExecuteScript(
      guest_mainframe,
      "var enter_count = 0;\n"
      "var leave_count = 0;\n"
      "document.addEventListener('mouseenter', function (){\n"
      "  enter_count++;"
      "});\n"
      "document.addEventListener('mouseleave', function (){\n"
      "  leave_count++;"
      "});"));

  // Inject some MouseMoves to invoke a MouseLeave in the PDF.
  WebContents* embedder_contents = GetActiveWebContents();
  content::SimulateMouseEvent(embedder_contents,
                              blink::WebInputEvent::Type::kMouseMove,
                              point_in_parent);
  content::SimulateMouseEvent(
      embedder_contents, blink::WebInputEvent::Type::kMouseMove, point_in_pdf);
  content::SimulateMouseEvent(embedder_contents,
                              blink::WebInputEvent::Type::kMouseMove,
                              point_in_parent);

  // Verify MouseEnter, MouseLeave received.
  int leave_count = 0;
  do {
    ASSERT_TRUE(ExecuteScriptAndExtractInt(
        guest_mainframe, "window.domAutomationController.send(leave_count);",
        &leave_count));
  } while (!leave_count);
  int enter_count = 0;
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      guest_mainframe, "window.domAutomationController.send(enter_count);",
      &enter_count));
  EXPECT_EQ(1, enter_count);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionHitTestTest, ContextMenuCoordinates) {
  // Load page with embedded PDF and make sure it succeeds.
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  // Observe context menu IPC.
  content::RenderFrameHost* plugin_frame = GetPluginFrame(guest);
  content::ContextMenuInterceptor context_menu_interceptor(plugin_frame);

  ContextMenuWaiter menu_observer;

  // Send mouse right-click to activate context menu.
  gfx::Point context_menu_position(80, 130);
  content::SimulateMouseClickAt(GetActiveWebContents(), kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::kRight,
                                context_menu_position);

  // We expect the context menu, invoked via the RenderFrameHost, to be using
  // root view coordinates.
  menu_observer.WaitForMenuOpenAndClose();
  ASSERT_EQ(context_menu_position.x(), menu_observer.params().x);
  ASSERT_EQ(context_menu_position.y(), menu_observer.params().y);

  // We expect the IPC, received from the renderer, to be using local coords.
  context_menu_interceptor.Wait();
  blink::UntrustworthyContextMenuParams params =
      context_menu_interceptor.get_params();
  gfx::Point received_context_menu_position =
      plugin_frame->GetRenderWidgetHost()
          ->GetView()
          ->TransformPointToRootCoordSpace({params.x, params.y});
  EXPECT_EQ(context_menu_position, received_context_menu_position);

  // TODO(wjmaclean): If it ever becomes possible to filter outgoing IPCs from
  // the RenderProcessHost, we should verify the blink.mojom.PluginActionAt
  // message is sent with the same coordinates as in the
  // UntrustworthyContextMenuParams.
}

// The plugin document and the mime handler should both use the same background
// color.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BackgroundColor) {
  // The background color for plugins is injected when the first response
  // is intercepted, at which point not all the plugins have loaded. This line
  // ensures that the PDF plugin has loaded and the right background color is
  // beign used.
  WaitForPluginServiceToLoad();
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);
  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  const std::string script =
      "window.domAutomationController.send("
      "    window.getComputedStyle(document.body, null)."
      "    getPropertyValue('background-color'))";
  std::string outer;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(GetActiveWebContents(),
                                                     script, &outer));
  std::string inner;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(guest_mainframe, script, &inner));
  EXPECT_EQ(inner, outer);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DefaultFocusForEmbeddedPDF) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/pdf_embed.html"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  // Verify that current focus state is body element.
  const std::string script =
      "const is_plugin_focused = document.activeElement === "
      "document.body;"
      "window.domAutomationController.send(is_plugin_focused);";

  bool result = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(guest_mainframe, script, &result));
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DefaultFocusForNonEmbeddedPDF) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  // Verify that current focus state is document element.
  const std::string script =
      "const is_plugin_focused = document.activeElement === "
      "document.body;"
      "window.domAutomationController.send(is_plugin_focused);";

  bool result = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(guest_mainframe, script, &result));
  ASSERT_TRUE(result);
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

  RequestWaiter(const RequestWaiter&) = delete;
  RequestWaiter& operator=(const RequestWaiter&) = delete;

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
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL(
          "/pdf/pdf_embed_with_hung_sibling_subframe.html"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);  // Don't wait for completion.

  // Wait for the request for the MimeHandlerView extension.  Afterwards, the
  // main page should be still loading because of
  // 1) the MimeHandlerView frame is loading
  // 2) the hung subframe is loading.
  interceptor.WaitForRequest();

  // Remove the hung subframe.  Afterwards the main page should stop loading as
  // soon as the MimeHandlerView frame stops loading (assumming we have not bugs
  // similar to https://crbug.com/964364).
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      web_contents, "document.getElementById('hung_subframe').remove();"));

  // MAIN VERIFICATION: Wait for the main frame to report that is has stopped
  // loading.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
}

// This test verifies that it is possible to add an <embed src=pdf> element into
// a new popup window when using document.write.  See also
// https://crbug.com/1041880.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DocumentWriteIntoNewPopup) {
  // Navigate to an empty/boring test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // Open a new popup and call document.write to add an embedded PDF.
  WebContents* popup = nullptr;
  {
    const char kScriptTemplate[] = R"(
        const url = $1;
        const html = '<embed type="application/pdf" src="' + url + '">';

        const popup = window.open('', '_blank');
        popup.document.write(html);
    )";
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(content::ExecJs(
        GetActiveWebContents(),
        content::JsReplace(kScriptTemplate,
                           embedded_test_server()->GetURL("/pdf/test.pdf"))));
    popup = popup_observer.GetWebContents();
  }

  // Verify the PDF loaded successfully.
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(popup));
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LoadPdfFromExtension) {
  const extensions::Extension* test_extension = LoadExtension(
      GetTestResourcesParentDir().AppendASCII("pdf/extension_with_pdf"));
  ASSERT_TRUE(test_extension);

  EXPECT_TRUE(LoadPdf(test_extension->GetResourceURL("test.pdf")));
}

// Tests that the PDF extension loads in the presence of an extension that, on
// the completion of document loading, adds an <iframe> to the body element.
// https://bugs.chromium.org/p/chromium/issues/detail?id=1046795
IN_PROC_BROWSER_TEST_F(PDFExtensionTest,
                       PdfLoadsWithExtensionThatInjectsFrame) {
  const extensions::Extension* test_extension = LoadExtension(
      GetTestResourcesParentDir().AppendASCII("pdf/extension_injects_iframe"));
  ASSERT_TRUE(test_extension);

  EXPECT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Metrics) {
  base::HistogramTester histograms;
  base::UserActionTester actions;

  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/combobox_form.pdf"));
  ASSERT_TRUE(guest);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Histograms.
  // Duplicating some constants to avoid reaching into pdf/ internals.
  constexpr int kAcroForm = 1;
  constexpr int k1_7 = 8;
  histograms.ExpectUniqueSample("PDF.FormType", kAcroForm, 1);
  histograms.ExpectUniqueSample("PDF.Version", k1_7, 1);
  histograms.ExpectUniqueSample("PDF.HasAttachment", 0, 1);

  // Custom histograms.
  histograms.ExpectUniqueSample("PDF.PageCount", 1, 1);

  // User actions.
  EXPECT_EQ(1, actions.GetActionCount("PDF.LoadSuccess"));
}

// Flaky. See https://crbug.com/1101514.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DISABLED_TabInAndOutOfPDFPlugin) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  // Set focus on last toolbar element (zoom-out-button).
  ASSERT_TRUE(
      content::ExecuteScript(guest_mainframe,
                             R"(viewer.shadowRoot.querySelector('#zoomToolbar')
         .$['zoom-out-button']
         .$$('cr-icon-button')
         .focus();)"));

  // The script will ensure we return the the focused element on focus.
  const char kScript[] = R"(
    const plugin = viewer.shadowRoot.querySelector('#plugin');
    plugin.addEventListener('focus', () => {
      window.domAutomationController.send('plugin');
    });

    const button = viewer.shadowRoot.querySelector('#zoomToolbar')
                   .$['zoom-out-button']
                   .$$('cr-icon-button');
    button.addEventListener('focus', () => {
      window.domAutomationController.send('zoom-out-button');
    });
  )";
  ASSERT_TRUE(content::ExecuteScript(guest_mainframe, kScript));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [guest_mainframe, this](bool reverse) {
    content::DOMMessageQueue msg_queue(guest_mainframe);
    std::string reply;
    SimulateKeyPress(GetActiveWebContents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, /*shift=*/reverse, false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> and ensure that PDF document receives focus.
  EXPECT_EQ("\"plugin\"", press_tab_and_wait_for_message(false));
  // Press <shift-tab> and ensure that last toolbar element (zoom-out-button)
  // receives focus.
  EXPECT_EQ("\"zoom-out-button\"", press_tab_and_wait_for_message(true));
}

class PDFExtensionPrerenderTest : public PDFExtensionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);
    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because PDFExtensionTest also uses a ScopedFeatureList
    // and initialization order matters.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&PDFExtensionPrerenderTest::GetActiveWebContents,
                            base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    prerender_helper_->SetUp(embedded_test_server());
    PDFExtensionTest::SetUpOnMainThread();
  }

 protected:
  void PrerenderAndExpectCancellation(const GURL& prerender_url) {
    content::test::PrerenderHostObserver observer(*GetActiveWebContents(),
                                                  prerender_url);
    prerender_helper().AddPrerenderAsync(prerender_url);
    observer.WaitForDestroyed();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// TODO(1205920): The PDF viewer cannot currently be prerendered correctly. This
// tests that prerendering is cancelled. Once we're able to support this, this
// test should be replaced with one that prerenders the PDF viewer.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderTest, CancelPrerender) {
  const GURL initial_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL pdf_url =
      embedded_test_server()->GetURL("a.test", "/pdf/test.pdf");
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  PrerenderAndExpectCancellation(pdf_url);
  EXPECT_EQ(0U, GetGuestViewManager()->num_guests_created());

  prerender_helper().NavigatePrimaryPage(pdf_url);
  ASSERT_EQ(web_contents->GetLastCommittedURL(), pdf_url);
  EXPECT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
}

// TODO(1205920): The PDF viewer cannot currently be prerendered correctly. This
// tests that prerendering is cancelled if a PDF is embedded in a prerendered
// page. Once we're able to support this, this test should be replaced with one
// that prerenders the PDF viewer.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderTest,
                       CancelPrerenderWithEmbeddedPdf) {
  const GURL initial_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL pdf_url =
      embedded_test_server()->GetURL("a.test", "/pdf/test-iframe.html");
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  PrerenderAndExpectCancellation(pdf_url);
  EXPECT_EQ(0U, GetGuestViewManager()->num_guests_created());

  prerender_helper().NavigatePrimaryPage(pdf_url);
  ASSERT_EQ(web_contents->GetLastCommittedURL(), pdf_url);
  EXPECT_TRUE(GetGuestViewManager()->WaitForSingleGuestViewCreated());
}

// Cross-origin subframe navigations are deferred during prerendering, which
// means that an embedded cross-site PDF will not cause the PDF viewer to be
// created until prerender activation.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderTest,
                       PrerenderWithCrossSiteEmbeddedPdf) {
  const GURL initial_url =
      embedded_test_server()->GetURL("a.test", "/empty.html");
  const GURL pdf_url = embedded_test_server()->GetURL(
      "a.test", "/pdf/test-cross-site-iframe.html");
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::test::PrerenderHostRegistryObserver registry_observer(*web_contents);
  prerender_helper().AddPrerenderAsync(pdf_url);
  registry_observer.WaitForTrigger(pdf_url);
  const auto host_id = prerender_helper().GetHostForUrl(pdf_url);
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  content::test::PrerenderHostObserver prerender_observer(*web_contents,
                                                          host_id);
  prerender_helper().NavigatePrimaryPage(pdf_url);
  prerender_observer.WaitForActivation();
  ASSERT_EQ(web_contents->GetLastCommittedURL(), pdf_url);
  EXPECT_TRUE(GetGuestViewManager()->WaitForSingleGuestViewCreated());
}

class PDFExtensionSubmitFormTest : public PDFExtensionTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
        [this](const net::test_server::HttpRequest& request) {
          if (request.relative_url != "/pdf/test_endpoint")
            return;

          EXPECT_EQ(request.method, net::test_server::METHOD_POST);
          EXPECT_THAT(request.content, StartsWith("\%FDF"));
          ASSERT_TRUE(quit_closure_);
          std::move(quit_closure_).Run();
        }));

    PDFExtensionTest::SetUpOnMainThread();
  }

 protected:
  // Retrieves a `base::RunLoop` and saves its `QuitClosure()`. The test
  // monitors HTTP requests on the IO thread, so `quit_closure_` needs to be set
  // up on the UI thread before the requests can arrive.
  std::unique_ptr<base::RunLoop> CreateFormSubmissionRunLoop() {
    auto run_loop = std::make_unique<base::RunLoop>();
    EXPECT_FALSE(quit_closure_);
    quit_closure_ = run_loop->QuitClosure();
    return run_loop;
  }

 private:
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionSubmitFormTest, SubmitForm) {
  MimeHandlerViewGuest* guest = LoadPdfGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/submit_form.pdf"));
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);

  std::unique_ptr<base::RunLoop> run_loop = CreateFormSubmissionRunLoop();

  // Click on the "Submit Form" button.
  SimulateMouseClickAt(
      guest, blink::WebInputEvent::kNoModifiers,
      blink::WebMouseEvent::Button::kLeft,
      ConvertPageCoordToScreenCoord(guest_mainframe, {200, 200}));

  run_loop->Run();
}

class PDFExtensionPrerenderAndFencedFrameTest : public PDFExtensionTest {
 public:
  PDFExtensionPrerenderAndFencedFrameTest() = default;
  ~PDFExtensionPrerenderAndFencedFrameTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTest::SetUpCommandLine(command_line);
    // `prerender_helper_` and `fenced_frame_helper_` has a ScopedFeatureList so
    // we needed to delay its creation until now because PDFExtensionTest
    // also uses a ScopedFeatureList and initialization order matters.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&PDFExtensionPrerenderTest::GetActiveWebContents,
                            base::Unretained(this)));
    fenced_frame_helper_ =
        std::make_unique<content::test::FencedFrameTestHelper>();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_;
  }

  content::test::FencedFrameTestHelper& fenced_frame_helper() {
    return *fenced_frame_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  std::unique_ptr<content::test::FencedFrameTestHelper> fenced_frame_helper_;
};

// TODO(1205920): The PDF viewer cannot currently be prerendered correctly. Once
// this is supported, this test should be re-enabled.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderAndFencedFrameTest,
                       DISABLED_LoadPDFInPrerender) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());
  // Set a 1s delay to delay MimeHandlerViewGuest's creation to ensure that the
  // fenced frame is loaded while the PDF stream is not yet consumed.
  const int creation_delay = TestTimeouts::tiny_timeout().InMilliseconds();
  TestMimeHandlerViewGuest::DelayNextCreateWebContents(creation_delay);

  // Load a PDF in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/pdf/test.pdf");

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  prerender_helper().AddPrerenderAsync(prerender_url);
  registry_observer.WaitForTrigger(prerender_url);

  // Create a fenced frame.
  const GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_helper().CreateFencedFrame(
          GetActiveWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Ensure that the fenced frame's navigation should not abort the PDF stream.
  EXPECT_EQ(1U, GetGuestViewManager()->GetNumGuestsActive());
}

// Test that ensures we cannot navigate a fenced frame to a PDF because PDF
// isn't allowed by default static sandbox flags of fenced frames.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderAndFencedFrameTest,
                       LoadPdfInFencedFrame) {
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(), embedded_test_server()->GetURL("/empty.html")));

  // Create a fenced frame and try to navigate to a PDF.
  EXPECT_TRUE(fenced_frame_helper().CreateFencedFrame(
      GetActiveWebContents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("/pdf/test-fenced-frame.pdf"),
      net::Error::ERR_BLOCKED_BY_CLIENT));
  EXPECT_EQ(CountPDFProcesses(), 0);
}

// Like `LoadPdfInFencedFrame`, but without Supports-Loading-Mode headers set.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderAndFencedFrameTest,
                       LoadPdfInFencedFrameWithoutFencedFrameOptIn) {
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(), embedded_test_server()->GetURL("/empty.html")));

  // Create a fenced frame and try to navigate to a PDF.
  EXPECT_TRUE(fenced_frame_helper().CreateFencedFrame(
      GetActiveWebContents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("/pdf/test.pdf"),
      net::Error::ERR_BLOCKED_BY_RESPONSE));
  EXPECT_EQ(CountPDFProcesses(), 0);
}

// Test that ensures a fenced frame cannot load a document embedding a PDF
// because PDF isn't allowed in fenced frames.
IN_PROC_BROWSER_TEST_F(PDFExtensionPrerenderAndFencedFrameTest,
                       LoadEmbeddedPdfInFencedFrame) {
  ASSERT_TRUE(content::NavigateToURL(
      GetActiveWebContents(), embedded_test_server()->GetURL("/empty.html")));

  // Create a fenced frame for loading a document with pdf embed(s).
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_helper().CreateFencedFrame(
          GetActiveWebContents()->GetPrimaryMainFrame(),
          embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  ASSERT_TRUE(fenced_frame_host);

  const GURL pdf_url =
      embedded_test_server()->GetURL("/pdf/test-fenced-frame.pdf");
  // Ensure that the fenced frame cannot load a PDF embedding with <iframe>.
  ASSERT_TRUE(content::ExecJs(
      fenced_frame_host,
      content::JsReplace("let e = document.createElement('iframe');"
                         "e.src = $1;"
                         "e.type = 'application/pdf';"
                         "document.body.appendChild(e);",
                         pdf_url)));
  ASSERT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));
  EXPECT_EQ(CountPDFProcesses(), 0);

  // Ensure that the fenced frame cannot load a PDF embedding with <object>.
  ASSERT_TRUE(content::ExecJs(
      fenced_frame_host,
      content::JsReplace("let e = document.createElement('object');"
                         "e.data = $1;"
                         "e.type = 'application/pdf';"
                         "document.body.appendChild(e);",
                         pdf_url)));
  ASSERT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));
  EXPECT_EQ(CountPDFProcesses(), 0);

  // Ensure that the fenced frame cannot load a PDF embedding with <embed>.
  ASSERT_TRUE(content::ExecJs(
      fenced_frame_host,
      content::JsReplace("let e = document.createElement('embed');"
                         "e.src = $1;"
                         "e.type = 'application/pdf';"
                         "document.body.appendChild(e);",
                         pdf_url)));
  ASSERT_TRUE(content::WaitForLoadStop(GetActiveWebContents()));
  EXPECT_EQ(CountPDFProcesses(), 0);
}

// Exercise a race condition where the profile is destroyed in the middle of a
// PDF navigation and ensure that this doesn't crash.  Specifically,
// `PdfNavigationThrottle` intercepts PDF navigations to PDF stream URLs,
// cancels them, and posts a task to navigate to the original URL instead.
// Triggering profile destruction after this task is posted but before it runs
// has previously led to issues in https://crbug.com/1382761.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfNavigationDuringProfileShutdown) {
  // Open an Incognito window and navigate it to a page with a PDF embedded in
  // an iframe.
  Browser* incognito = CreateIncognitoBrowser();
  content::WebContents* incognito_contents =
      incognito->tab_strip_model()->GetActiveWebContents();
  incognito_contents->GetController().LoadURL(
      embedded_test_server()->GetURL("/pdf/test-cross-site-iframe.html"),
      content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Wait for the MimeHandleView guest to be created.  This should return
  // before the actual PDF navigation in the guest is started.
  GuestViewBase* guest_view = GetGuestViewManager(incognito->profile())
                                  ->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);

  // Look up the PDF stream URL to which the navigation will take place.
  extensions::MimeHandlerViewGuest* guest =
      extensions::MimeHandlerViewGuest::FromWebContents(
          guest_view->web_contents());
  ASSERT_TRUE(guest);
  base::WeakPtr<extensions::StreamContainer> stream = guest->GetStreamWeakPtr();
  EXPECT_TRUE(stream);
  GURL stream_url(stream->stream_url());

  // Use TestNavigationManager to wait for first yield after running
  // DidStartNavigation throttles.  This should be precisely after the
  // navigation to the stream URL gets canceled and the task to start a new
  // navigation to the original URL is scheduled.
  {
    content::TestNavigationManager manager(guest_view->web_contents(),
                                           stream_url);
    ASSERT_TRUE(manager.WaitForFirstYieldAfterDidStartNavigation());
  }

  // Now, close Incognito and destroy its profile.  This is subtle: simply
  // closing the Incognito window and waiting for browser destruction (e.g.,
  // with `ui_test_utils::WaitForBrowserToClose(incognito)`) will trigger
  // asynchronous profile destruction which will allow the PDF task to run
  // before profile destruction is complete, sidestepping the bug in
  // https://crbug.com/1382761.  Instead, use the hard shutdown/restart logic
  // similar to that in `BrowserCloseManager::CloseBrowsers()`, which is used
  // by `chrome::ExitIgnoreUnloadHandlers() and forces the `Browser` and its
  // profile shutdown to complete synchronously, but only on the Incognito
  // Browser object. Note that we can't just use
  // `chrome::ExitIgnoreUnloadHandlers()` here, as that shuts down all Browser
  // objects and the rest of the browser process and appears to be unsupported
  // in tests.
  chrome::CloseWindow(incognito);
  BrowserView* incognito_view = static_cast<BrowserView*>(incognito->window());
  incognito_view->DestroyBrowser();

  // The test succeeds if it doesn't crash when the posted PDF task attempts to
  // run (the task should be canceled/ignored), so wait for this to happen.
  base::RunLoop().RunUntilIdle();
}
