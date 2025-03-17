// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/user_scripts_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/user_scripts_allowed_state.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UserScriptsAPITest : public ExtensionApiTest,
                           public testing::WithParamInterface<bool> {
 public:
  UserScriptsAPITest();
  UserScriptsAPITest(const UserScriptsAPITest&) = delete;
  const UserScriptsAPITest& operator=(const UserScriptsAPITest&) = delete;
  ~UserScriptsAPITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  void OpenInCurrentTab(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    nav_observer.Wait();

    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  }

  content::RenderFrameHost* OpenInNewTab(const GURL& url) {
    content::TestNavigationObserver nav_observer(url);
    nav_observer.StartWatchingNewWebContents();
    content::RenderFrameHost* tab = ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    nav_observer.Wait();

    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(url, browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetLastCommittedURL());

    return tab;
  }

  content::EvalJsResult GetInjectedElements(content::RenderFrameHost* host) {
    static constexpr char kGetInjectedScripts[] =
        R"(const divs = document.body.getElementsByTagName('div');
           JSON.stringify(Array.from(divs).map(div => div.id).sort());)";
    return content::EvalJs(host, kGetInjectedScripts);
  }

  // Loads the extension and pauses in-between loading and running the tests to
  // enable the userScripts API.
  testing::AssertionResult RunUserScriptsExtensionTestImpl(
      const base::FilePath& extension_path,
      bool allow_api) {
    // Load the extension.
    ExtensionTestMessageListener test_ready_listener(
        "ready",
        allow_api ? ReplyBehavior::kWillReply : ReplyBehavior::kWontReply);
    ResultCatcher catcher;
    const Extension* extension = LoadExtension(extension_path);
    if (!extension) {
      return testing::AssertionFailure() << "Failed to load extension";
    }

    if (allow_api) {
      // Wait until extension tests are ready to run, then allow the
      // userScripts API, then continue on with the API testing.
      bool extension_ready = test_ready_listener.WaitUntilSatisfied();
      if (!extension_ready) {
        testing::AssertionFailure()
            << "extension did not signal that it was ready after loading";
      }
      user_scripts_test_util::SetUserScriptsAPIAllowed(profile(),
                                                       extension->id(),
                                                       /*allowed=*/true);
      test_ready_listener.Reply("");
    }

    // Observe each test result.
    {
      base::test::ScopedRunLoopTimeout timeout(
          FROM_HERE, std::nullopt,
          base::BindRepeating(
              [](const base::FilePath& extension_path) {
                return "GetNextResult timeout while "
                       "RunUserScriptsExtensionTest: " +
                       extension_path.MaybeAsASCII();
              },
              extension_path));
      if (!catcher.GetNextResult()) {
        return testing::AssertionFailure() << catcher.message();
      }
    }

    return testing::AssertionSuccess();
    ;
  }

  testing::AssertionResult RunUserScriptsExtensionTest(
      const char* extension_name) {
    const base::FilePath& root_path = test_data_dir_;
    base::FilePath extension_path = root_path.AppendASCII(extension_name);
    return RunUserScriptsExtensionTestImpl(extension_path, /*allow_api=*/true);
  }

  testing::AssertionResult RunUserScriptsExtensionTest(
      const base::FilePath& extension_path) {
    return RunUserScriptsExtensionTestImpl(extension_path, /*allow_api=*/true);
  }

  testing::AssertionResult RunUserScriptsExtensionTestNotAllowed(
      const base::FilePath& extension_path) {
    return RunUserScriptsExtensionTestImpl(extension_path, /*allow_api=*/false);
  }

 private:
  // Some userScripts API methods are currently behind a feature restriction.
  base::test::ScopedFeatureList scoped_feature_list_;
};

UserScriptsAPITest::UserScriptsAPITest() {
  if (GetParam()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {extensions_features::kApiUserScriptsMultipleWorlds,
         extensions_features::kApiUserScriptsExecute,
         extensions_features::kUserScriptUserExtensionToggle},
        /*disabled_features=*/{});
  } else {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::
                                  kApiUserScriptsMultipleWorlds,
                              extensions_features::kApiUserScriptsExecute},
        /*disabled_features=*/{
            extensions_features::kUserScriptUserExtensionToggle});
  }
}

// TODO(crbug.com/40935741, crbug.com/335421977): Flaky on Linux debug and on
// "Linux ChromiumOS MSan Tests".
#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || \
    (BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER))
#define MAYBE_RegisterUserScripts DISABLED_RegisterUserScripts
#else
#define MAYBE_RegisterUserScripts RegisterUserScripts
#endif
IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, MAYBE_RegisterUserScripts) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/register")) << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, GetUserScripts) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/get_scripts"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, UnregisterUserScripts) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/unregister"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, UpdateUserScripts) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/update")) << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, ExecuteUserScripts) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/execute")) << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, ExecuteUserScripts_Subframes) {
  // Open up two tabs, each with cross-site iframes, one at a.com and one at
  // d.com. In both cases, the cross-site iframes point to b.com and c.com.
  OpenInCurrentTab(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  OpenInNewTab(
      embedded_test_server()->GetURL("d.com", "/iframe_cross_site.html"));

  ASSERT_TRUE(
      RunUserScriptsExtensionTest("user_scripts/execute_with_subframes"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, ExecuteUserScripts_SizeLimit) {
  auto single_scripts_limit_reset =
      script_parsing::CreateScopedMaxScriptLengthForTesting(700u);
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/execute_size_limit"))
      << message_;
}

// TODO(crbug.com/335421977): Flaky on "Linux ChromiumOS MSan Tests".
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_ConfigureWorld DISABLED_ConfigureWorld
#else
#define MAYBE_ConfigureWorld ConfigureWorld
#endif
IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, MAYBE_ConfigureWorld) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/configure_world"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest, GetAndRemoveWorlds) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/get_and_remove_worlds"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITest,
                       UserScriptInjectionOrderIsAlphabetical) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("user_scripts/injection_order"))
      << message_;
}

// Tests that registered user scripts are disabled when the userScripts API is
// not allowed and are re-enabled if when the API is allowed again.
IN_PROC_BROWSER_TEST_P(UserScriptsAPITest,
                       UserScriptsAreDisabledWhenAPIIsNotAllowed) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("user_scripts/allowed_tests"));
  ASSERT_TRUE(extension);
  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/true);

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

  // Since the userScript API is available (as part of this test suite's setup),
  // both the user script and the content script should inject.
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));

  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/false);

  // Open a new tab. Now, user scripts should be disabled. However, content
  // scripts should still inject.
  new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script"])", GetInjectedElements(new_tab));

  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/true);

  // Open a new tab. The user script should inject again.
  new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));
}

// Tests that unregisterContentScripts unregisters only content scripts and
// not user scripts.
IN_PROC_BROWSER_TEST_P(UserScriptsAPITest,
                       ScriptingAPIDoesNotAffectUserScripts) {
  ASSERT_TRUE(RunUserScriptsExtensionTest("scripting/dynamic_user_scripts"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserScriptsAPITest,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Bool());

// Base test fixture for tests spanning multiple sessions where a custom arg
// is set before the test is run.
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
IN_PROC_BROWSER_TEST_P(PersistentUserScriptsAPITest,
                       PRE_PRE_PersistentScripts) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("user_scripts/persistent_scripts"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/true);
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_P(PersistentUserScriptsAPITest, PRE_PersistentScripts) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_P(PersistentUserScriptsAPITest, PersistentScripts) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

// Tests that the world configuration of a registered user script is persisted
// across sessions. The test is run across three sessions.
IN_PROC_BROWSER_TEST_P(PersistentUserScriptsAPITest,
                       PRE_PRE_PersistentWorldConfiguration) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("user_scripts/persistent_configure_world"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/true);
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_P(PersistentUserScriptsAPITest,
                       PRE_PersistentWorldConfiguration) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_P(PersistentUserScriptsAPITest,
                       PersistentWorldConfiguration) {
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PersistentUserScriptsAPITest,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Bool());

// A test suite that runs without developer mode enabled.
class UserScriptsAPITestWithoutDeveloperMode : public UserScriptsAPITest {
 public:
  UserScriptsAPITestWithoutDeveloperMode() = default;
  UserScriptsAPITestWithoutDeveloperMode(
      const UserScriptsAPITestWithoutDeveloperMode&) = delete;
  UserScriptsAPITestWithoutDeveloperMode& operator=(
      const UserScriptsAPITestWithoutDeveloperMode&) = delete;
  ~UserScriptsAPITestWithoutDeveloperMode() override = default;
};

// TODO(crbug.com/390138269): Remove this test once the per-extension toggle is
// enabled by default since the API will no longer be controlled by dev mode.
// Verifies that the `chrome.userScripts` API is unavailable if the user doesn't
// have dev mode turned on.
IN_PROC_BROWSER_TEST_P(UserScriptsAPITestWithoutDeveloperMode,
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

  ASSERT_TRUE(RunUserScriptsExtensionTestNotAllowed(test_dir.UnpackedPath()))
      << message_;
}

// This tests inherits from a test that uses GetParam() so the test must be
// parameterized, even if there's only one test case to run).
INSTANTIATE_TEST_SUITE_P(All,
                         UserScriptsAPITestWithoutDeveloperMode,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Values(false));

using UserScriptsAPITestWithoutUserAllowed =
    UserScriptsAPITestWithoutDeveloperMode;

// Verifies that the `chrome.userScripts` API is undefined if the API is not
// allowed yet.
IN_PROC_BROWSER_TEST_P(UserScriptsAPITestWithoutUserAllowed,
                       UserScriptsAPIIsUndefinedWithoutAPIAllowed) {
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
           function userScriptsIsUndefined() {
             chrome.test.assertTrue(chrome.userScripts === undefined);
             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  ASSERT_TRUE(RunUserScriptsExtensionTestNotAllowed(test_dir.UnpackedPath()))
      << message_;
}

// This tests inherits from a test that uses GetParam() so the test must be
// parameterized, even if there's only one test case to run).
INSTANTIATE_TEST_SUITE_P(All,
                         UserScriptsAPITestWithoutUserAllowed,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Values(true));

class UserScriptsAPITestWithoutAPIAllowed : public UserScriptsAPITest {
 public:
  UserScriptsAPITestWithoutAPIAllowed() = default;

  // UserScriptsAPITest override.
  void SetUp() override {
    // Initialize the listener object here before calling SetUp. This avoids a
    // race condition where the extension loads (as part of browser startup) and
    // sends a message before a message listener in C++ has been initialized.
    background_started_listener_ =
        std::make_unique<ExtensionTestMessageListener>("started");

    UserScriptsAPITest::SetUp();
  }

  // Reset listener before the browser gets torn down.
  void TearDown() override {
    background_started_listener_.reset();
    UserScriptsAPITest::TearDown();
  }

 protected:
  std::unique_ptr<ExtensionTestMessageListener> background_started_listener_;
};

// Tests that registered user scripts are properly ignored when loading
// stored dynamic scripts if the API is not allowed.
IN_PROC_BROWSER_TEST_P(UserScriptsAPITestWithoutAPIAllowed,
                       PRE_UserScriptsDisabledOnStartupIfAPINotAllowed) {
  // Load an extension and register user scripts and a dynamic content script.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("user_scripts/allowed_tests"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(background_started_listener_->WaitUntilSatisfied());
  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/true);

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

  // Disallow userScript API, and then re-open the browser...
  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension->id(),
                                                   /*allowed=*/false);
}

IN_PROC_BROWSER_TEST_P(UserScriptsAPITestWithoutAPIAllowed,
                       UserScriptsDisabledOnStartupIfAPINotAllowed) {
  // Wait until the extension loads so we can get it's ID.
  ASSERT_TRUE(background_started_listener_->WaitUntilSatisfied());

  // Find the extension's ID so we can make some assertions.
  ExtensionId extension_id;
  for (const auto& extension :
       ExtensionRegistry::Get(profile())->enabled_extensions()) {
    if (extension->name() == "Test") {
      extension_id = extension->id();
    }
  }
  ASSERT_TRUE(!extension_id.empty());

  // userScripts should remain disallowed after browser restart.
  if (GetParam()) {
    EXPECT_FALSE(GetCurrentUserScriptAllowedState(
        util::GetBrowserContextId(profile()), extension_id));
  } else {
    EXPECT_FALSE(GetCurrentDeveloperMode(util::GetBrowserContextId(profile())));
  }

  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  // And, to start, only the content script should inject.
  content::RenderFrameHost* new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script"])", GetInjectedElements(new_tab));

  user_scripts_test_util::SetUserScriptsAPIAllowed(profile(), extension_id,
                                                   /*allowed=*/true);

  // All scripts should once again inject.
  new_tab = OpenInNewTab(url);
  EXPECT_EQ(R"(["content-script","user-script-code","user-script-file"])",
            GetInjectedElements(new_tab));
}

INSTANTIATE_TEST_SUITE_P(All,
                         UserScriptsAPITestWithoutAPIAllowed,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Bool());

// TODO(crbug.com/390138269): Write a test that confirms that enabling the API
// for an extension in one profile doesn't enable it for the same extension in
// another profile. Also write tests to confirm incognito split/span mode
// behavior.

}  // namespace extensions
