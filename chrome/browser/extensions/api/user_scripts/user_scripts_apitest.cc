// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
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

}  // namespace extensions
