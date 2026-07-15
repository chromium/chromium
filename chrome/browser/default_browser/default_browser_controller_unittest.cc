// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

namespace {

class MockDefaultBrowserSetter : public DefaultBrowserSetter {
 public:
  MockDefaultBrowserSetter() = default;
  ~MockDefaultBrowserSetter() override = default;

  MOCK_METHOD(DefaultBrowserSetterType, GetType, (), (const override));
  MOCK_METHOD(void,
              Execute,
              (DefaultBrowserSetterCompletionCallback, const ExecuteParams&),
              (override));
};

using StrictlyMockedDefaultBrowserSetter =
    testing::StrictMock<MockDefaultBrowserSetter>;

}  // namespace

class DefaultBrowserControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    auto setter = std::make_unique<StrictlyMockedDefaultBrowserSetter>();
    setter_ = setter.get();

    controller_ = std::make_unique<DefaultBrowserController>(
        std::move(setter), DefaultBrowserEntrypointType::kSettingsPage);

    EXPECT_CALL(*setter_, GetType)
        .WillRepeatedly(
            testing::Return(DefaultBrowserSetterType::kShellIntegration));
  }

  void TearDown() override { setter_ = nullptr; }

  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<StrictlyMockedDefaultBrowserSetter> setter_;
  std::unique_ptr<DefaultBrowserController> controller_;
};

TEST_F(DefaultBrowserControllerTest, OnShown) {
  base::HistogramTester histogram_tester;
  controller_->OnShown();

  histogram_tester.ExpectTotalCount(
      "DefaultBrowser.SettingsPage.ShellIntegration.Shown", 1);
}

TEST_F(DefaultBrowserControllerTest, OnIgnored) {
  base::HistogramTester histogram_tester;
  controller_->OnIgnored();

  histogram_tester.ExpectUniqueSample(
      "DefaultBrowser.SettingsPage.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kIgnored, 1);
}

TEST_F(DefaultBrowserControllerTest, OnDismissed) {
  base::HistogramTester histogram_tester;
  controller_->OnDismissed();

  histogram_tester.ExpectUniqueSample(
      "DefaultBrowser.SettingsPage.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kDismissed, 1);
}

TEST_F(DefaultBrowserControllerTest, OnAcceptedSuccess) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<DefaultBrowserState> future;
  DefaultBrowserState state = DefaultBrowserState::IS_DEFAULT;

  EXPECT_CALL(*setter_, Execute(testing::_, testing::_))
      .WillOnce([state](DefaultBrowserSetterCompletionCallback callback,
                        const DefaultBrowserSetter::ExecuteParams& params) {
        std::move(callback).Run(state);
      });

  controller_->OnAccepted(future.GetCallback());

  ASSERT_EQ(future.Get(), state);
  histogram_tester.ExpectUniqueSample(
      "DefaultBrowser.SettingsPage.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kAccepted, 1);

  histogram_tester.ExpectUniqueSample("DefaultBrowser.ShellIntegration.Result",
                                      true, 1);

  histogram_tester.ExpectTotalCount(
      "DefaultBrowser.ShellIntegration.SuccessDuration", 1);
}

TEST_F(DefaultBrowserControllerTest, OnAcceptedFailure) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<DefaultBrowserState> future;
  DefaultBrowserState state = DefaultBrowserState::NOT_DEFAULT;

  EXPECT_CALL(*setter_, Execute(testing::_, testing::_))
      .WillOnce([state](DefaultBrowserSetterCompletionCallback callback,
                        const DefaultBrowserSetter::ExecuteParams& params) {
        std::move(callback).Run(state);
      });

  controller_->OnAccepted(future.GetCallback());

  ASSERT_EQ(future.Get(), state);
  histogram_tester.ExpectUniqueSample(
      "DefaultBrowser.SettingsPage.ShellIntegration.Interaction",
      DefaultBrowserInteractionType::kAccepted, 1);

  histogram_tester.ExpectUniqueSample("DefaultBrowser.ShellIntegration.Result",
                                      false, 1);

  histogram_tester.ExpectTotalCount(
      "DefaultBrowser.ShellIntegration.SuccessDuration", 0);
}

// Checks that there is no crash when OnAccepted is called second time before
// the first execution have finished.
TEST_F(DefaultBrowserControllerTest, ConcurrentOnAcceptedCompletesBoth) {
  base::test::TestFuture<DefaultBrowserState> first_future;
  base::test::TestFuture<DefaultBrowserState> second_future;

  std::vector<DefaultBrowserSetterCompletionCallback> captured_callbacks;
  EXPECT_CALL(*setter_, Execute(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly(
          [&captured_callbacks](DefaultBrowserSetterCompletionCallback cb,
                                const DefaultBrowserSetter::ExecuteParams&) {
            captured_callbacks.push_back(std::move(cb));
          });

  controller_->OnAccepted(first_future.GetCallback());
  controller_->OnAccepted(second_future.GetCallback());

  ASSERT_EQ(captured_callbacks.size(), 2u);
  std::move(captured_callbacks[0]).Run(DefaultBrowserState::IS_DEFAULT);
  std::move(captured_callbacks[1]).Run(DefaultBrowserState::NOT_DEFAULT);

  EXPECT_EQ(first_future.Get(), DefaultBrowserState::IS_DEFAULT);
  EXPECT_EQ(second_future.Get(), DefaultBrowserState::NOT_DEFAULT);
}

}  // namespace default_browser
