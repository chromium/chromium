// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, ButtonVisibleOnWelcomeScreen) {
  OobeScreenWaiter(chromeos::WelcomeView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kQuickStartButtonPath);
}

}  // namespace ash
