// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class UserScriptsAPITest : public ExtensionApiTest {
 public:
  UserScriptsAPITest();
  UserScriptsAPITest(const UserScriptsAPITest&) = delete;
  const UserScriptsAPITest& operator=(const UserScriptsAPITest&) = delete;
  ~UserScriptsAPITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    if (ShouldEnableDevMode()) {
      util::SetDeveloperModeForProfile(profile(), true);
    }

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  content::RenderFrameHost* OpenInNewTab(const GURL& url) {
    return ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  content::EvalJsResult GetInjectedElements(content::RenderFrameHost* host) {
    static constexpr char kGetInjectedScripts[] =
        R"(const divs = document.body.getElementsByTagName('div');
           JSON.stringify(Array.from(divs).map(div => div.id).sort());)";
    return content::EvalJs(host, kGetInjectedScripts);
  }

 private:
  // Whether to enable developer mode at the start of the test. We do this
  // for most tests because the `userScripts` API is restricted to dev mode.
  virtual bool ShouldEnableDevMode() { return true; }

  // The userScripts API is currently behind a feature restriction.
  // TODO(crbug.com/40926805): Remove once the feature is stable for awhile.
  base::test::ScopedFeatureList scoped_feature_list_;
};

UserScriptsAPITest::UserScriptsAPITest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kApiUserScriptsMultipleWorlds);
}

// TODO(crbug.com/40935741, crbug.com/335421977): Flaky on Linux debug and on
// "Linux ChromiumOS MSan Tests".
#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || \
    (BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER))
#define MAYBE_RegisterUserScripts DISABLED_RegisterUserScripts
#else
#define MAYBE_RegisterUserScripts RegisterUserScripts
#endif
IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, MAYBE_RegisterUserScripts) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/register")) << message_;
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, GetUserScripts) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/get_scripts")) << message_;
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, UnregisterUserScripts) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/unregister")) << message_;
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, UpdateUserScripts) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/update")) << message_;
}

// TODO(crbug.com/335421977): Flaky on "Linux ChromiumOS MSan Tests".
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_ConfigureWorld DISABLED_ConfigureWorld
#else
#define MAYBE_ConfigureWorld ConfigureWorld
#endif
IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, MAYBE_ConfigureWorld) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/configure_world")) << message_;
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, GetAndRemoveWorlds) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/get_and_remove_worlds"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITest,
                       UserScriptInjectionOrderIsAlphabetical) {
  ASSERT_TRUE(RunExtensionTest("user_scripts/injection_order")) << message_;
}

// Tests that registered user scripts are disabled when dev mode is disabled and
// are re-enabled if dev mode is turned back on.
IN_PROC_BROWSER_TEST_F(UserScriptsAPITest,
                       UserScriptsAreDisabledWhenDevModeIsDisabled) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("user_scripts/dev_mode_tests"));
  ASSERT_TRUE(extension);

  // Register a user script and a content script.
  EXPECT_EQ("success",
            BackgroundScriptExecutor::ExecuteScript(
                profile(), extension->id(), "registerUserScripts();",
                BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
  EXPECT_EQ("success",
            BackgroundScriptExecutor::ExecuteScript(
                profile(), extension->id(), "registerContentScript();",
                BackgroundScriptExecutor::ResultCapture::kSendScriptResult));

  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  // Open a new tab.
  content::RenderFrameHost* new_tab = OpenInNewTab(url);

  // Since dev mode is enabled (as part of this test suite's setup), both the
  // user script and the content script should inject.
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));

  // Disable dev mode.
  util::SetDeveloperModeForProfile(profile(), false);

  // Open a new tab. Now, user scripts should be disabled. However, content
  // scripts should still inject.
  new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script"])", GetInjectedElements(new_tab));

  // Re-enable dev mode.
  util::SetDeveloperModeForProfile(profile(), true);

  // Open a new tab. The user script should inject again.
  new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));
}

// Base test fixture for tests spanning multiple sessions where a custom arg is
// set before the test is run.
class PersistentUserScriptsAPITest : public UserScriptsAPITest {
 public:
  PersistentUserScriptsAPITest() = default;

  // UserScriptsAPITest override.
  void SetUp() override {
    // Initialize the listener object here before calling SetUp. This avoids a
    // race condition where the extension loads (as part of browser startup) and
    // sends a message before a message listener in C++ has been initialized.
    listener_ = std::make_unique<ExtensionTestMessageListener>(
        "ready", ReplyBehavior::kWillReply);

    UserScriptsAPITest::SetUp();
  }

  // Reset listener before the browser gets torn down.
  void TearDownOnMainThread() override {
    listener_.reset();
    UserScriptsAPITest::TearDownOnMainThread();
  }

 protected:
  // Used to wait for results from extension tests. This is initialized before
  // the test is run which avoids a race condition where the extension is loaded
  // (as part of startup) and finishes its tests before the ResultCatcher is
  // created.
  ResultCatcher result_catcher_;

  // Used to wait for the extension to load and send a ready message so the test
  // can reply which the extension waits for to start its testing functions.
  // This ensures that the testing functions will run after the browser has
  // finished initializing.
  std::unique_ptr<ExtensionTestMessageListener> listener_;
};

// Tests that registered user scripts persist across sessions. The test is run
// across three sessions.
IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest,
                       PRE_PRE_PersistentScripts) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("user_scripts/persistent_scripts"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest, PRE_PersistentScripts) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest, PersistentScripts) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

// Tests that the world configuration of a registered user script is persisted
// across sessions. The test is run across three sessions.
IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest,
                       PRE_PRE_PersistentWorldConfiguration) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("user_scripts/persistent_configure_world"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest,
                       PRE_PersistentWorldConfiguration) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest,
                       PersistentWorldConfiguration) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

// A test suite that runs without developer mode enabled.
class UserScriptsAPITestWithoutDeveloperMode : public UserScriptsAPITest {
 public:
  UserScriptsAPITestWithoutDeveloperMode() = default;
  UserScriptsAPITestWithoutDeveloperMode(
      const UserScriptsAPITestWithoutDeveloperMode&) = delete;
  UserScriptsAPITestWithoutDeveloperMode& operator=(
      const UserScriptsAPITestWithoutDeveloperMode&) = delete;
  ~UserScriptsAPITestWithoutDeveloperMode() override = default;

 private:
  bool ShouldEnableDevMode() override { return false; }
};

// Verifies that the `chrome.userScripts` API is unavailable if the user doesn't
// have dev mode turned on.
IN_PROC_BROWSER_TEST_F(UserScriptsAPITestWithoutDeveloperMode,
                       UserScriptsAPIIsUnavailableWithoutDeveloperMode) {
  static constexpr char kManifest[] =
      R"({
           "name": "user scripts",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["userScripts"]
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function userScriptsIsUnavailable() {
             let caught = false;
             try {
               chrome.userScripts;
             } catch (e) {
               caught = true;
               const expectedError =
                   `Failed to read the 'userScripts' property from 'Object': ` +
                   `The 'userScripts' API is only available for ` +
                   `users in developer mode.`;
               chrome.test.assertEq(expectedError, e.message);
             }
             chrome.test.assertTrue(caught);
             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

// Tests that registered user scripts are properly ignored when loading
// stored dynamic scripts if developer mode is disabled.
IN_PROC_BROWSER_TEST_F(UserScriptsAPITestWithoutDeveloperMode,
                       PRE_UserScriptsDisabledOnStartupIfDevModeOff) {
  // Load an extension and register user scripts and a dynamic content script.
  util::SetDeveloperModeForProfile(profile(), true);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("user_scripts/dev_mode_tests"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("success",
            BackgroundScriptExecutor::ExecuteScript(
                profile(), extension->id(), "registerUserScripts();",
                BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
  EXPECT_EQ("success",
            BackgroundScriptExecutor::ExecuteScript(
                profile(), extension->id(), "registerContentScript();",
                BackgroundScriptExecutor::ResultCapture::kSendScriptResult));

  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  // To start, all scripts should inject.
  content::RenderFrameHost* new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));

  // Disable dev mode, and re-open the browser...
  util::SetDeveloperModeForProfile(profile(), false);
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITestWithoutDeveloperMode,
                       UserScriptsDisabledOnStartupIfDevModeOff) {
  // ... dev mode should remain disabled.
  EXPECT_FALSE(GetCurrentDeveloperMode(util::GetBrowserContextId(profile())));

  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  // And, to start, only the content script should inject.
  content::RenderFrameHost* new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script"])", GetInjectedElements(new_tab));

  // Enable dev mode.
  util::SetDeveloperModeForProfile(profile(), true);

  // All scripts should once again inject.
  new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));
}

}  // namespace extensions
