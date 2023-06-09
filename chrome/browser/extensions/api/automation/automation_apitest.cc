// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/tree_generator.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/display_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif

namespace extensions {

namespace {
static const char kDomain[] = "a.com";
static const char kSitesDir[] = "automation/sites";
static const char kGotTree[] = "got_tree";
}  // anonymous namespace

class AutomationApiTest : public ExtensionApiTest {
 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::NumberToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    GURL url =
        embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
    return url;
  }

  void StartEmbeddedTestServer() {
    base::FilePath test_data;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/api_test").AppendASCII(kSitesDir));
    ASSERT_TRUE(ExtensionApiTest::StartEmbeddedTestServer());
  }

 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class AutomationApiCanvasTest : public AutomationApiTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    ExtensionApiTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TestRendererAccessibilityEnabled) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(tab->IsWebContentsOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(tab->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, SanityCheck) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "sanity_check.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ImageLabels) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Enable image labels.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, true);

  // Initially there should be no accessibility mode set.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto accessibility_mode = web_contents->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_EQ(ui::AXMode(), accessibility_mode);

  // Enable automation.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  // Now the AXMode should include kLabelImages.
  ui::AXMode expected_mode = ui::kAXModeWebContentsOnly;
  expected_mode.set_mode(ui::AXMode::kLabelImages, true);
  accessibility_mode = web_contents->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  EXPECT_EQ(expected_mode, accessibility_mode);
}

// Flaky on Mac: crbug.com/1248445
#if BUILDFLAG(IS_MAC)
#define MAYBE_GetTreeByTabId DISABLED_GetTreeByTabId
#else
#define MAYBE_GetTreeByTabId GetTreeByTabId
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_GetTreeByTabId) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "tab_id.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Events) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "events.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Actions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "actions.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Location) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "location.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Location2) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "location2.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, BoundsForRange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "bounds_for_range.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, LineStartOffsets) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "line_start_offsets.html"}))
      << message_;
}

// Flaky on Mac: crbug.com/1338036
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_ImageData DISABLED_ImageData
#else
#define MAYBE_ImageData ImageData
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiCanvasTest, MAYBE_ImageData) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "image_data.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TableProperties) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "table_properties.html"}))
      << message_;
}

// Flaky on Mac and Windows: crbug.com/1235249
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_TabsAutomationBooleanPermissions \
  DISABLED_TabsAutomationBooleanPermissions
#else
#define MAYBE_TabsAutomationBooleanPermissions TabsAutomationBooleanPermissions
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest,
                       MAYBE_TabsAutomationBooleanPermissions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs_automation_boolean",
                               {.extension_url = "permissions.html"}))
      << message_;
}

// Flaky on Mac and Windows: crbug.com/1235249
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_TabsAutomationBooleanActions \
  DISABLED_TabsAutomationBooleanActions
#else
#define MAYBE_TabsAutomationBooleanActions TabsAutomationBooleanActions
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_TabsAutomationBooleanActions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs_automation_boolean",
                               {.extension_url = "actions.html"}))
      << message_;
}

// Flaky on Mac and Windows: crbug.com/1202710
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_TabsAutomationHostsPermissions \
  DISABLED_TabsAutomationHostsPermissions
#else
#define MAYBE_TabsAutomationHostsPermissions TabsAutomationHostsPermissions
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest,
                       MAYBE_TabsAutomationHostsPermissions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs_automation_hosts",
                               {.extension_url = "permissions.html"}))
      << message_;
}

// Flaky on Mac and Windows: crbug.com/1235249
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_CloseTab DISABLED_CloseTab
#else
#define MAYBE_CloseTab CloseTab
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_CloseTab) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "close_tab.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, QuerySelector) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "queryselector.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Find) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionTest("automation/tests/tabs", {.extension_url = "find.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Attributes) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "attributes.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ReverseRelations) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "reverse_relations.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TreeChange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "tree_change.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TreeChangeIndirect) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "tree_change_indirect.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DocumentSelection) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "document_selection.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, HitTest) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "hit_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, WordBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "word_boundaries.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, SentenceBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "sentence_boundaries.html"}))
      << message_;
}

class AutomationApiTestWithLanguageDetection : public AutomationApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutomationApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTestWithLanguageDetection,
                       DetectedLanguage) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "detected_language.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, IgnoredNodesNotReturned) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionTest("automation/tests/tabs",
                       {.extension_url = "ignored_nodes_not_returned.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ForceLayout) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "force_layout.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Intents) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "intents.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, EnumValidity) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "enum_validity.html"}))
      << message_;
}

#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopNotRequested) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "desktop_not_requested.html"}))
      << message_;
}
#endif  // defined(USE_AURA)

#if !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopNotSupported) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "desktop_not_supported.html"}))
      << message_;
}
#endif  // !defined(USE_AURA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class AutomationApiFencedFrameTest : public AutomationApiTest {
 protected:
  AutomationApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{blink::features::kFencedFrames, {}},
                              {features::kPrivacySandboxAdsAPIsOverride, {}},
                              {blink::features::kFencedFramesAPIChanges, {}},
                              {blink::features::kFencedFramesDefaultMode, {}}},
        /*disabled_features=*/{features::kSpareRendererForSitePerProcess});
  }

  ~AutomationApiFencedFrameTest() override = default;

 public:
  void SetUpOnMainThread() override { AutomationApiTest::SetUpOnMainThread(); }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutomationApiFencedFrameTest, DesktopFindInFencedframe) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop/fencedframe",
                               {.extension_url = "focus_fencedframe.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Desktop) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "desktop.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopInitialFocus) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "initial_focus.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopFocusWeb) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "focus_web.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopFocusIframe) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "focus_iframe.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopHitTestIframe) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "hit_test_iframe.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopFocusViews) {
  AutomationManagerAura::GetInstance()->Enable();
  // Trigger the shelf subtree to be computed.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::AcceleratorAction::kFocusShelf, {});

  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "focus_views.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopGetNextTextMatch) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "get_next_text_match.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, LocationInWebView) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/webview",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopActions) {
  AutomationManagerAura::GetInstance()->Enable();
  // Trigger the shelf subtree to be computed.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::AcceleratorAction::kFocusShelf, {});

  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "actions.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopHitTestOneDisplay) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "hit_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopHitTestPrimaryDisplay) {
  ash::ShellTestApi shell_test_api;
  // Create two displays, both 800x750px, next to each other. The primary
  // display has top left corner at (0, 0), and the secondary display has
  // top left corner at (801, 0).
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("800x750,801+0-800x750");
  // Ensure it worked. By default InProcessBrowserTest uses just one display.
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());
  // The browser will open in the primary display.
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "hit_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopHitTestSecondaryDisplay) {
  ash::ShellTestApi shell_test_api;
  // Create two displays, both 800x750px, next to each other. The primary
  // display has top left corner at (0, 0), and the secondary display has
  // top left corner at (801, 0).
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("800x750,801+0-800x750");
  // Ensure it worked. By default InProcessBrowserTest uses just one display.
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());

  display::Screen* screen = display::Screen::GetScreen();
  int64_t display2 = display_manager_test_api.GetSecondaryDisplay().id();
  screen->SetDisplayForNewWindows(display2);
  // Run the test in the browser in the non-primary display.
  // Open a browser on the secondary display, which is default for new windows.
  CreateBrowser(browser()->profile());
  // Close the browser which was already opened on the primary display.
  CloseBrowserSynchronously(browser());
  // Sets browser() to return the one created above, instead of the one which
  // was closed.
  SelectFirstBrowser();
  // The test will run in browser().
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "hit_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopLoadTabs) {
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "load_tabs.html"}))
      << message_;
}

class AutomationApiTestWithDeviceScaleFactor : public AutomationApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutomationApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor, "2.0");
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTestWithDeviceScaleFactor, LocationScaled) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/location_scaled",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTestWithDeviceScaleFactor, HitTest) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "hit_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Position) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "position.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, AccessibilityFocus) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "accessibility_focus.html"}))
      << message_;
}

// TODO(http://crbug.com/1162238): flaky on ChromeOS.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_TextareaAppendPerf) {
  StartEmbeddedTestServer();

  {
    base::RunLoop wait_for_tracing;
    content::TracingController::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(
            R"({"included_categories": ["accessibility"])"),
        wait_for_tracing.QuitClosure());
    wait_for_tracing.Run();
  }

  ASSERT_TRUE(RunExtensionTest("automation/tests/tabs",
                               {.extension_url = "textarea_append_perf.html"}))
      << message_;

  base::test::TestFuture<std::unique_ptr<std::string>> stop_tracing_future;
  content::TracingController::GetInstance()->StopTracing(
      content::TracingController::CreateStringEndpoint(
          stop_tracing_future.GetCallback()));

  absl::optional<base::Value> trace_data =
      base::JSONReader::Read(*stop_tracing_future.Take());
  ASSERT_TRUE(trace_data && trace_data->is_dict());

  const base::Value::List* trace_events =
      trace_data->GetDict().FindList("traceEvents");
  ASSERT_TRUE(trace_events);

  int renderer_total_dur = 0;
  int automation_total_dur = 0;
  for (const base::Value& event : *trace_events) {
    const std::string* cat = event.GetDict().FindString("cat");
    if (!cat || *cat != "accessibility")
      continue;

    const std::string* name = event.GetDict().FindString("name");
    if (!name)
      continue;

    absl::optional<int> dur = event.GetDict().FindInt("dur");
    if (!dur)
      continue;

    if (*name == "AutomationAXTreeWrapper::OnAccessibilityEvents")
      automation_total_dur += *dur;
    else if (*name == "RenderAccessibilityImpl::SendPendingAccessibilityEvents")
      renderer_total_dur += *dur;
  }

  ASSERT_GT(automation_total_dur, 0);
  ASSERT_GT(renderer_total_dur, 0);
  LOG(INFO) << "Total duration in automation: " << automation_total_dur;
  LOG(INFO) << "Total duration in renderer: " << renderer_total_dur;

  // Assert that the time spent in automation isn't more than 2x
  // the time spent in the renderer code.
  ASSERT_LT(automation_total_dur, renderer_total_dur * 2);
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, IframeNav) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "iframenav.html"}))
      << message_;
}

// TODO(crbug.com/1325383): test is flaky on Chromium OS MSAN builder.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_AddRemoveEventListeners DISABLED_AddRemoveEventListeners
#else
#define MAYBE_AddRemoveEventListeners AddRemoveEventListeners
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_AddRemoveEventListeners) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionTest("automation/tests/desktop",
                       {.extension_url = "add_remove_event_listeners.html"}))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/1209766) Flaky on lacros
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HitTestMultipleWindows DISABLED_HitTestMultipleWindows
#else
#define MAYBE_HitTestMultipleWindows HitTestMultipleWindows
#endif

IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_HitTestMultipleWindows) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionTest("automation/tests/desktop",
                       {.extension_url = "hit_test_multiple_windows.html"}))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
