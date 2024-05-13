// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_feature_pod_controller.h"

#include <memory>

#include "ash/shell.h"
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

class PrivacyScreenFeaturePodControllerTest : public AshTestBase {
 public:
  PrivacyScreenFeaturePodControllerTest() = default;

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
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateButton() {
    controller_ = std::make_unique<PrivacyScreenFeaturePodController>();
    tile_ = controller_->CreateTile();
  }

  // Sets up the internal display to support privacy screen.
  void CreateDisplayWithPrivacyScreen() {
    std::vector<std::unique_ptr<display::DisplaySnapshot>> outputs;
    outputs.push_back(display::FakeDisplaySnapshot::Builder()
                          .SetId(123u)
                          .SetNativeMode(kDisplaySize)
                          .SetCurrentMode(kDisplaySize)
                          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
                          .SetPrivacyScreen(display::kDisabled)
                          .Build());

    native_display_delegate_->SetOutputs(std::move(outputs));
    display_manager()->configurator()->OnConfigurationChanged();
    display_manager()->configurator()->ForceInitialConfigure();
    EXPECT_TRUE(test_api_->TriggerConfigureTimeout());
    display_change_observer_->OnDisplayConfigurationChanged(
        native_display_delegate_->GetOutputs());
  }

  bool IsButtonVisible() { return tile_->GetVisible(); }

  bool IsButtonToggled() { return tile_->IsToggled(); }

  void PressIcon() { controller_->OnIconPressed(); }

 private:

  std::unique_ptr<display::test::ActionLogger> logger_;
  raw_ptr<display::test::TestNativeDisplayDelegate, DanglingUntriaged>
      native_display_delegate_ = nullptr;
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<display::DisplayConfigurator::TestApi> test_api_;

  std::unique_ptr<PrivacyScreenFeaturePodController> controller_;
  std::unique_ptr<FeatureTile> tile_;
};

TEST_F(PrivacyScreenFeaturePodControllerTest, NormalDisplay) {
  ASSERT_FALSE(Shell::Get()->privacy_screen_controller()->IsSupported());

  // With a display that does not support privacy screen, the button is hidden.
  CreateButton();
  EXPECT_FALSE(IsButtonVisible());
}

TEST_F(PrivacyScreenFeaturePodControllerTest, PrivacyScreenDisplay) {
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
