// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
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

    // The userScripts API is only available to users in developer mode.
    util::SetDeveloperModeForProfile(profile(), true);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  // The userScripts API is currently behind a channel and feature restriction.
  // TODO(crbug.com/1472902): Remove channel override when user scripts API goes
  // to stable.
  ScopedCurrentChannel current_channel_override_{
      version_info::Channel::UNKNOWN};
  base::test::ScopedFeatureList scoped_feature_list_;
};

UserScriptsAPITest::UserScriptsAPITest() {
  scoped_feature_list_.InitWithFeatures(
      {extensions_features::kApiUserScripts,
       // Also enable the dev mode restriction feature to gate the API on
       // developer mode.
       // TODO(https://crbug.com/1495451): Remove this when the feature is
       // enabled by default.
       extensions_features::kRestrictDeveloperModeAPIs},
      /*disabled_features=*/{});
}

// TODO(crbug.com/1491361): Flaky on Linux debug.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
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

// A test suite that runs without developer mode enabled.
class UserScriptsAPITestWithoutDeveloperMode : public UserScriptsAPITest {
 public:
  UserScriptsAPITestWithoutDeveloperMode() = default;
  UserScriptsAPITestWithoutDeveloperMode(
      const UserScriptsAPITestWithoutDeveloperMode&) = delete;
  UserScriptsAPITestWithoutDeveloperMode& operator=(
      const UserScriptsAPITestWithoutDeveloperMode&) = delete;
  ~UserScriptsAPITestWithoutDeveloperMode() override = default;

  void SetUpOnMainThread() override {
    // Note: We explicitly do *not* call UserScriptsAPITest::SetUpOnMainThread()
    // here. This ensures the user is not in developer mode.
    ExtensionApiTest::SetUpOnMainThread();
  }
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

}  // namespace extensions
