// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

TEST(DefaultBrowserManagerTest, GetDefaultBrowserState) {
  content::BrowserTaskEnvironment task_environment;

  base::test::TestFuture<DefaultBrowserState> future;
  DefaultBrowserManager::GetDefaultBrowserState(future.GetCallback());

  ASSERT_TRUE(future.Wait()) << "GetDefaultBrowserState should trigger the "
                                "callback after fetching default browser state";
  EXPECT_LT(future.Get(), DefaultBrowserState::NUM_DEFAULT_STATES);
}

TEST(DefaultBrowserManagerTest, CreateControllerForSettingsPage) {
  auto controller = DefaultBrowserManager::CreateControllerFor(
      DefaultBrowserEntrypointType::kSettingsPage);

  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->GetSetterType(),
            DefaultBrowserSetterType::kShellIntegration);
}

TEST(DefaultBrowserManagerTest, CreateControllerForStartupInfobar) {
  auto controller = DefaultBrowserManager::CreateControllerFor(
      DefaultBrowserEntrypointType::kStartupInfobar);

  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(controller->GetSetterType(),
            DefaultBrowserSetterType::kShellIntegration);
}

}  // namespace default_browser
