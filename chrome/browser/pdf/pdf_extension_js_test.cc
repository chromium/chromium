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
#include "base/test/run_until.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_time_zone.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

class PDFExtensionJSTestBase : public PDFExtensionTestBase {
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

  // Loads `url` either in the current tab or a new tab and wait for it to be
  // fully loaded before returning. Returns whether the PDF loaded or not.
  virtual bool LoadPdfAndWait(const GURL& url, bool new_tab) {
    return new_tab ? LoadPdfInNewTab(url) : LoadPdf(url);
  }

 private:
  // Runs the extensions test at chrome/test/data/pdf/<filename> on the PDF file
  // at chrome/test/data/pdf/<pdf_filename>, where |filename| is loaded as a JS
  // module.
  void RunTestsInJsModuleHelper(const std::string& filename,
                                const std::string& pdf_filename,
                                bool new_tab) {
    GURL url(embedded_test_server()->GetURL("/pdf/" + pdf_filename));
    ASSERT_TRUE(LoadPdfAndWait(url, new_tab));
    content::RenderFrameHost* extension_host =
        pdf_extension_test_util::GetOnlyPdfExtensionHost(
            GetActiveWebContents());
    ASSERT_TRUE(extension_host);

    extensions::ResultCatcher catcher;
    constexpr char kModuleLoaderTemplate[] =
        R"(var s = document.createElement('script');
           s.type = 'module';
           s.src = 'chrome://%s/pdf/%s';
           s.onerror = function(e) {
             console.error('Error while loading', e.target.src);
           };
           document.body.appendChild(s);)";

    bool result = content::ExecJs(
        extension_host,
        base::StringPrintf(kModuleLoaderTemplate,
                           chrome::kChromeUIWebUITestHost, filename.c_str()));

    if (coverage_handler_ && coverage_handler_->CoverageEnabled()) {
      const auto* test_info =
          ::testing::UnitTest::GetInstance()->current_test_info();
      const std::string full_test_name = base::StrCat(
          {test_info->test_suite_name(), test_info->name()});
      coverage_handler_->CollectCoverage(full_test_name);
    }

    ASSERT_TRUE(result);

    if (!catcher.GetNextResult()) {
      FAIL() << catcher.message();
    }
  }

  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;
};

class PDFExtensionJSTest : public base::test::WithFeatureOverride,
                           public PDFExtensionJSTestBase {
 public:
  PDFExtensionJSTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const override { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Basic) {
  RunTestsInJsModule("basic_test.js", "test.pdf");
  EXPECT_EQ(1, CountPDFProcesses());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, BasicPlugin) {
  RunTestsInJsModule("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PluginController) {
  RunTestsInJsModule("plugin_controller_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Viewport) {
  RunTestsInJsModule("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Layout3) {
  RunTestsInJsModule("layout_test.js", "test-layout3.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Layout4) {
  RunTestsInJsModule("layout_test.js", "test-layout4.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Bookmark) {
  RunTestsInJsModule("bookmarks_test.js", "test-bookmarks-with-zoom.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Navigator) {
  RunTestsInJsModule("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ParamsParser) {
  RunTestsInJsModule("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ZoomManager) {
  RunTestsInJsModule("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, GestureDetector) {
  RunTestsInJsModule("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, SwipeDetector) {
  RunTestsInJsModule("swipe_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, TouchHandling) {
  RunTestsInJsModule("touch_handling_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, DownloadControls) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("download_controls_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Title) {
  RunTestsInJsModule("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, WhitespaceTitle) {
  RunTestsInJsModule("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PageChange) {
  RunTestsInJsModule("page_change_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PageSelector) {
  RunTestsInJsModule("page_selector_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ScrollWithFormFieldFocusedTest) {
  RunTestsInJsModule("scroll_with_form_field_focused_test.js",
                     "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Metrics) {
  RunTestsInJsModule("metrics_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPasswordDialog) {
  RunTestsInJsModule("viewer_password_dialog_test.js", "encrypted.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ArrayBufferAllocator) {
  // Run several times to see if there are issues with unloading.
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
  RunTestsInJsModule("beep_test.js", "array_buffer.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerPdfSidenav) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_pdf_sidenav_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerThumbnailBar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerThumbnail) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_thumbnail_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerAttachmentBar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_attachment_bar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerAttachment) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_attachment_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Fullscreen) {
  // Use a PDF document with multiple pages, to exercise navigating between
  // pages.
  RunTestsInJsModule("fullscreen_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, PostMessageProxy) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("post_message_proxy_test.js", "test.pdf");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Printing) {
  RunTestsInJsModule("printing_icon_test.js", "test.pdf");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_INK)
// TODO(crbug.com/41434927): Test times out under sanitizers.
// TODO(crbug.com/41495998): Test fails for
// testViewportToCameraConversion.
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, DISABLED_AnnotationsFeatureEnabled) {
  // TODO(crbug.com/40268279): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  RunTestsInJsModule("annotations_feature_enabled_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, AnnotationsToolbar) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("annotations_toolbar_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, ViewerToolbarDropdown) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInJsModule("viewer_toolbar_dropdown_test.js", "test.pdf");
}
#endif  // BUILDFLAG(ENABLE_INK)

// PDFExtensionJSTest with forced Pacific Time Zone.
class PDFExtensionPacificTimeZoneJSTest : public PDFExtensionJSTest {
  // This will apply to the new processes spawned within RunTestsInJsModule(),
  // thus consistently running the test in a well known time zone.
  // ScopedTimeZone needs to be created before the test setup. ScopedTimeZone
  // overrides TimeZoneMonitor binder in DeviceService so the test setup creates
  // FakeTimeZoneMonitor instead of the real TimeZoneMonitor implementation.
  content::ScopedTimeZone scoped_time_zone_{"America/Los_Angeles"};
};

IN_PROC_BROWSER_TEST_P(PDFExtensionPacificTimeZoneJSTest,
                       ViewerPropertiesDialog) {
  // The properties dialog formats some values based on locale.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale{"en_US"};
  RunTestsInJsModule("viewer_properties_dialog_test.js", "document_info.pdf");
}

class PDFExtensionContentSettingJSTest : public PDFExtensionJSTest {
 protected:
  void SetPdfJavaScript(bool enabled) {
    auto* map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::JAVASCRIPT,
        enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  }

  // Uses a different mechanism than PDFExtensionTestBase::LoadPdfInNewTab to
  // wait for tabs to load to support content setting tests.
  bool LoadPdfAndWait(const GURL& url, bool new_tab) override {
    if (new_tab) {
      EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
          browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    } else {
      EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    }

    content::WebContents* contents = GetActiveWebContents();

    // Wait for the extension host to load.
    bool result = base::test::RunUntil([&]() {
      return pdf_extension_test_util::GetOnlyPdfExtensionHost(contents);
    });
    if (!result) {
      return false;
    }

    // Wait for the extension to finish initializing.
    content::RenderFrameHost* extension_host =
        pdf_extension_test_util::GetOnlyPdfExtensionHost(contents);
    static constexpr char kEnsurePdfHasLoadedScript[] = R"(
       document.body.querySelector('#viewer').loadState_ == 'success'
     )";

    while (true) {
      // content::EvalJs uses a run loop internally.
      auto js_result =
          content::EvalJs(extension_host, kEnsurePdfHasLoadedScript);
      // The dom can be in an unusable state during setup. If the EvalJs
      // errors out tries again.
      if (js_result.error.empty() && js_result.ExtractBool()) {
        return true;
      }
    }
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, Beep) {
  RunTestsInJsModule("beep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, NoBeep) {
  SetPdfJavaScript(/*enabled=*/false);
  RunTestsInJsModule("nobeep_test.js", "test-beep.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, BeepThenNoBeep) {
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
  // The script-source * directive in the mock headers file should
  // allow the JavaScript to execute the beep().
  RunTestsInJsModule("beep_test.js", "test-beep-csp.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionContentSettingJSTest, DISABLED_NoBeepCsp) {
  // The script-source none directive in the mock headers file should
  // prevent the JavaScript from executing the beep().
  // TODO(crbug.com/40050941) functionality not implemented.
  RunTestsInJsModule("nobeep_test.js", "test-nobeep-csp.pdf");
}

class PDFExtensionWebUICodeCacheJSTest : public PDFExtensionJSTest {
 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionJSTest::GetEnabledFeatures();
    enabled.push_back({features::kWebUICodeCache, {}});
    return enabled;
  }
};

// Regression test for https://crbug.com/1239148.
IN_PROC_BROWSER_TEST_P(PDFExtensionWebUICodeCacheJSTest, Basic) {
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
  RunServiceWorkerTest("empty.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// then falls back to network by not calling FetchEvent.respondWith().
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, NetworkFallback) {
  RunServiceWorkerTest("network_fallback_worker.js");
}

// Test navigating to a PDF when a service worker intercepts the request and
// provides a response.
IN_PROC_BROWSER_TEST_P(PDFExtensionServiceWorkerJSTest, Interception) {
  RunServiceWorkerTest("respond_with_fetch_worker.js");
}

#if BUILDFLAG(ENABLE_PDF_INK2)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Test behavior when Ink2 and annotation mode are disabled for the PDF viewer.
// Don't run this test on Ash, as annotation mode is always enabled there.
IN_PROC_BROWSER_TEST_P(PDFExtensionJSTest, Ink2Disabled) {
  RunTestsInJsModule("ink2_disabled_test.js", "test.pdf");
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class PDFExtensionJSInk2Test : public PDFExtensionJSTest {
 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionJSTest::GetEnabledFeatures();
    enabled.push_back({chrome_pdf::features::kPdfInk2, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionJSInk2Test, Ink2) {
  RunTestsInJsModule("ink2_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSInk2Test, Ink2Save) {
  RunTestsInJsModule("ink2_save_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSInk2Test, Ink2SidePanel) {
  RunTestsInJsModule("ink2_side_panel_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_P(PDFExtensionJSInk2Test, Ink2ViewerToolbar) {
  RunTestsInJsModule("ink2_viewer_toolbar_test.js", "test.pdf");
}

class PDFExtensionJSInk2BeforeUnloadTest : public PDFExtensionJSTestBase {
 public:
  // OOPIF PDF only, since MimeHandler handles the beforeunload event instead.
  bool UseOopif() const override { return true; }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionJSTestBase::GetEnabledFeatures();
    enabled.push_back({chrome_pdf::features::kPdfInk2, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_F(PDFExtensionJSInk2BeforeUnloadTest, Stroke) {
  RunTestsInJsModule("ink2_before_unload_stroke_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionJSInk2BeforeUnloadTest, Undo) {
  RunTestsInJsModule("ink2_before_unload_undo_test.js", "test.pdf");
}

#endif  // BUILDFLAG(ENABLE_PDF_INK2)

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionPacificTimeZoneJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionContentSettingJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionWebUICodeCacheJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionServiceWorkerJSTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionJSInk2Test);
