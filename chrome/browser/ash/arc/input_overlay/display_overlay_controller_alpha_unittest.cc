// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/lottie/resource.h"

namespace arc::input_overlay {

// TODO(b/253646354): Test can be removed when removing the pre-beta version.
class DisplayOverlayControllerAlphaTest : public exo::test::ExoTestBase {
 public:
  DisplayOverlayControllerAlphaTest() = default;

  gfx::Rect GetInputMappingViewBounds() {
    return controller_->GetInputMappingViewBoundsForTesting();
  }

  void DismissEducationalDialog() {
    controller_->DismissEducationalViewForTesting();
  }

  bool ShowingNudge() { return !!controller_->nudge_view_; }

 protected:
  std::unique_ptr<test::ArcTestWindow> arc_test_window_;
  std::unique_ptr<DisplayOverlayController> controller_;
  std::unique_ptr<TouchInjector> injector_;

 private:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(ash::features::kGameDashboard);
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);

    exo::test::ExoTestBase::SetUp();
    arc_test_window_ = std::make_unique<test::ArcTestWindow>(
        exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
        "org.chromium.arc.testapp.inputoverlay");
    injector_ = std::make_unique<TouchInjector>(
        arc_test_window_->GetWindow(),
        *arc_test_window_->GetWindow()->GetProperty(ash::kArcPackageNameKey),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<AppDataProto>, std::string) {}));
    controller_ = std::make_unique<DisplayOverlayController>(
        injector_.get(), /*first_launch=*/true);
  }

  void TearDown() override {
    controller_.reset();
    injector_.reset();
    arc_test_window_.reset();
    exo::test::ExoTestBase::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DisplayOverlayControllerAlphaTest, TestWindowBoundsChange) {
  // Make sure educational dialog is bypassed.
  DismissEducationalDialog();
  auto original_bounds = GetInputMappingViewBounds();
  auto new_bounds = gfx::Rect(original_bounds);
  new_bounds.set_width(new_bounds.size().width() + 50);
  new_bounds.set_height(new_bounds.size().height() + 50);

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  arc_test_window_->SetBounds(display, new_bounds);
  auto updated_bounds = GetInputMappingViewBounds();
  EXPECT_NE(original_bounds, updated_bounds);
  EXPECT_EQ(updated_bounds, new_bounds);
}

TEST_F(DisplayOverlayControllerAlphaTest, TestEducationNudgeDismissesOnClick) {
  injector_->NotifyFirstTimeLaunch();
  DismissEducationalDialog();
  EXPECT_TRUE(ShowingNudge());
  auto center = injector_->window()->bounds().CenterPoint();
  auto mouse_pressed = ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  controller_->OnMouseEvent(&mouse_pressed);
  EXPECT_FALSE(ShowingNudge());
}

}  // namespace arc::input_overlay
