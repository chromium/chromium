// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/unlock_throughput_recorder.h"

#include <memory>
#include <vector>

#include "ash/login/ui/login_test_base.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/animation_metrics_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/login/login_state/login_state.h"

namespace ash {
namespace {
constexpr char kAshUnlockAnimationSmoothnessTabletMode[] =
    "Ash.UnlockAnimation.Smoothness.TabletMode";
constexpr char kAshUnlockAnimationSmoothnessClamshellMode[] =
    "Ash.UnlockAnimation.Smoothness.ClamshellMode";
}  // namespace

class UnlockThroughputRecorderTest : public LoginTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  UnlockThroughputRecorderTest() = default;
  UnlockThroughputRecorderTest(const UnlockThroughputRecorderTest&) = delete;
  UnlockThroughputRecorderTest& operator=(const UnlockThroughputRecorderTest&) =
      delete;
  ~UnlockThroughputRecorderTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  void LoginOwner() {
    CreateUserSessions(1);
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
  }

  void EnableTabletMode(bool enable) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);
  }

  void LockScreenAndAnimate() {
    GetSessionControllerClient()->LockScreen();
    test::RunSimpleAnimation();
  }

  void UnlockScreenAndAnimate() {
    GetSessionControllerClient()->UnlockScreen();
    test::RunSimpleAnimation();
  }

  // Used to verify recorded data.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Boolean parameter controls tablet mode.
INSTANTIATE_TEST_SUITE_P(All, UnlockThroughputRecorderTest, testing::Bool());

TEST_P(UnlockThroughputRecorderTest, ReportUnlock) {
  LoginOwner();

  EnableTabletMode(GetParam());

  LockScreenAndAnimate();
  UnlockScreenAndAnimate();

  test::MetricsWaiter(histogram_tester_.get(),
                      GetParam() ? kAshUnlockAnimationSmoothnessTabletMode
                                 : kAshUnlockAnimationSmoothnessClamshellMode)
      .Wait();
}

}  // namespace ash
