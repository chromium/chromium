// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
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
              (DefaultBrowserSetterCompletionCallback),
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
  }

  void TearDown() override { setter_ = nullptr; }

  raw_ptr<StrictlyMockedDefaultBrowserSetter> setter_;
  std::unique_ptr<DefaultBrowserController> controller_;
};

TEST_F(DefaultBrowserControllerTest, OnShown) {
  base::HistogramTester histogram_tester;
  controller_->OnShown();

  histogram_tester.ExpectTotalCount("DefaultBrowser.SettingsPage.Shown", 1);
}

TEST_F(DefaultBrowserControllerTest, OnIgnored) {
  base::HistogramTester histogram_tester;
  controller_->OnIgnored();

  histogram_tester.ExpectUniqueSample("DefaultBrowser.SettingsPage.Interaction",
                                      DefaultBrowserInteractionType::kIgnored,
                                      1);
}

TEST_F(DefaultBrowserControllerTest, OnDismissed) {
  base::HistogramTester histogram_tester;
  controller_->OnDismissed();

  histogram_tester.ExpectUniqueSample("DefaultBrowser.SettingsPage.Interaction",
                                      DefaultBrowserInteractionType::kDismissed,
                                      1);
}

TEST_F(DefaultBrowserControllerTest, OnAcceptedSuccess) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<DefaultBrowserState> future;
  DefaultBrowserState state = DefaultBrowserState::IS_DEFAULT;

  EXPECT_CALL(*setter_, GetType)
      .WillOnce(testing::Return(DefaultBrowserSetterType::kShellIntegration));
  EXPECT_CALL(*setter_, Execute(testing::_))
      .WillOnce([state](DefaultBrowserSetterCompletionCallback callback) {
        std::move(callback).Run(state);
      });

  controller_->OnAccepted(future.GetCallback());

  ASSERT_EQ(future.Get(), state);
  histogram_tester.ExpectUniqueSample("DefaultBrowser.SettingsPage.Interaction",
                                      DefaultBrowserInteractionType::kAccepted,
                                      1);

  histogram_tester.ExpectUniqueSample("DefaultBrowser.ShellIntegration.Result",
                                      true, 1);
}

TEST_F(DefaultBrowserControllerTest, OnAcceptedFailure) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<DefaultBrowserState> future;
  DefaultBrowserState state = DefaultBrowserState::NOT_DEFAULT;

  EXPECT_CALL(*setter_, GetType)
      .WillOnce(testing::Return(DefaultBrowserSetterType::kShellIntegration));
  EXPECT_CALL(*setter_, Execute(testing::_))
      .WillOnce([state](DefaultBrowserSetterCompletionCallback callback) {
        std::move(callback).Run(state);
      });

  controller_->OnAccepted(future.GetCallback());

  ASSERT_EQ(future.Get(), state);
  histogram_tester.ExpectUniqueSample("DefaultBrowser.SettingsPage.Interaction",
                                      DefaultBrowserInteractionType::kAccepted,
                                      1);

  histogram_tester.ExpectUniqueSample("DefaultBrowser.ShellIntegration.Result",
                                      false, 1);
}

}  // namespace default_browser
