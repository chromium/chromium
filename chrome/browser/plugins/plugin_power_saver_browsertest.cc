// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugin_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point.h"

using testing::_;
using testing::Return;

namespace {

// Use fixed browser dimensions for pixel tests.
const int kBrowserWidth = 600;
const int kBrowserHeight = 700;

// Compare only a portion of the snapshots, as different platforms will
// capture different sized snapshots (due to differences in browser chrome).
const gfx::Size kComparisonSize = gfx::Size(500, 600);

// This also tests that we have JavaScript access to the underlying plugin.
bool PluginLoaded(content::WebContents* contents,
                  const std::string& element_id) {
  std::string result = PluginTestUtils::RunTestScript(
      "if (plugin.postMessage === undefined) {"
      "  window.domAutomationController.send('poster_only');"
      "} else {"
      "  window.domAutomationController.send('plugin_loaded');"
      "}",
      contents, element_id);
  EXPECT_NE("error", result);
  return result == "plugin_loaded";
}

// Also waits for the placeholder UI overlay to finish loading.
void VerifyPluginIsThrottled(content::WebContents* contents,
                             const std::string& element_id) {
  std::string result = PluginTestUtils::RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral && event.data.isThrottled && "
      "      event.data.isHiddenForPlaceholder) {"
      "    window.domAutomationController.send('throttled');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("throttled", result);

  // Page should continue to have JavaScript access to all throttled plugins.
  EXPECT_TRUE(PluginLoaded(contents, element_id));

  PluginTestUtils::WaitForPlaceholderReady(contents, element_id);
}

void VerifyPluginMarkedEssential(content::WebContents* contents,
                                 const std::string& element_id) {
  std::string result = PluginTestUtils::RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral === false) {"
      "    window.domAutomationController.send('essential');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("essential", result);
  EXPECT_TRUE(PluginLoaded(contents, element_id));
}

}  // namespace

class PluginPowerSaverBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromDirectory(
        ui_test_utils::GetTestFilePath(
            base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
            base::FilePath()));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Plugin throttling only operates once Flash is ALLOW-ed on a site.
    GURL server_root = embedded_test_server()->GetURL("/");
    HostContentSettingsMap* content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    content_settings_map->SetContentSettingDefaultScope(
        server_root, server_root, ContentSettingsType::PLUGINS, std::string(),
        CONTENT_SETTING_ALLOW);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePluginPlaceholderTesting);
    ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

 protected:
  virtual void LoadHTML(const std::string& file) {
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(file));
    EXPECT_TRUE(content::WaitForRenderFrameReady(
        GetActiveWebContents()->GetMainFrame()));
  }

  // Loads a peripheral plugin (small cross origin) named 'plugin'.
  void LoadPeripheralPlugin() { LoadHTML("/load_peripheral_plugin.html"); }

  // Returns the background WebContents.
  content::WebContents* LoadHTMLInBackgroundTab(const std::string& file) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), embedded_test_server()->GetURL(file),
        WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    int index = browser()->tab_strip_model()->GetIndexOfLastWebContentsOpenedBy(
        GetActiveWebContents(), 0 /* start_index */);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    EXPECT_TRUE(content::WaitForRenderFrameReady(contents->GetMainFrame()));
    return contents;
  }

  void ActivateTab(content::WebContents* contents) {
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(contents),
        {TabStripModel::GestureType::kOther});
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // This sends a simulated click at |point| and waits for test plugin to send
  // a status message indicating that it is essential. The test plugin sends a
  // status message during:
  //  - Plugin creation, to handle a plugin freshly created from a poster.
  //  - Peripheral status change.
  //  - In response to the explicit 'getPowerSaverStatus' request, in case the
  //    test has missed the above two events.
  void SimulateClickAndAwaitMarkedEssential(const std::string& element_id,
                                            const gfx::Point& point) {
    PluginTestUtils::WaitForPlaceholderReady(GetActiveWebContents(),
                                             element_id);
    content::SimulateMouseClickAt(GetActiveWebContents(), 0 /* modifiers */,
                                  blink::WebMouseEvent::Button::kLeft, point);

    VerifyPluginMarkedEssential(GetActiveWebContents(), element_id);
  }

  // |element_id| must be an element on the foreground tab.
  void VerifyPluginIsPlaceholderOnly(const std::string& element_id) {
    EXPECT_FALSE(PluginLoaded(GetActiveWebContents(), element_id));
    PluginTestUtils::WaitForPlaceholderReady(GetActiveWebContents(),
                                             element_id);
  }

 protected:
  policy::MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargePostersNotThrottled) {
  // This test verifies that small posters are throttled, large posters are not,
  // and that large posters can whitelist origins for other plugins.
  LoadHTML("/large_posters_not_throttled.html");

  VerifyPluginIsPlaceholderOnly("poster_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "poster_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "plugin_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "poster_large");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, OriginWhitelisting) {
  LoadHTML("/origin_whitelisting.html");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_large");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, EssentialPlugins) {
  LoadHTML("/essential_plugins.html");

  VerifyPluginMarkedEssential(GetActiveWebContents(), "small_same_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "small_same_origin_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "large_cross_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "medium_16_9_cross_origin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ExpandingSmallPlugin) {
  LoadPeripheralPlugin();
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  std::string script = "window.document.getElementById('plugin').height = 400;";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabPlugins) {
  content::WebContents* background_contents =
      LoadHTMLInBackgroundTab("/background_tab_plugins.html");

  EXPECT_FALSE(PluginLoaded(background_contents, "same_origin"));
  EXPECT_FALSE(PluginLoaded(background_contents, "small_cross_origin"));

  ActivateTab(background_contents);

  VerifyPluginMarkedEssential(background_contents, "same_origin");
  VerifyPluginIsThrottled(background_contents, "small_cross_origin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ZoomIndependent) {
  zoom::ZoomController::FromWebContents(GetActiveWebContents())
      ->SetZoomLevel(4.0);
  LoadHTML("/zoom_independent.html");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BlockTinyPlugins) {
  LoadHTML("/block_tiny_plugins.html");

  VerifyPluginIsPlaceholderOnly("tiny_same_origin");
  VerifyPluginIsPlaceholderOnly("tiny_cross_origin_1");
  VerifyPluginIsPlaceholderOnly("tiny_cross_origin_2");
  VerifyPluginIsPlaceholderOnly("completely_obscured");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabTinyPlugins) {
  content::WebContents* background_contents =
      LoadHTMLInBackgroundTab("/background_tab_tiny_plugins.html");
  EXPECT_FALSE(PluginLoaded(background_contents, "tiny"));

  ActivateTab(background_contents);
  VerifyPluginIsPlaceholderOnly("tiny");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ExpandingTinyPlugins) {
  LoadHTML("/expanding_tiny_plugins.html");

  VerifyPluginIsPlaceholderOnly("expand_to_peripheral");
  VerifyPluginIsPlaceholderOnly("expand_to_essential");

  std::string script =
      "window.document.getElementById('expand_to_peripheral').height = 200;"
      "window.document.getElementById('expand_to_peripheral').width = 200;"
      "window.document.getElementById('expand_to_essential').height = 400;"
      "window.document.getElementById('expand_to_essential').width = 400;";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));

  VerifyPluginIsThrottled(GetActiveWebContents(), "expand_to_peripheral");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "expand_to_essential");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, RunAllFlashInAllowMode) {
  LoadHTML("/run_all_flash.html");
  VerifyPluginIsThrottled(GetActiveWebContents(), "small");
  VerifyPluginIsThrottled(GetActiveWebContents(), "cross_origin");

  policy::PolicyMap policy;
  policy.Set(policy::key::kRunAllFlashInAllowMode,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  provider_.UpdateChromePolicy(policy);
  content::RunAllPendingInMessageLoop();

  ASSERT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kRunAllFlashInAllowMode));

  LoadHTML("/run_all_flash.html");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "small");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "cross_origin");
}

class PluginPowerSaverPixelTest : public PluginPowerSaverBrowserTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    PluginPowerSaverBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The pixel tests run more reliably in software mode.
    command_line->AppendSwitch(switches::kDisableGpu);

    PluginPowerSaverBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  void LoadHTML(const std::string& file) override {
    gfx::Rect bounds(gfx::Rect(0, 0, kBrowserWidth, kBrowserHeight));
    gfx::Rect screen_bounds =
        display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
    ASSERT_GT(screen_bounds.width(), kBrowserWidth);
    ASSERT_GT(screen_bounds.height(), kBrowserHeight);
    browser()->window()->SetBounds(bounds);

    PluginPowerSaverBrowserTest::LoadHTML(file);
  }
};

// This test fail on macOS 10.12. https://crbug.com/599484.
// Flaky on Windows, Asan, and Msan. See crbug.com/549285 and crbug.com/512140.
// Because ChromeOS cannot use software rendering and the pixel tests continue
// to flake with hardware acceleration, disable these on ChromeOS.
#if defined(OS_MAC) || defined(OS_WIN) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(OS_CHROMEOS)
#define MAYBE_SmallCrossOrigin DISABLED_SmallCrossOrigin
#else
#define MAYBE_SmallCrossOrigin SmallCrossOrigin
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverPixelTest, MAYBE_SmallCrossOrigin) {
  LoadHTML("/small_cross_origin.html");

  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
  VerifyPluginIsPlaceholderOnly("plugin_poster");

  base::FilePath reference = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
      base::FilePath(FILE_PATH_LITERAL("small_cross_origin_expected.png")));
  EXPECT_TRUE(CompareWebContentsOutputToReference(GetActiveWebContents(),
                                                  reference, kComparisonSize));

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
  SimulateClickAndAwaitMarkedEssential("plugin_poster", gfx::Point(50, 150));
}

// Flaky on Windows, Asan, and Msan. See crbug.com/549285 and crbug.com/512140.
// Because ChromeOS cannot use software rendering and the pixel tests continue
// to flake with hardware acceleration, disable these on ChromeOS.
#if defined(OS_WIN) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(OS_CHROMEOS)
#define MAYBE_SmallerThanPlayIcon DISABLED_SmallerThanPlayIcon
#else
#define MAYBE_SmallerThanPlayIcon SmallerThanPlayIcon
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverPixelTest, MAYBE_SmallerThanPlayIcon) {
  LoadHTML("/smaller_than_play_icon.html");

  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_16");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_32");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_16_64");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_64_16");

  base::FilePath reference = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
      base::FilePath(FILE_PATH_LITERAL("smaller_than_play_icon_expected.png")));
  EXPECT_TRUE(CompareWebContentsOutputToReference(GetActiveWebContents(),
                                                  reference, kComparisonSize));
}

// This test fail on macOS 10.12. https://crbug.com/599484.
// Flaky on Windows, Asan, and Msan. See crbug.com/549285 and crbug.com/512140.
// Because ChromeOS cannot use software rendering and the pixel tests continue
// to flake with hardware acceleration, disable these on ChromeOS.
#if defined(OS_MAC) || defined(OS_WIN) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(OS_CHROMEOS)
#define MAYBE_PosterTests DISABLED_PosterTests
#else
#define MAYBE_PosterTests PosterTests
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverPixelTest, MAYBE_PosterTests) {
  // This test simultaneously verifies the varied supported poster syntaxes,
  // as well as verifies that the poster is rendered correctly with various
  // mismatched aspect ratios and sizes, following the same rules as VIDEO.
  LoadHTML("/poster_tests.html");

  VerifyPluginIsPlaceholderOnly("plugin_src");
  VerifyPluginIsPlaceholderOnly("plugin_srcset");

  VerifyPluginIsPlaceholderOnly("plugin_poster_param");
  VerifyPluginIsPlaceholderOnly("plugin_embed_src");
  VerifyPluginIsPlaceholderOnly("plugin_embed_srcset");

  VerifyPluginIsPlaceholderOnly("poster_missing");
  VerifyPluginIsPlaceholderOnly("poster_too_small");
  VerifyPluginIsPlaceholderOnly("poster_too_big");

  VerifyPluginIsPlaceholderOnly("poster_16");
  VerifyPluginIsPlaceholderOnly("poster_32");
  VerifyPluginIsPlaceholderOnly("poster_16_64");
  VerifyPluginIsPlaceholderOnly("poster_64_16");

  VerifyPluginIsPlaceholderOnly("poster_obscured");

  base::FilePath reference = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
      base::FilePath(FILE_PATH_LITERAL("poster_tests_expected.png")));
  EXPECT_TRUE(CompareWebContentsOutputToReference(GetActiveWebContents(),
                                                  reference, kComparisonSize));

  // Test that posters can be unthrottled via click.
  SimulateClickAndAwaitMarkedEssential("plugin_src", gfx::Point(50, 50));
}

// Flaky on almost all platforms: crbug.com/648827.
IN_PROC_BROWSER_TEST_F(PluginPowerSaverPixelTest,
                       DISABLED_LargeCrossOriginObscured) {
  LoadHTML("/large_cross_origin_obscured.html");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
  base::FilePath reference = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
      base::FilePath(
          FILE_PATH_LITERAL("large_cross_origin_obscured_expected.png")));
  EXPECT_TRUE(CompareWebContentsOutputToReference(GetActiveWebContents(),
                                                  reference, kComparisonSize));

  // Test that's unthrottled if it is unobscured.
  std::string script =
      "var container = window.document.getElementById('container');"
      "container.setAttribute('style', 'width: 400px; height: 400px;');";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}
