// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extensions_container.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksExtensionsContainerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
  }

  void TearDown() override {
    browser_window_.reset();
    profile_.reset();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_window_;
};

TEST_F(ContextualTasksExtensionsContainerTest, GetAndSetActiveWebContents) {
  auto web_contents1 = CreateTestWebContents();
  auto web_contents2 = CreateTestWebContents();

  ContextualTasksExtensionsContainer container(browser_window_.get(),
                                               web_contents1.get());
  EXPECT_EQ(container.GetActiveWebContents(), web_contents1.get());

  container.SetWebContents(web_contents2.get());
  EXPECT_EQ(container.GetActiveWebContents(), web_contents2.get());

  container.SetWebContents(nullptr);
  EXPECT_EQ(container.GetActiveWebContents(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, WebContentsWeakPtrLifecycle) {
  auto web_contents = CreateTestWebContents();
  ContextualTasksExtensionsContainer container(browser_window_.get(),
                                               web_contents.get());
  EXPECT_EQ(container.GetActiveWebContents(), web_contents.get());

  // Destroying the WebContents should safely clear the weak pointer.
  web_contents.reset();
  EXPECT_EQ(container.GetActiveWebContents(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, ContainerDefaults) {
  auto web_contents = CreateTestWebContents();
  ContextualTasksExtensionsContainer container(browser_window_.get(),
                                               web_contents.get());

  EXPECT_TRUE(container.IsVisible());
  EXPECT_FALSE(container.HasAnyExtensions());
  EXPECT_EQ(container.GetActionForId("test_extension"), nullptr);
  EXPECT_FALSE(container.IsActionVisibleOnToolbar("test_extension"));
  EXPECT_EQ(container.GetPoppedOutActionId(), std::nullopt);
  EXPECT_FALSE(container.ShowToolbarActionPopupForAPICall("test_extension",
                                                          base::DoNothing()));
}

TEST_F(ContextualTasksExtensionsContainerTest, PopOutActionRunsCallback) {
  auto web_contents = CreateTestWebContents();
  ContextualTasksExtensionsContainer container(browser_window_.get(),
                                               web_contents.get());

  bool called = false;
  container.PopOutAction("test_extension",
                         base::BindLambdaForTesting([&]() { called = true; }));
  EXPECT_TRUE(called);
}

TEST_F(ContextualTasksExtensionsContainerTest, GetPopupArrow) {
  auto web_contents = CreateTestWebContents();
  ContextualTasksExtensionsContainer container(browser_window_.get(),
                                               web_contents.get());
  EXPECT_EQ(container.GetPopupArrow(), views::BubbleBorder::TOP_LEFT);
}

}  // namespace contextual_tasks
