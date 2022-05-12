// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include "ash/shell.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace input_overlay {
namespace {

constexpr const char kValidJson[] =
    R"json({
      "tap": [
        {
          "input_sources": [
            "keyboard"
          ],
          "name": "Run",
          "key": "KeyB",
          "location": [
            {
              "type": "position",
              "anchor_to_target": [
                0.8,
                0.8
              ]
            }
          ]
        }
      ]
    })json";
}  // namespace

class DisplayOverlayControllerTest : public exo::test::ExoTestBase {
 public:
  DisplayOverlayControllerTest() = default;

  gfx::Rect GetInputMappingViewBounds() {
    return controller_->GetInputMappingViewBoundsForTesting();
  }

  void DismissEducationalDialog() {
    controller_->DismissEducationalViewForTesting();
  }

 protected:
  std::unique_ptr<input_overlay::test::ArcTestWindow> arc_test_window_;
  std::unique_ptr<DisplayOverlayController> controller_;

 private:
  void SetUp() override {
    exo::test::ExoTestBase::SetUp();
    arc_test_window_ = std::make_unique<input_overlay::test::ArcTestWindow>(
        exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
        "org.chromium.arc.testapp.inputoverlay");
    injector_ = std::make_unique<TouchInjector>(
        arc_test_window_->GetWindow(),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<AppDataProto>, const std::string&) {}));
    base::JSONReader::ValueWithError json_value =
        base::JSONReader::ReadAndReturnValueWithError(kValidJson);
    injector_->ParseActions(json_value.value.value());
    controller_ =
        std::make_unique<DisplayOverlayController>(injector_.get(), false);
  }

  void TearDown() override {
    controller_.reset();
    injector_.reset();
    arc_test_window_.reset();
    exo::test::ExoTestBase::TearDown();
  }

  std::unique_ptr<TouchInjector> injector_;
};

TEST_F(DisplayOverlayControllerTest, TestWindowBoundsChange) {
  // Make sure educational dialog is bypassed.
  DismissEducationalDialog();
  auto original_bounds = GetInputMappingViewBounds();
  auto new_bounds = gfx::Rect(original_bounds);
  new_bounds.set_width(new_bounds.size().width() + 50);
  new_bounds.set_height(new_bounds.size().height() + 50);

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  arc_test_window_->SetBounds(display, new_bounds);
  controller_->OnWindowBoundsChanged();
  auto updated_bounds = GetInputMappingViewBounds();
  EXPECT_NE(original_bounds, updated_bounds);
}

}  // namespace input_overlay
}  // namespace arc
