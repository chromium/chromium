// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_idle {

// TODO(crbug.com/40222234): Enable this when Android supports >1 Action.
#if !BUILDFLAG(IS_ANDROID)
TEST(IdleActionTest, Build) {
  auto* factory = ActionFactory::GetInstance();

  auto queue = factory->Build(
      nullptr, {ActionType::kCloseBrowsers, ActionType::kShowProfilePicker});
  EXPECT_EQ(2u, queue.size());
  EXPECT_EQ(static_cast<int>(ActionType::kCloseBrowsers),
            queue.top()->priority());
  queue.pop();
  EXPECT_EQ(static_cast<int>(ActionType::kShowProfilePicker),
            queue.top()->priority());

  queue = factory->Build(nullptr, {ActionType::kCloseBrowsers});
  EXPECT_EQ(1u, queue.size());
  EXPECT_EQ(static_cast<int>(ActionType::kCloseBrowsers),
            queue.top()->priority());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST(IdleActionTest, ClearBrowsingDataIsSingleAction) {
  auto* factory = ActionFactory::GetInstance();

  auto queue = factory->Build(nullptr, {
#if !BUILDFLAG(IS_ANDROID)
    ActionType::kClearDownloadHistory, ActionType::kClearHostedAppData,
#endif  // !BUILDFLAG(IS_ANDROID)
        ActionType::kClearBrowsingHistory,
        ActionType::kClearCookiesAndOtherSiteData,
        ActionType::kClearCachedImagesAndFiles,
        ActionType::kClearPasswordSignin, ActionType::kClearAutofill,
        ActionType::kClearSiteSettings
  });
  EXPECT_EQ(1u, queue.size());
  EXPECT_EQ(static_cast<int>(ActionType::kClearBrowsingHistory),
            queue.top()->priority());
}

}  // namespace enterprise_idle
