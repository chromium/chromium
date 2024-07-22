// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/compat_mode_button.h"
#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/window/caption_button_types.h"

namespace arc {

namespace {

class FakeCompatModeButtonController : public CompatModeButtonController {
 public:
  FakeCompatModeButtonController() {}
  ~FakeCompatModeButtonController() override = default;

  // CompatModeButton:
  void OnButtonPressed() override { fake_visible_when_button_pressed_ = true; }

  bool fake_visible_when_button_pressed_{false};

  bool visible_when_button_pressed() const {
    return fake_visible_when_button_pressed_;
  }
};

}  // namespace

class CompatModeButtonTest : public views::ViewsTestBase {
 public:
  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    controller_ = std::make_unique<FakeCompatModeButtonController>();
    button_ = std::make_unique<arc::CompatModeButton>(
        controller_.get(), views::Button::PressedCallback());
  }
  void TearDown() override {
    button_.reset();
    controller_.reset();
    ViewsTestBase::TearDown();
  }

  FakeCompatModeButtonController* compat_mode_button_controller() {
    return controller_.get();
  }
  CompatModeButton* compat_mode_button() { return button_.get(); }

  ui::MouseEvent GenerateMouseEvent(ui::EventType event_type) {
    const gfx::PointF location(0, 0);
    return ui::MouseEvent(event_type, location, location,
                          base::TimeTicks::Now(), 0, 0);
  }

  ui::GestureEvent GenerateGestureEvent(ui::EventType event_type) {
    return ui::GestureEvent(0, 0, 0, base::TimeTicks::Now(),
                            ui::GestureEventDetails(event_type));
  }

 private:
  std::unique_ptr<FakeCompatModeButtonController> controller_;
  std::unique_ptr<arc::CompatModeButton> button_;
};

TEST_F(CompatModeButtonTest, ConstructDestruct) {}

TEST_F(CompatModeButtonTest, PressWithMouseEvent) {
  auto event = GenerateMouseEvent(ui::EventType::kMousePressed);
  compat_mode_button()->OnMousePressed(event);
  EXPECT_TRUE(compat_mode_button_controller()->visible_when_button_pressed());
}

TEST_F(CompatModeButtonTest, PressWithGestureEvent) {
  auto double_tap_event =
      GenerateGestureEvent(ui::EventType::kGestureDoubleTap);
  compat_mode_button()->OnGestureEvent(&double_tap_event);
  EXPECT_FALSE(compat_mode_button_controller()->visible_when_button_pressed());

  auto tap_down_event = GenerateGestureEvent(ui::EventType::kGestureTapDown);
  compat_mode_button()->OnGestureEvent(&tap_down_event);
  EXPECT_TRUE(compat_mode_button_controller()->visible_when_button_pressed());

  auto tap_event = GenerateGestureEvent(ui::EventType::kGestureTap);
  compat_mode_button()->OnGestureEvent(&tap_event);
  EXPECT_TRUE(compat_mode_button_controller()->visible_when_button_pressed());
}

}  // namespace arc
