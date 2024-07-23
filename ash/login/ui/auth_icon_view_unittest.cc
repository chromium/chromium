// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_icon_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/test/views_drawing_test_utils.h"

namespace ash {

// Most methods of AuthIconView are not tested because they do not lend
// themselves to be easily unit tested. The methods do not have easily
// observable/verifiable results. Most of the methods are paints/draws that call
// methods in a ui gfx/compositor framework without easily mockable interfaces.
class AuthIconViewTest : public AshTestBase {
 public:
  AuthIconViewTest() = default;
  AuthIconViewTest(const AuthIconView&) = delete;
  AuthIconViewTest& operator=(const AuthIconViewTest&) = delete;
  ~AuthIconViewTest() override = default;

  void OnTapOrClickCallback() { on_tap_or_click_callback_called_ = true; }

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    auth_icon_view_ = std::make_unique<AuthIconView>();
  }

  std::unique_ptr<AuthIconView> auth_icon_view_;
  bool on_tap_or_click_callback_called_ = false;
};

TEST_F(AuthIconViewTest, OnGestureEvent) {
  auth_icon_view_->set_on_tap_or_click_callback(base::BindRepeating(
      &AuthIconViewTest::OnTapOrClickCallback, base::Unretained(this)));
  EXPECT_FALSE(on_tap_or_click_callback_called_);
  ui::GestureEvent scroll_begin(
      /*x=*/0, /*y=*/0, /*flags=*/0, /*time_stamp=*/base::TimeTicks(),
      /*details=*/
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, 1));
  auth_icon_view_->OnGestureEvent(&scroll_begin);
  EXPECT_FALSE(on_tap_or_click_callback_called_);
  ui::GestureEvent tap(
      /*x=*/0, /*y=*/0, /*flags=*/0, /*time_stamp=*/base::TimeTicks(),
      /*details=*/ui::GestureEventDetails(ui::EventType::kGestureTap));
  auth_icon_view_->OnGestureEvent(&tap);
  EXPECT_TRUE(on_tap_or_click_callback_called_);
}

TEST_F(AuthIconViewTest, OnMousePressed) {
  ui::MouseEvent mouse(ui::EventType::kMousePressed, gfx::PointF(),
                       gfx::PointF(), base::TimeTicks(), 0, 0);
  EXPECT_FALSE(auth_icon_view_->OnMousePressed(mouse));
  auth_icon_view_->set_on_tap_or_click_callback(base::BindRepeating(
      &AuthIconViewTest::OnTapOrClickCallback, base::Unretained(this)));
  EXPECT_TRUE(auth_icon_view_->OnMousePressed(mouse));
  EXPECT_TRUE(on_tap_or_click_callback_called_);
}

}  // namespace ash
