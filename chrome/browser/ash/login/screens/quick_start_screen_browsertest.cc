// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
constexpr char kWelcomeScreen[] = "welcomeScreen";
constexpr char kQuickStartButton[] = "quickStart";
constexpr test::UIPath kQuickStartButtonPath = {
    chromeos::WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartButton};
}  // namespace

class QuickStartBrowserTest : public OobeBaseTest {
 public:
  QuickStartBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }
  ~QuickStartBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        nullptr);
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, ButtonVisibleOnWelcomeScreen) {
  OobeScreenWaiter(chromeos::WelcomeView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kQuickStartButtonPath);

  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();
}

}  // namespace ash
