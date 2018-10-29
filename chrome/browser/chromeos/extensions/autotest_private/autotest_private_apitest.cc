// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"

#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/extensions/autotest_private/autotest_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/arc/arc_util.h"

namespace extensions {

class AutotestPrivateApiTest : public ExtensionApiTest {
 public:
  AutotestPrivateApiTest() = default;
  ~AutotestPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Make ARC enabled for tests.
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutotestPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, AutotestPrivate) {
  // Turn on testing mode so we don't kill the browser.
  AutotestPrivateAPI::GetFactoryInstance()
      ->Get(browser()->profile())
      ->set_test_mode(true);
  ASSERT_TRUE(RunComponentExtensionTest("autotest_private")) << message_;
}

}  // namespace extensions
