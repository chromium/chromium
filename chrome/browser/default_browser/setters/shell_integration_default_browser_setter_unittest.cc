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

  setter.Execute(future.GetCallback());

  ASSERT_TRUE(future.Wait())
      << "Callback should be called after setter executes";
  EXPECT_LT(future.Get(), DefaultBrowserState::NUM_DEFAULT_STATES);
}

}  // namespace default_browser
