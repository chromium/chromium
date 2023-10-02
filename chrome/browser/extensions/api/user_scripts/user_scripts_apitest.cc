// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
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
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kApiUserScripts);
}

IN_PROC_BROWSER_TEST_F(UserScriptsAPITest, RegisterUserScripts) {
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
  LOG(INFO) << "PRE_PRE_PersistentScripts";
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("user_scripts/persistent_scripts"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest, PRE_PersistentScripts) {
  LOG(INFO) << "PRE_PersistentScripts";
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentUserScriptsAPITest, PersistentScripts) {
  LOG(INFO) << "PersistentScripts";
  ASSERT_TRUE(listener_->WaitUntilSatisfied());
  listener_->Reply(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

}  // namespace extensions
