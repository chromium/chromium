// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_desks_api.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

class TestDesksActivationObserver : public DesksController::Observer {
 public:
  TestDesksActivationObserver() { DesksController::Get()->AddObserver(this); }
  TestDesksActivationObserver(const TestDesksActivationObserver&) = delete;
  TestDesksActivationObserver& operator=(const TestDesksActivationObserver&) =
      delete;
  ~TestDesksActivationObserver() override {
    DesksController::Get()->RemoveObserver(this);
  }

  int activation_changes() const { return activation_changes_; }
  void set_activation_changes(int val) { activation_changes_ = val; }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {}
  void OnDeskRemoved(const Desk* desk) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    ++activation_changes_;
  }
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {}

 private:
  int activation_changes_ = 0;
};

}  // namespace

using AutotestDesksApiTest = AshTestBase;

TEST_F(AutotestDesksApiTest, CreateNewDesk) {
  AutotestDesksApi test_api;
  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());
  EXPECT_FALSE(test_api.CreateNewDesk());
}

TEST_F(AutotestDesksApiTest, ActivateDeskAtIndex) {
  AutotestDesksApi test_api;
  EXPECT_FALSE(test_api.ActivateDeskAtIndex(-1, base::DoNothing()));
  EXPECT_FALSE(test_api.ActivateDeskAtIndex(4, base::DoNothing()));

  // Activating already active desk does nothing.
  EXPECT_FALSE(test_api.ActivateDeskAtIndex(0, base::DoNothing()));

  // Create 4 desks and switch between all of them.
  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());
  EXPECT_EQ(4u, controller->desks().size());
  constexpr int kIndices[] = {1, 2, 3, 0};
  for (const int index : kIndices) {
    base::RunLoop run_loop;
    EXPECT_TRUE(test_api.ActivateDeskAtIndex(index, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(controller->active_desk(), controller->desks()[index].get());
  }
}

TEST_F(AutotestDesksApiTest, RemoveActiveDesk) {
  AutotestDesksApi test_api;
  EXPECT_FALSE(test_api.RemoveActiveDesk(base::DoNothing()));

  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());
  EXPECT_EQ(4u, controller->desks().size());
  EXPECT_EQ(controller->active_desk(), controller->desks()[0].get());

  // List of desks that will be activated after each time we invoke
  // RemoveActiveDesk().
  const Desk* desks_after_removal[] = {
      controller->desks()[1].get(),
      controller->desks()[2].get(),
      controller->desks()[3].get(),
  };

  for (const Desk* desk : desks_after_removal) {
    base::RunLoop run_loop;
    EXPECT_TRUE(test_api.RemoveActiveDesk(run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(controller->active_desk(), desk);
  }

  // Can no longer remove desks.
  EXPECT_FALSE(test_api.RemoveActiveDesk(base::DoNothing()));
}

class EnhancedDeskAnimationsAutotestDesksApiTest : public AutotestDesksApiTest {
 public:
  EnhancedDeskAnimationsAutotestDesksApiTest() = default;
  EnhancedDeskAnimationsAutotestDesksApiTest(
      const EnhancedDeskAnimationsAutotestDesksApiTest&) = delete;
  EnhancedDeskAnimationsAutotestDesksApiTest& operator=(
      const EnhancedDeskAnimationsAutotestDesksApiTest&) = delete;
  ~EnhancedDeskAnimationsAutotestDesksApiTest() override = default;

  // AutotestDesksApiTest:
  void SetUp() override {
    features_.InitAndEnableFeature(features::kEnhancedDeskAnimations);

    AutotestDesksApiTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(EnhancedDeskAnimationsAutotestDesksApiTest,
       ActivateAdjacentDesksToTargetIndex) {
  // Create all desks possible.
  AutotestDesksApi test_api;
  auto* controller = DesksController::Get();
  while (controller->CanCreateDesks())
    EXPECT_TRUE(test_api.CreateNewDesk());

  EXPECT_FALSE(
      test_api.ActivateAdjacentDesksToTargetIndex(-1, base::DoNothing()));
  EXPECT_FALSE(
      test_api.ActivateAdjacentDesksToTargetIndex(4, base::DoNothing()));

  // Activating already active desk does nothing.
  EXPECT_FALSE(
      test_api.ActivateAdjacentDesksToTargetIndex(0, base::DoNothing()));
  EXPECT_EQ(4u, controller->desks().size());

  // Replacing needs to be done while a current animation is underway, otherwise
  // it will have no effect.
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Activate the rightmost desk. Test that we end on that desk and that we
  // observed 3 activation changes.
  TestDesksActivationObserver desk_activation_observer;
  {
    base::RunLoop run_loop;
    EXPECT_TRUE(
        test_api.ActivateAdjacentDesksToTargetIndex(3, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(controller->active_desk(), controller->desks().back().get());
  EXPECT_EQ(3, desk_activation_observer.activation_changes());

  // Activate the leftmost desk. Test that we end on that desk and that we
  // observed 3 activation changes.
  desk_activation_observer.set_activation_changes(0);
  {
    base::RunLoop run_loop;
    EXPECT_TRUE(
        test_api.ActivateAdjacentDesksToTargetIndex(0, run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(controller->active_desk(), controller->desks().front().get());
  EXPECT_EQ(3, desk_activation_observer.activation_changes());
}

}  // namespace ash
