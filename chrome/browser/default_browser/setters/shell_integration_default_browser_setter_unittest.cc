// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"

#include "base/test/test_future.h"
#include "chrome/browser/shell_integration.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

TEST(ShellIntegrationDefaultBrowserSetterTest, Execute) {
  content::BrowserTaskEnvironment task_environment_;
  shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();

  ShellIntegrationDefaultBrowserSetter setter;
  base::test::TestFuture<DefaultBrowserState> future;

  setter.Execute(future.GetCallback(), /*params*/ {});

  ASSERT_TRUE(future.Wait())
      << "Callback should be called after setter executes";
  EXPECT_LT(future.Get(), DefaultBrowserState::NUM_DEFAULT_STATES);
}

// Checks that there is no crash when Execute is called second time before the
// first worker have finished.
TEST(ShellIntegrationDefaultBrowserSetterTest, ConcurrentExecuteCompletesBoth) {
  content::BrowserTaskEnvironment task_environment_;
  shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();

  ShellIntegrationDefaultBrowserSetter setter;
  base::test::TestFuture<DefaultBrowserState> first_future;
  base::test::TestFuture<DefaultBrowserState> second_future;

  DefaultBrowserSetter::ExecuteParams params;
  setter.Execute(first_future.GetCallback(), params);
  setter.Execute(second_future.GetCallback(), params);

  EXPECT_TRUE(first_future.Wait())
      << "First operation's callback must be invoked";
  EXPECT_TRUE(second_future.Wait())
      << "Second operation's callback must be invoked";
  EXPECT_LT(first_future.Get(), DefaultBrowserState::NUM_DEFAULT_STATES);
  EXPECT_LT(second_future.Get(), DefaultBrowserState::NUM_DEFAULT_STATES);
}

}  // namespace default_browser
