// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/screens/quick_start_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
constexpr char kWelcomeScreen[] = "welcomeScreen";
constexpr char kQuickStartButton[] = "quickStart";
constexpr test::UIPath kQuickStartButtonPath = {
    WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartButton};
}  // namespace

class QuickStartBrowserTest : public OobeBaseTest {
 public:
  QuickStartBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }
  ~QuickStartBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
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

class QuickStartNotDeterminedBrowserTest : public QuickStartBrowserTest {
 public:
  QuickStartNotDeterminedBrowserTest() {
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
  }
};

IN_PROC_BROWSER_TEST_F(QuickStartNotDeterminedBrowserTest,
                       ButtonVisibleOnWelcomeScreen) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath(kQuickStartButtonPath);

  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, QRCode) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kQuickStartButtonPath);

  test::OobeJS().ClickOnPath(kQuickStartButtonPath);

  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  connection_broker_factory_.instances().front()->InitiateConnection(
      "fake_device_id");

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath({QuickStartView::kScreenId.name}) +
                    ".uiStep === 'verification'")
      ->Wait();
  test::OobeJS().ExpectAttributeEQ("canvasSize_",
                                   {QuickStartView::kScreenId.name}, 225);
}

}  // namespace ash
