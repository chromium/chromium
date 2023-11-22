// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/icu_test_util.h"
#include "base/test/with_feature_override.h"
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
#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/test_switches.h"
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
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

class PDFExtensionJSTest : public base::test::WithFeatureOverride,
                           public PDFExtensionTestBase {
 public:
  PDFExtensionJSTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const override { return GetParam(); }

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

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
      base::FilePath devtools_code_coverage_dir =
          command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
      coverage_handler_ = std::make_unique<DevToolsAgentCoverageObserver>(
          devtools_code_coverage_dir);
    }
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

    bool result = content::ExecJs(
        guest->GetGuestMainFrame(),
        base::StringPrintf(kModuleLoaderTemplate,
                           chrome::kChromeUIWebUITestHost, filename.c_str()));

    if (coverage_handler_ && coverage_handler_->CoverageEnabled()) {
      const auto* test_info =
          ::testing::UnitTest::GetInstance()->current_test_info();
      const std::string full_test_name = base::StrCat(
          {test_info->test_suite_name(), test_info->test_case_name()});
      coverage_handler_->CollectCoverage(full_test_name);
    }

    ASSERT_TRUE(result);

    if (!catcher.GetNextResult()) {
      FAIL() << catcher.message();
    }
  }

  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;
};

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Basic) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("basic_test.js", "test.pdf");
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, BasicPlugin) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PluginController) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("plugin_controller_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Viewport) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewportScroller) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("viewport_scroller_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Layout3) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("layout_test.js", "test-layout3.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Layout4) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("layout_test.js", "test-layout4.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Bookmark) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("bookmarks_test.js", "test-bookmarks-with-zoom.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Navigator) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ParamsParser) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ZoomManager) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, GestureDetector) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, SwipeDetector) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("swipe_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, TouchHandling) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("touch_handling_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Elements) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, DownloadControls) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("download_controls_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Title) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, WhitespaceTitle) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PageChange) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("page_change_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ScrollWithFormFieldFocusedTest) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("scroll_with_form_field_focused_test.js",
                     "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Metrics) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("metrics_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPasswordDialog) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("viewer_password_dialog_test.js", "encrypted.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ArrayBufferAllocator) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Run several times to see if there are issues with unloading.
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerToolbar) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPdfSidenav) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_pdf_sidenav_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerThumbnailBar) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerThumbnail) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerAttachmentBar) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_attachment_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerAttachment) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_attachment_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Fullscreen) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Use a PDF document with multiple pages, to exercise navigating between
  // pages.
  RunTestsInJsModule("fullscreen_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPropertiesDialog) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // The properties dialog formats some values based on locale.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale{"en_US"};
  // This will apply to the new processes spawned within RunTestsInJsModule(),
  // thus consistently running the test in a well known time zone.
  content::ScopedTimeZone scoped_time_zone{"America/Los_Angeles"};
  RunTestsInJsModule("viewer_properties_dialog_test.js", "document_info.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PostMessageProxy) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("post_message_proxy_test.js", "test.pdf");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Printing) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

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
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, MAYBE_AnnotationsFeatureEnabled) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("annotations_feature_enabled_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, AnnotationsToolbar) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("annotations_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerToolbarDropdown) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_dropdown_test.js", "test.pdf");
}
#endif  // BUILDFLAG(ENABLE_INK)

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
// TODO(crbug.com/1444895): Re-enable it when integrating PDF OCR with
// Select-to-Speak.
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, DISABLED_PdfOcrToolbar) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

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

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, Beep) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, NoBeep) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, BeepThenNoBeep) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModuleNewTab("nobeep_test.js", "test-beep.pdf");

  // Make sure there are two PDFs in the same process.
  const int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, NoBeepThenBeep) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

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

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, BeepCsp) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // The script-source * directive in the mock headers file should
  // allow the JavaScript to execute the beep().
  RunTestsInJsModule("beep_test.js", "test-beep-csp.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, DISABLED_NoBeepCsp) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

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
IN_PROC_BROWSER_TEST_P(PDFExtensionWebUICodeCacheJSTest, Basic) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

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
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, NoFetchHandler) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunServiceWorkerTest("empty.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// then falls back to network by not calling FetchEvent.respondWith().
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, NetworkFallback) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunServiceWorkerTest("network_fallback_worker.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// provides a response.
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, Interception) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunServiceWorkerTest("respond_with_fetch_worker.js");
}

// TODO(crbug.com/1445746): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionContentSettingJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionWebUICodeCacheJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionServiceWorkerJSTest);
