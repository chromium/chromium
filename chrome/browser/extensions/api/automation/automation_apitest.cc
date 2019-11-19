// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/tree_generator.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/display_switches.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/accelerators.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
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
        test_data.AppendASCII("extensions/api_test")
        .AppendASCII(kSitesDir));
    ASSERT_TRUE(ExtensionApiTest::StartEmbeddedTestServer());
  }

 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalAccessibilityLabels);
    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(tab->IsWebContentsOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree, false /* no reply */);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(tab->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, SanityCheck) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "sanity_check.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ImageLabels) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Enable image labels.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, true);

  // Initially there should be no accessibility mode set.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_EQ(ui::AXMode(), web_contents->GetAccessibilityMode());

  // Enable automation.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree, false /* no reply */);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  // Now the AXMode should include kLabelImages.
  ui::AXMode expected_mode = ui::kAXModeWebContentsOnly;
  expected_mode.set_mode(ui::AXMode::kLabelImages, true);
  EXPECT_EQ(expected_mode, web_contents->GetAccessibilityMode());
}

// TODO(aboxhall): Fix flakiness
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_GetTreeByTabId) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "tab_id.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Events) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "events.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Actions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "actions.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Location) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "location.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Location2) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "location2.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, BoundsForRange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs",
                                  "bounds_for_range.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, LineStartOffsets) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "line_start_offsets.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiCanvasTest, ImageData) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "image_data.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TableProperties) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "table_properties.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TabsAutomationBooleanPermissions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest(
          "automation/tests/tabs_automation_boolean", "permissions.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TabsAutomationBooleanActions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest(
          "automation/tests/tabs_automation_boolean", "actions.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TabsAutomationHostsPermissions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest(
          "automation/tests/tabs_automation_hosts", "permissions.html"))
      << message_;
}

#if defined(USE_AURA)
// TODO(https://crbug.com/754870): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_Desktop) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "desktop.html"))
      << message_;
}

#if defined(OS_CHROMEOS)
// TODO(https://crbug.com/754870): Flaky on CrOS sanitizers.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_DesktopInitialFocus) {
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/desktop", "initial_focus.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopFocusWeb) {
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/desktop", "focus_web.html"))
      << message_;
}

// TODO(https://crbug.com/622387): flaky.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_DesktopFocusIframe) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/desktop", "focus_iframe.html"))
      << message_;
}

// TODO(https://crbug.com/622387): flaky.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_DesktopHitTestIframe) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/desktop", "hit_test_iframe.html"))
      << message_;
}

// TODO(https://crbug.com/892960): flaky.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_DesktopFocusViews) {
  AutomationManagerAura::GetInstance()->Enable();
  // Trigger the shelf subtree to be computed.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(ash::FOCUS_SHELF,
                                                            {});

  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/desktop", "focus_views.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopGetNextTextMatch) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop",
                                  "get_next_text_match.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, LocationInWebView) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunPlatformAppTest("automation/tests/webview")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopNotRequested) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs",
                                  "desktop_not_requested.html")) << message_;
}

#if defined(OS_CHROMEOS)
// TODO(https://crbug.com/894016): flaky.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_DesktopActions) {
  AutomationManagerAura::GetInstance()->Enable();
  // Trigger the shelf subtree to be computed.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(ash::FOCUS_SHELF,
                                                            {});

  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "actions.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopHitTest) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "hit_test.html"))
      << message_;
}

// TODO(https://crbug.com/754870): flaky.
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_DesktopLoadTabs) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "load_tabs.html"))
      << message_;
}
#endif  // defined(OS_CHROMEOS)
#else  // !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopNotSupported) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop",
                                  "desktop_not_supported.html"))
      << message_;
}
#endif  // defined(USE_AURA)

// Flaky test on site_per_browser_tests: https://crbug.com/833318
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_CloseTab) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "close_tab.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, QuerySelector) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "queryselector.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Find) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "find.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Attributes) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "attributes.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ReverseRelations) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "reverse_relations.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TreeChange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "tree_change.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TreeChangeIndirect) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "tree_change_indirect.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DocumentSelection) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "document_selection.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, HitTest) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "hit_test.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, WordBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "word_boundaries.html"))
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
  ASSERT_TRUE(
      RunExtensionSubtest("automation/tests/tabs", "detected_language.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, IgnoredNodesNotReturned) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs",
                                  "ignored_nodes_not_returned.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ForceLayout) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "force_layout.html"))
      << message_;
}

#if defined(OS_CHROMEOS)

class AutomationApiTestWithDeviceScaleFactor : public AutomationApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutomationApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2.0");
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTestWithDeviceScaleFactor, LocationScaled) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunPlatformAppTest("automation/tests/location_scaled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTestWithDeviceScaleFactor, HitTest) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "hit_test.html"))
      << message_;
}

#endif  // defined(OS_CHROMEOS)

}  // namespace extensions
