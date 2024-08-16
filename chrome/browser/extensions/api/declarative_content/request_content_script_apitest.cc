// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Manifest permissions injected into |kManifest|:
const char* const kPermissions[] = {
  "*://*/*",              // ALL
  "http://127.0.0.1/*",   // PARTICULAR
  "http://nowhere.com/*"  // NOWHERE
};

// Script matchers for injected into |kBackgroundScriptSource|:
const char* const kScriptMatchers[] = {
  "{ pageUrl: { hostContains: '' } }",          // ALL
  "{ pageUrl: { hostEquals: '127.0.0.1' } }",   // PARTICULAR
  "{ pageUrl: { hostEquals: 'nowhere.com' } }"  // NOWHERE
};

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
  RequestContentScriptAPITest();
  ~RequestContentScriptAPITest() override {}

  // Performs script injection test on a common local URL using the given
  // |manifest_permission| and |script_matcher|. Does not return until
  // the renderer should have completed its task and any browser-side reactions
  // have been cleared from the task queue.
  testing::AssertionResult RunTest(PermissionOrMatcherType manifest_permission,
                                   PermissionOrMatcherType script_matcher,
                                   bool should_inject);

 private:
  testing::AssertionResult CreateAndLoadExtension(
      PermissionOrMatcherType manifest_permission,
      PermissionOrMatcherType script_matcher);

  std::unique_ptr<TestExtensionDir> test_extension_dir_;
  raw_ptr<const Extension> extension_;
};

RequestContentScriptAPITest::RequestContentScriptAPITest()
    : extension_(nullptr) {}

testing::AssertionResult RequestContentScriptAPITest::RunTest(
    PermissionOrMatcherType manifest_permission,
    PermissionOrMatcherType script_matcher,
    bool should_inject) {
  if (extension_)
    UnloadExtension(extension_->id());
  testing::AssertionResult result = CreateAndLoadExtension(manifest_permission,
                                                           script_matcher);
  if (!result)
    return result;

  // Setup listener for actual injection of script.
  ExtensionTestMessageListener injection_succeeded_listener(
      kInjectionSucceeded);
  injection_succeeded_listener.set_extension_id(extension_->id());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html")));

  content::WebContents* web_contents =
      browser() ? browser()->tab_strip_model()->GetActiveWebContents()
                : nullptr;
  if (!web_contents)
    return testing::AssertionFailure() << "No web contents.";

  // Give the extension plenty of time to inject.
  if (!RunAllPendingInRenderer(web_contents))
    return testing::AssertionFailure() << "Could not run pending in renderer.";

  // Make sure all running tasks are complete.
  content::RunAllPendingInMessageLoop();

  if (injection_succeeded_listener.was_satisfied() != should_inject) {
    return testing::AssertionFailure()
        << (should_inject ?
            "Expected injection, but got none." :
            "Expected no injection, but got one.");
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
  if (!extension)
    return testing::AssertionFailure() << "Failed to load extension.";

  test_extension_dir_ = std::move(dir);
  extension_ = extension;

  // Wait for rules to be setup before navigating to trigger script injection.
  EXPECT_TRUE(injection_setup_listener.WaitUntilSatisfied());

  return testing::AssertionSuccess();
}


// Try different permutations of "match all", "match particular domain (that is
// visited by test)", and "match nonsense domain (not visited by test)" for
// both manifest permissions and injection matcher conditions.
// http://crbug.com/421118
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

}  // namespace extensions
