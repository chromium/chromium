// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Manifest permissions injected into |kManifest|:
constexpr auto kPermissions = std::to_array<const char*>({
    "*://*/*",               // ALL
    "http://127.0.0.1/*",    // PARTICULAR
    "http://nowhere.com/*",  // NOWHERE
});

// Script matchers for injected into |kBackgroundScriptSource|:
constexpr auto kScriptMatchers = std::to_array<const char*>({
    "{ pageUrl: { hostContains: '' } }",           // ALL
    "{ pageUrl: { hostEquals: '127.0.0.1' } }",    // PARTICULAR
    "{ pageUrl: { hostEquals: 'nowhere.com' } }",  // NOWHERE
});

enum PermissionOrMatcherType {
  ALL = 0,
  PARTICULAR,
  NOWHERE
};

// JSON/JS sources:
const char kManifest[] =
    "{\n"
    "  \"name\": \"Test DeclarativeContentScript\",\n"
    "  \"manifest_version\": 2,\n"
    "  \"version\": \"1.0\",\n"
    "  \"description\": \"Test declarative content script interface\",\n"
    "  \"permissions\": [\"declarativeContent\", \"%s\"],\n"
    "  \"background\": {\n"
    "    \"scripts\": [\"background.js\"]\n"
    "  }\n"
    "}\n";
const char kBackgroundScriptSource[] =
    "var declarativeContent = chrome.declarativeContent;\n"
    "var PageStateMatcher = declarativeContent.PageStateMatcher;\n"
    "var RequestContentScript = declarativeContent.RequestContentScript;\n"
    "var onPageChanged = declarativeContent.onPageChanged;\n"
    "onPageChanged.removeRules(undefined, function() {\n"
    "  onPageChanged.addRules(\n"
    "      [{\n"
    "        conditions: [new PageStateMatcher(%s)],\n"
    "        actions: [new RequestContentScript({js: ['script.js']}\n"
    "        )]\n"
    "      }],\n"
    "      function(details) {\n"
    "        if (!chrome.runtime.lastError)\n"
    "          chrome.test.sendMessage('injection setup');\n"
    "      }\n"
    "  );\n"
    "});\n";
const char kContentScriptSource[] =
    "chrome.test.sendMessage('injection succeeded');\n";

// Messages from scripts:
const char kInjectionSetup[] = "injection setup";
const char kInjectionSucceeded[] = "injection succeeded";

// Runs all pending tasks in the renderer associated with |web_contents|.
// Returns true on success.
bool RunAllPendingInRenderer(content::WebContents* web_contents) {
  // TODO(devlin): If too many tests start to need this, move it somewhere
  // common.
  // This is slight hack to achieve a RunPendingInRenderer() method. Since IPCs
  // are sent synchronously, anything started prior to this method will finish
  // before this method returns (as content::ExecJs() is synchronous).
  return content::ExecJs(web_contents, "1 == 1;");
}

}  // namespace

class RequestContentScriptAPITest : public ExtensionBrowserTest {
 public:
  RequestContentScriptAPITest() = default;
  ~RequestContentScriptAPITest() override = default;

  // Performs script injection test on a common local URL using the given
  // |manifest_permission| and |script_matcher|. Does not return until
  // the renderer should have completed its task and any browser-side reactions
  // have been cleared from the task queue.
  testing::AssertionResult RunTest(PermissionOrMatcherType manifest_permission,
                                   PermissionOrMatcherType script_matcher,
                                   bool should_inject);

 protected:
  void TearDownOnMainThread() override {
    extension_ = nullptr;
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  testing::AssertionResult CreateAndLoadExtension(
      PermissionOrMatcherType manifest_permission,
      PermissionOrMatcherType script_matcher);

  const Extension* extension() const { return extension_.get(); }

 private:
  std::unique_ptr<TestExtensionDir> test_extension_dir_;
  raw_ptr<const Extension> extension_ = nullptr;
};

testing::AssertionResult RequestContentScriptAPITest::RunTest(
    PermissionOrMatcherType manifest_permission,
    PermissionOrMatcherType script_matcher,
    bool should_inject) {
  testing::AssertionResult result = CreateAndLoadExtension(manifest_permission,
                                                           script_matcher);
  if (!result) {
    return result;
  }

  // Setup listener for actual injection of script.
  ExtensionTestMessageListener injection_succeeded_listener(
      kInjectionSucceeded);
  injection_succeeded_listener.set_extension_id(extension_->id());

  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return testing::AssertionFailure() << "No web contents.";
  }

  EXPECT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                              "/extensions/test_file.html")));

  // Give the extension plenty of time to inject.
  if (!RunAllPendingInRenderer(web_contents)) {
    return testing::AssertionFailure() << "Could not run pending in renderer.";
  }

  // Make sure all running tasks are complete.
  content::RunAllPendingInMessageLoop();

  if (injection_succeeded_listener.was_satisfied() != should_inject) {
    return testing::AssertionFailure()
        << (should_inject ?
            "Expected injection, but got none." :
            "Expected no injection, but got one.");
  }

  if (extension_) {
    ExtensionId extension_id = extension_->id();
    // Avoid dangling pointers by clearing `extension_` before unloading.
    extension_ = nullptr;
    UnloadExtension(extension_id);
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult RequestContentScriptAPITest::CreateAndLoadExtension(
    PermissionOrMatcherType manifest_permission,
    PermissionOrMatcherType script_matcher) {
  // Setup a listener to note when injection rules have been setup.
  ExtensionTestMessageListener injection_setup_listener(kInjectionSetup);

  std::string manifest = base::StringPrintf(kManifest,
                                            kPermissions[manifest_permission]);
  std::string background_src = base::StringPrintf(
      kBackgroundScriptSource,
      kScriptMatchers[script_matcher]);

  auto dir = std::make_unique<TestExtensionDir>();
  dir->WriteManifest(manifest);
  dir->WriteFile(FILE_PATH_LITERAL("background.js"), background_src);
  dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                 kContentScriptSource);

  const Extension* extension = LoadExtension(dir->UnpackedPath());
  if (!extension) {
    return testing::AssertionFailure() << "Failed to load extension.";
  }

  test_extension_dir_ = std::move(dir);
  extension_ = extension;

  // Wait for rules to be setup before navigating to trigger script injection.
  EXPECT_TRUE(injection_setup_listener.WaitUntilSatisfied());

  return testing::AssertionSuccess();
}

// Try different permutations of "match all", "match particular domain (that is
// visited by test)", and "match nonsense domain (not visited by test)" for
// both manifest permissions and injection matcher conditions.
// http://crbug.com/41135960
IN_PROC_BROWSER_TEST_F(RequestContentScriptAPITest,
                       DISABLED_PermissionMatcherAgreementInjection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Positive tests: permissions and matcher contain conditions that match URL
  // visited during test.
  EXPECT_TRUE(RunTest(ALL, ALL, true));
  EXPECT_TRUE(RunTest(ALL, PARTICULAR, true));
  EXPECT_TRUE(RunTest(PARTICULAR, ALL, true));
  EXPECT_TRUE(RunTest(PARTICULAR, PARTICULAR, true));

  // Negative tests: permissions or matcher (or both) contain conditions that
  // do not match URL visited during test.
  EXPECT_TRUE(RunTest(NOWHERE, ALL, false));
  EXPECT_TRUE(RunTest(NOWHERE, PARTICULAR, false));
  EXPECT_TRUE(RunTest(NOWHERE, NOWHERE, false));
  EXPECT_TRUE(RunTest(ALL, NOWHERE, false));
  EXPECT_TRUE(RunTest(PARTICULAR, NOWHERE, false));

  // TODO(markdittmer): Add more tests:
  // - Inject script with multiple files
  // - Inject multiple scripts
  // - Match on CSS selector conditions
  // - Match all frames in document containing frames
}

// Tests that when an extension's host permissions are withheld (e.g., site
// access set to "On click"), triggering a declarative content rule with
// `RequestContentScript` should not record script execution in
// `ScriptInjectionTracker`.
//
// The browser and renderer states should stay synchronized regarding whether
// extension code has executed. Without proper gating, a bug causes these states
// to desynchronize:
// In the browser process,
// `RequestContentScript::InstructRenderProcessToInject()` checks
// `PermissionsData::CanAccessPage()`, which returns `true` even when host
// permissions are withheld (`PermissionsData::PageAccess::kWithheld`). This
// causes the browser to invoke `ScriptInjectionTracker::WillExecuteCode()`,
// erroneously marking the renderer process as having executed the content
// script.
// In the renderer process,
// `extensions::mojom::LocalFrame::ExecuteDeclarativeScript()` detects that host
// permissions are withheld and defers injection without running any script.
//
// As a result of this bug, the browser process considers the renderer
// authorized to act on behalf of the extension, even though no extension code
// ever executed in that renderer.
//
// Currently, this test documents the existing buggy behavior by expecting
// `ScriptInjectionTracker::DidProcessRunContentScriptFromExtension()` to return
// `true` so the test passes before the fix.
IN_PROC_BROWSER_TEST_F(RequestContentScriptAPITest,
                       WithheldPermissionsPrematurelyUpdatesTracker) {
  // Start the embedded test server to serve test pages.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up an unpacked extension that registers a declarative content rule with
  // `RequestContentScript` matching HTTP and HTTPS URLs.
  ASSERT_TRUE(CreateAndLoadExtension(/*manifest_permission=*/ALL,
                                     /*script_matcher=*/ALL));

  // Withhold host permissions for the extension so that page access requires
  // explicit user permission.
  ScriptingPermissionsModifier(profile(), extension())
      .SetWithholdHostPermissions(/*withhold=*/true);

  // Set up a listener for script execution and navigate to a test URL on the
  // embedded test server.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  ExtensionTestMessageListener script_listener(kInjectionSucceeded);
  script_listener.set_extension_id(extension()->id());

  const GURL target_url =
      embedded_test_server()->GetURL("/extensions/test_file.html");
  ASSERT_TRUE(NavigateToURL(web_contents, target_url));

  content::RenderProcessHost* target_process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  ASSERT_TRUE(target_process);

  // Run pending tasks in renderer to allow any potential injection to complete.
  ASSERT_TRUE(RunAllPendingInRenderer(web_contents));

  // Verify that the extension's page access is withheld on the target URL.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            extension()->permissions_data()->GetPageAccess(
                target_url, ExtensionTabUtil::GetTabId(web_contents),
                /*error=*/nullptr));

  // Verify that the content script was not executed in the renderer.
  EXPECT_FALSE(script_listener.was_satisfied());

  // Verify that `ScriptInjectionTracker` records that the process ran a content
  // script from this extension. This currently returns `true` because of the
  // bug where `PermissionsData::CanAccessPage()` returns `true` for withheld
  // permissions, erroneously notifying `ScriptInjectionTracker` before sending
  // the injection message to the renderer.
  //
  // TODO(crbug.com/513486355): Once the bug is fixed in
  // `RequestContentScript::InstructRenderProcessToInject()`, rename this test
  // to `WithheldPermissionsDoNotUpdateTracker` and update this expectation to
  // verify that `ScriptInjectionTracker` does not record execution when
  // permissions are withheld:
  // EXPECT_FALSE(
  //     ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
  //         *target_process, extension()->id()));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *target_process, extension()->id()));
}

}  // namespace extensions
