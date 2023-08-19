// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_feature_pod_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_constants.h"

namespace ash {
namespace {

constexpr gfx::Size kDisplaySize{1024, 768};

}  // namespace

// Tests are parameterized by feature QsRevamp.
class PrivacyScreenFeaturePodControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyScreenFeaturePodControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

  bool IsQsRevampEnabled() const { return GetParam(); }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Setup helpers for overriding the display configuration.
    logger_ = std::make_unique<display::test::ActionLogger>();
    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        base::WrapUnique(native_display_delegate_.get()));
    display_change_observer_ =
        std::make_unique<display::DisplayChangeObserver>(display_manager());
    test_api_ = std::make_unique<display::DisplayConfigurator::TestApi>(
        display_manager()->configurator());
  }

  void TearDown() override {
    display_change_observer_.reset();
    tile_.reset();
    button_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    controller_ = std::make_unique<PrivacyScreenFeaturePodController>();
    if (IsQsRevampEnabled()) {
      tile_ = controller_->CreateTile();
    } else {
      button_ = base::WrapUnique(controller_->CreateButton());
    }
  }

  // Sets up the internal display to support privacy screen.
  void CreateDisplayWithPrivacyScreen() {
    std::vector<display::DisplaySnapshot*> outputs;
    owned_snapshot_ = display::FakeDisplaySnapshot::Builder()
                          .SetId(123u)
                          .SetNativeMode(kDisplaySize)
                          .SetCurrentMode(kDisplaySize)
                          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
                          .SetPrivacyScreen(display::kDisabled)
                          .Build();
    outputs.push_back(owned_snapshot_.get());

    native_display_delegate_->set_outputs(outputs);
    display_manager()->configurator()->OnConfigurationChanged();
    display_manager()->configurator()->ForceInitialConfigure();
    EXPECT_TRUE(test_api_->TriggerConfigureTimeout());
    display_change_observer_->OnDisplayModeChanged(outputs);
  }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }

  bool IsButtonToggled() {
    return IsQsRevampEnabled() ? tile_->IsToggled() : button_->IsToggled();
  }

  void PressIcon() { controller_->OnIconPressed(); }

 private:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<display::test::ActionLogger> logger_;
  raw_ptr<display::test::TestNativeDisplayDelegate,
          DanglingUntriaged | ExperimentalAsh>
      native_display_delegate_ = nullptr;
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<display::DisplayConfigurator::TestApi> test_api_;
  std::unique_ptr<display::DisplaySnapshot> owned_snapshot_;

  std::unique_ptr<PrivacyScreenFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         PrivacyScreenFeaturePodControllerTest,
                         testing::Bool());

TEST_P(PrivacyScreenFeaturePodControllerTest, NormalDisplay) {
  ASSERT_FALSE(Shell::Get()->privacy_screen_controller()->IsSupported());

  // With a display that does not support privacy screen, the button is hidden.
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(PrivacyScreenFeaturePodControllerTest, PrivacyScreenDisplay) {
  CreateDisplayWithPrivacyScreen();
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  ASSERT_TRUE(privacy_screen_controller->IsSupported());

  // With a display that supports privacy screen, the button is visible.
  CreateButton();
  EXPECT_TRUE(IsButtonVisible());

  // Pressing the button enables the privacy screen.
  PressIcon();
  EXPECT_TRUE(privacy_screen_controller->GetEnabled());
  EXPECT_TRUE(IsButtonToggled());

  // Pressing the button again disables the privacy screen.
  PressIcon();
  EXPECT_FALSE(privacy_screen_controller->GetEnabled());
  EXPECT_FALSE(IsButtonToggled());
}

}  // namespace ash
