// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/touch_selection/touch_selection_magnifier_aura.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

gfx::Rect GetCursorBoundsInScreen(views::Textfield* textfield, int cursor_pos) {
  gfx::Rect cursor_bounds =
      views::TextfieldTestApi(textfield).GetRenderText()->GetCursorBounds(
          gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD),
          /*insert_mode=*/true);
  views::View::ConvertRectToScreen(textfield, &cursor_bounds);
  return cursor_bounds;
}

class TouchSelectionPixelTest : public AshTestBase {
 public:
  TouchSelectionPixelTest() = default;
  TouchSelectionPixelTest(const TouchSelectionPixelTest&) = delete;
  TouchSelectionPixelTest& operator=(const TouchSelectionPixelTest&) = delete;
  ~TouchSelectionPixelTest() override = default;

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  std::unique_ptr<views::Widget> CreateContainerWidget() {
    std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
    widget->SetBounds(gfx::Rect(5, 5, 500, 500));
    widget->SetContentsView(std::make_unique<views::View>());
    widget->Show();
    return widget;
  }

 private:
  // Disable animations so that touch selection UI appears immediately when
  // triggered.
  ui::ScopedAnimationDurationScaleMode disable_animations_{
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

TEST_F(TouchSelectionPixelTest, MagnifierOnTextfield) {
  auto widget = CreateContainerWidget();
  auto* textfield = widget->GetContentsView()->AddChildView(
      std::make_unique<views::Textfield>());
  textfield->GetViewAccessibility().SetName(u"Textfield");
  textfield->SetBoundsRect(gfx::Rect(100, 100, 200, 30));
  textfield->SetText(u"Text in a textfield");
  textfield->SetSelectedRange({0, 9});
  textfield->RequestFocus();

  ui::TouchSelectionMagnifierAura magnifier;
  const gfx::Rect cursor_bounds =
      GetCursorBoundsInScreen(textfield, /*cursor_pos=*/9);
  magnifier.ShowFocusBound(textfield->GetNativeView()->GetRootWindow()->layer(),
                           cursor_bounds.top_center(),
                           cursor_bounds.bottom_center());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "touch_selection",
      /*revision_number=*/2, widget.get()));
}

}  // namespace

}  // namespace ash
