// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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
#include "extensions/test/test_extension_dir.h"
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
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif

namespace extensions {

namespace {

constexpr char kManifestStub[] = R"(
{
  "name": "chrome.automation.test",
  "key": "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8xv6iO+j4kzj1HiBL93+XVJH/CRyAQMUHS/Z0l8nCAzaAFkW/JsNwxJqQhrZspnxLqbQxNncXs6g6bsXAwKHiEs+LSs+bIv0Gc/2ycZdhXJ8GhEsSMakog5dpQd1681c2gLK/8CrAoewE/0GIKhaFcp7a2iZlGh4Am6fgMKy0iQIDAQAB",
  "version": "0.1",
  "manifest_version": 2,
  "description": "Tests for the Automation API.",
  "background": { %s },
  "permissions": %s,
  "automation": %s
}
)";

constexpr char kPersistentBackground[] = R"("scripts": ["common.js"])";
constexpr char kServiceWorkerBackground[] = R"("service_worker": "common.js")";
constexpr char kPermissionsDefault[] = R"(["tabs", "http://a.com/"])";

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    defined(USE_AURA)

constexpr char kAutomationDesktop[] = R"({ "desktop": true })";

#endif

static constexpr char kCommonScript[] = R"(

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

var rootNode = null;
var url = '';

function findAutomationNode(root, condition) {
  if (condition(root))
    return root;

  var children = root.children;
  for (var i = 0; i < children.length; i++) {
    var result = findAutomationNode(children[i], condition);
    if (result)
      return result;
  }
  return null;
}

function runWithDocument(docString, callback) {
  var url = 'data:text/html,<!doctype html>' + docString;
  var createParams = {
    active: true,
    url: url
  };
  createTabAndWaitUntilLoaded(url, function(tab) {
    chrome.automation.getDesktop(desktop => {
      const url = tab.url || tab.pendingUrl;
      let rootNode = desktop.find({attributes: {docUrl: url}});
      if (rootNode && rootNode.docLoaded) {
        callback(rootNode);
        return;
      }

      let listener = () => {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          desktop.removeEventListener('loadComplete', listener);
          desktop.addEventListener('focus', () => {});
          callback(rootNode);
        }
      };
      desktop.addEventListener('loadComplete', listener);
    });
  });
}

function listenOnce(node, eventType, callback, capture) {
  var innerCallback = function(evt) {
    node.removeEventListener(eventType, innerCallback, capture);
    callback(evt);
  };
  node.addEventListener(eventType, innerCallback, capture);
}

function setUpAndRunDesktopTests(allTests) {
  chrome.automation.getDesktop(function(rootNodeArg) {
    rootNode = rootNodeArg;
    chrome.test.runTests(allTests);
  });
}

function setUpAndRunTabsTests(allTests, opt_path, opt_ensurePersists = true) {
  var path = opt_path || 'index.html';
  getUrlFromConfig(path, function(url) {
    createTabAndWaitUntilLoaded(url, function(unused_tab) {
      chrome.automation.getDesktop(function(desktop) {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          chrome.test.runTests(allTests);
          return;
        }
        function listener() {
          rootNode = desktop.find({attributes: {docUrl: url}});
          if (rootNode && rootNode.docLoaded) {
            desktop.removeEventListener('loadComplete', listener);
            if (opt_ensurePersists) {
              desktop.addEventListener('focus', () => {});
            }
            chrome.test.runTests(allTests);
          }
        }
        desktop.addEventListener('loadComplete', listener);
      });
    });
  });
}

function getUrlFromConfig(path, callback) {
  chrome.test.getConfig(function(config) {
    assertTrue('testServer' in config, 'Expected testServer in config');
    url = ('http://a.com:PORT/' + path)
        .replace(/PORT/, config.testServer.port);
    callback(url)
  });
}

function createTabAndWaitUntilLoaded(url, callback) {
  chrome.tabs.create({'url': url}, function(tab) {
    chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo) {
      if (tabId == tab.id && changeInfo.status == 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        callback(tab);
      }
    });
  });
}

async function pollUntil(predicate, pollEveryMs) {
  return new Promise(r => {
    const id = setInterval(() => {
      let ret;
      if (ret = predicate()) {
        clearInterval(id);
        r(ret);
      }
    }, pollEveryMs);
  });
}

const scriptUrl = '_test_resources/api_test/automation/tests/%s';

chrome.test.loadScript(scriptUrl).then(function() {
  // The script will start the tests, so nothing to do here.
}).catch(function(error) {
  chrome.test.fail(scriptUrl + ' failed to load');
});

)";  // kCommonScript

}  // namespace

using ContextType = ExtensionBrowserTest::ContextType;

class AutomationApiTest : public ExtensionApiTest {
 public:
  explicit AutomationApiTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~AutomationApiTest() override = default;
  AutomationApiTest(const AutomationApiTest&) = delete;
  AutomationApiTest& operator=(const AutomationApiTest&) = delete;

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
    static const char kSitesDir[] = "automation/sites";
    base::FilePath test_data;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/api_test").AppendASCII(kSitesDir));
    ASSERT_TRUE(ExtensionApiTest::StartEmbeddedTestServer());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class AutomationApiTestWithContextType
    : public AutomationApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  AutomationApiTestWithContextType() : AutomationApiTest(GetParam()) {}
  ~AutomationApiTestWithContextType() override = default;
  AutomationApiTestWithContextType(const AutomationApiTestWithContextType&) =
      delete;
  AutomationApiTestWithContextType& operator=(
      const AutomationApiTestWithContextType&) = delete;

 protected:
  bool CreateExtensionAndRunTest(
      const char* script_path,
      const char* automation_type,
      const char* permissions = kPermissionsDefault) {
    TestExtensionDir test_dir;
    const char* background_value = GetParam() == ContextType::kServiceWorker
                                       ? kServiceWorkerBackground
                                       : kPersistentBackground;
    const std::string manifest = base::StringPrintf(
        kManifestStub, background_value, permissions, automation_type);
    const std::string common_script =
        base::StringPrintf(kCommonScript, script_path);
    test_dir.WriteManifest(manifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("common.js"), common_script);
    return RunExtensionTest(test_dir.UnpackedPath(), {},
                            {.context_type = ContextType::kFromManifest});
  }
};

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class AutomationApiCanvasTest : public AutomationApiTestWithContextType {
 public:
  void SetUp() override {
    EnablePixelOutput();
    AutomationApiTestWithContextType::SetUp();
  }
};

#if defined(USE_AURA)

// TODO(crbug.com/1093066): This should be moved outside of the USE_AURA
// block when tests are converted that are outside of this block.
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AutomationApiTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AutomationApiTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorker));

namespace {
static const char kDomain[] = "a.com";
static const char kGotTree[] = "got_tree";
}  // anonymous namespace

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType,
                       TestRendererAccessibilityEnabled) {
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

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ServiceWorker) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(tab->IsWebContentsOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/service_worker");
  ExtensionTestMessageListener got_tree(kGotTree);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(tab->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, SanityCheck) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/sanity_check.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, ImageLabels) {
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

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Events) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/events.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Actions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/actions.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Location) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/location.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Location2) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/location2.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, BoundsForRange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/bounds_for_range.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, LineStartOffsets) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/line_start_offsets.js",
                                        kAutomationDesktop))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AutomationApiCanvasTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AutomationApiCanvasTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Flaky on Mac: crbug.com/1338036
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_ImageData DISABLED_ImageData
#else
#define MAYBE_ImageData ImageData
#endif
IN_PROC_BROWSER_TEST_P(AutomationApiCanvasTest, MAYBE_ImageData) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/image_data.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, TableProperties) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/table_properties.js", kAutomationDesktop))
      << message_;
}

// Flaky on Mac and Windows: crbug.com/1235249
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_CloseTab DISABLED_CloseTab
#else
#define MAYBE_CloseTab CloseTab
#endif
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, MAYBE_CloseTab) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/close_tab.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Find) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/find.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Attributes) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/attributes.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, ReverseRelations) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/reverse_relations.js",
                                        kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, TreeChange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/tree_change.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, TreeChangeIndirect) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/tree_change_indirect.js",
                                        kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, DocumentSelection) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/document_selection.js",
                                        kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, HitTest) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/hit_test.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, WordBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/word_boundaries.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, SentenceBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/sentence_boundaries.js",
                                        kAutomationDesktop))
      << message_;
}

class AutomationApiTestWithLanguageDetection
    : public AutomationApiTestWithContextType {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutomationApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AutomationApiTestWithLanguageDetection,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AutomationApiTestWithLanguageDetection,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithLanguageDetection,
                       DetectedLanguage) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/detected_language.js",
                                        kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType,
                       IgnoredNodesNotReturned) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/ignored_nodes_not_returned.js",
                                        kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, ForceLayout) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/force_layout.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Intents) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/intents.js", kAutomationDesktop))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, EnumValidity) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(
      CreateExtensionAndRunTest("tabs/enum_validity.js", kAutomationDesktop))
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

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, AccessibilityFocus) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/accessibility_focus.js",
                                        kAutomationDesktop))
      << message_;
}

// TODO(http://crbug.com/1162238): flaky on ChromeOS.
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType,
                       DISABLED_TextareaAppendPerf) {
  StartEmbeddedTestServer();

  {
    base::RunLoop wait_for_tracing;
    content::TracingController::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(
            R"({"included_categories": ["accessibility"])"),
        wait_for_tracing.QuitClosure());
    wait_for_tracing.Run();
  }

  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/textarea_append_perf.js",
                                        kAutomationDesktop))
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

class AutomationApiTestWithMockedSourceRenderer
    : public AutomationApiTest,
      public ui::AXActionHandlerObserver {
 protected:
  // This method is used to intercept AXActions dispatched from extensions.
  // Because `DispatchActionResult`, from the automation API, is only used in
  // specific source renderers (e.g. arc++), we mock the behavior here so we can
  // test that the behavior in the automation api works correctly.
  void InterceptAXActions() {
    ui::AXActionHandlerRegistry* registry =
        ui::AXActionHandlerRegistry ::GetInstance();
    ASSERT_TRUE(registry);
    registry->AddObserver(this);
  }

 private:
  // ui::AXActionHandlerObserver :
  void PerformAction(const ui::AXActionData& action_data) override {
    extensions::AutomationEventRouter* router =
        extensions::AutomationEventRouter::GetInstance();
    ASSERT_TRUE(router);
    EXPECT_EQ(action_data.action, ax::mojom::Action::kScrollBackward);
    router->DispatchActionResult(action_data, /*result=*/true);
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTestWithMockedSourceRenderer,
                       ActionResult) {
  StartEmbeddedTestServer();

  // Intercept AXActions for this test in order to test the behavior of
  // DispatchActionResult. Here, we mock the action logic to always return true
  // to return to the extension test that the action was handled and that the
  // result is true. This will make sure that the passing of messages between
  // processes is correct.
  InterceptAXActions();
  ASSERT_TRUE(RunExtensionTest("automation/tests/desktop",
                               {.extension_url = "action_result.html"}))
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
