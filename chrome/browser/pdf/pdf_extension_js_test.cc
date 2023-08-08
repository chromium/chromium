// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/icu_test_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_time_zone.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

class PDFExtensionJSTest : public PDFExtensionTestBase {
 protected:
  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    // Load the pak file holding the resources served from chrome://webui-test.
    base::FilePath pak_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
    pak_path = pak_path.AppendASCII("browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::kScaleFactorNone);

    // Register the chrome://webui-test data source.
    webui::CreateAndAddWebUITestDataSource(browser()->profile());
  }

  void RunTestsInJsModule(const std::string& filename,
                          const std::string& pdf_filename) {
    RunTestsInJsModuleHelper(filename, pdf_filename, /*new_tab=*/false);
  }

  void RunTestsInJsModuleNewTab(const std::string& filename,
                                const std::string& pdf_filename) {
    RunTestsInJsModuleHelper(filename, pdf_filename, /*new_tab=*/true);
  }

 private:
  // Runs the extensions test at chrome/test/data/pdf/<filename> on the PDF file
  // at chrome/test/data/pdf/<pdf_filename>, where |filename| is loaded as a JS
  // module.
  void RunTestsInJsModuleHelper(const std::string& filename,
                                const std::string& pdf_filename,
                                bool new_tab) {
    extensions::ResultCatcher catcher;

    GURL url(embedded_test_server()->GetURL("/pdf/" + pdf_filename));

    // It should be good enough to just navigate to the URL. But loading up the
    // BrowserPluginGuest seems to happen asynchronously as there was flakiness
    // being seen due to the BrowserPluginGuest not being available yet (see
    // crbug.com/498077). So instead use LoadPdf() which ensures that the PDF is
    // loaded before continuing.
    extensions::MimeHandlerViewGuest* guest =
        new_tab ? LoadPdfInNewTabGetMimeHandlerView(url)
                : LoadPdfGetMimeHandlerView(url);
    ASSERT_TRUE(guest);

    constexpr char kModuleLoaderTemplate[] =
        R"(var s = document.createElement('script');
           s.type = 'module';
           s.src = 'chrome://%s/pdf/%s';
           s.onerror = function(e) {
             console.error('Error while loading', e.target.src);
           };
           document.body.appendChild(s);)";

    ASSERT_TRUE(content::ExecJs(
        guest->GetGuestMainFrame(),
        base::StringPrintf(kModuleLoaderTemplate,
                           chrome::kChromeUIWebUITestHost, filename.c_str())));

    if (!catcher.GetNextResult()) {
      FAIL() << catcher.message();
    }
  }
};

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Basic) {
  RunTestsInJsModule("basic_test.js", "test.pdf");
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, BasicPlugin) {
  RunTestsInJsModule("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, PluginController) {
  RunTestsInJsModule("plugin_controller_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Viewport) {
  RunTestsInJsModule("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewportScroller) {
  RunTestsInJsModule("viewport_scroller_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Layout3) {
  RunTestsInJsModule("layout_test.js", "test-layout3.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Layout4) {
  RunTestsInJsModule("layout_test.js", "test-layout4.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Bookmark) {
  RunTestsInJsModule("bookmarks_test.js", "test-bookmarks-with-zoom.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Navigator) {
  RunTestsInJsModule("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ParamsParser) {
  RunTestsInJsModule("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ZoomManager) {
  RunTestsInJsModule("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, GestureDetector) {
  RunTestsInJsModule("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, SwipeDetector) {
  RunTestsInJsModule("swipe_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, TouchHandling) {
  RunTestsInJsModule("touch_handling_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, DownloadControls) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("download_controls_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Title) {
  RunTestsInJsModule("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, WhitespaceTitle) {
  RunTestsInJsModule("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, PageChange) {
  RunTestsInJsModule("page_change_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ScrollWithFormFieldFocusedTest) {
  RunTestsInJsModule("scroll_with_form_field_focused_test.js",
                     "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Metrics) {
  RunTestsInJsModule("metrics_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerPasswordDialog) {
  RunTestsInJsModule("viewer_password_dialog_test.js", "encrypted.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ArrayBufferAllocator) {
  // Run several times to see if there are issues with unloading.
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerPdfSidenav) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_pdf_sidenav_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerThumbnailBar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerThumbnail) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerAttachmentBar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_attachment_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Fullscreen) {
  // Use a PDF document with multiple pages, to exercise navigating between
  // pages.
  RunTestsInJsModule("fullscreen_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerPropertiesDialog) {
  // The properties dialog formats some values based on locale.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale{"en_US"};
  // This will apply to the new processes spawned within RunTestsInJsModule(),
  // thus consistently running the test in a well known time zone.
  content::ScopedTimeZone scoped_time_zone{"America/Los_Angeles"};
  RunTestsInJsModule("viewer_properties_dialog_test.js", "document_info.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, PostMessageProxy) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("post_message_proxy_test.js", "test.pdf");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, Printing) {
  RunTestsInJsModule("printing_icon_test.js", "test.pdf");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_INK)
// TODO(https://crbug.com/920684): Test times out under sanitizers.
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(_DEBUG)
#define MAYBE_AnnotationsFeatureEnabled DISABLED_AnnotationsFeatureEnabled
#else
#define MAYBE_AnnotationsFeatureEnabled AnnotationsFeatureEnabled
#endif
IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, MAYBE_AnnotationsFeatureEnabled) {
  RunTestsInJsModule("annotations_feature_enabled_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, AnnotationsToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("annotations_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, ViewerToolbarDropdown) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_dropdown_test.js", "test.pdf");
}
#endif  // BUILDFLAG(ENABLE_INK)

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// TODO(crbug.com/1444895): Re-enable it when integrating PDF OCR with
// Select-to-Speak.
IN_PROC_BROWSER_TEST_F(PDFExtensionJSTest, DISABLED_PdfOcrToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("pdf_ocr_toolbar_test.js", "test.pdf");
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

class PDFExtensionContentSettingJSTest : public PDFExtensionJSTest {
 protected:
  // When blocking JavaScript, block the exact query from pdf/main.js while
  // still allowing enough JavaScript to run in the extension for the test
  // harness to complete its work.
  void SetPdfJavaScript(bool enabled) {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(),
        ContentSettingsPattern::FromString(
            "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai"),
        ContentSettingsType::JAVASCRIPT,
        enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  }
};

IN_PROC_BROWSER_TEST_F(PDFExtensionContentSettingJSTest, Beep) {
  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionContentSettingJSTest, NoBeep) {
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionContentSettingJSTest, BeepThenNoBeep) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModuleNewTab("nobeep_test.js", "test-beep.pdf");

  // Make sure there are two PDFs in the same process.
  const int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionContentSettingJSTest, NoBeepThenBeep) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
  SetPdfJavaScript(/*enabled=*/true);
  RunTestsInJsModuleNewTab("beep_test.js", "test-beep.pdf");

  // Make sure there are two PDFs in the same process.
  const int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionContentSettingJSTest, BeepCsp) {
  // The script-source * directive in the mock headers file should
  // allow the JavaScript to execute the beep().
  RunTestsInJsModule("beep_test.js", "test-beep-csp.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionContentSettingJSTest, DISABLED_NoBeepCsp) {
  // The script-source none directive in the mock headers file should
  // prevent the JavaScript from executing the beep().
  // TODO(https://crbug.com/1032511) functionality not implemented.
  RunTestsInJsModule("nobeep_test.js", "test-nobeep-csp.pdf");
}

class PDFExtensionWebUICodeCacheJSTest : public PDFExtensionJSTest {
 protected:
  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionJSTest::GetEnabledFeatures();
    enabled.push_back(features::kWebUICodeCache);
    return enabled;
  }
};

// Regression test for https://crbug.com/1239148.
IN_PROC_BROWSER_TEST_F(PDFExtensionWebUICodeCacheJSTest, Basic) {
  RunTestsInJsModule("basic_test.js", "test.pdf");
}

// Service worker tests are regression tests for
// https://crbug.com/916514.
class PDFExtensionServiceWorkerJSTest : public PDFExtensionJSTest {
 public:
  ~PDFExtensionServiceWorkerJSTest() override = default;

 protected:
  // Installs the specified service worker and tests navigating to a PDF in its
  // scope.
  void RunServiceWorkerTest(const std::string& worker_path) {
    // Install the service worker.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(GetActiveWebContents(),
                             "register('" + worker_path + "', '/pdf');"));

    // Navigate to a PDF in the service worker's scope. It should load.
    RunTestsInJsModule("basic_test.js", "test.pdf");
    EXPECT_EQ(1, CountPDFProcesses());
  }
};

// Test navigating to a PDF in the scope of a service worker with no fetch event
// handler.
IN_PROC_BROWSER_TEST_F(PDFExtensionServiceWorkerJSTest, NoFetchHandler) {
  RunServiceWorkerTest("empty.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// then falls back to network by not calling FetchEvent.respondWith().
IN_PROC_BROWSER_TEST_F(PDFExtensionServiceWorkerJSTest, NetworkFallback) {
  RunServiceWorkerTest("network_fallback_worker.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// provides a response.
IN_PROC_BROWSER_TEST_F(PDFExtensionServiceWorkerJSTest, Interception) {
  RunServiceWorkerTest("respond_with_fetch_worker.js");
}
