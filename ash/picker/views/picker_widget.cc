// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_widget.h"

#include "ash/picker/views/picker_view.h"
#include "base/memory/ptr_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr gfx::Size kPickerSize(320, 340);

// Padding to separate the Picker window from the caret.
constexpr gfx::Outsets kPaddingAroundCaret(4);

// Gets the anchor bounds to use for positioning the Picker. We prefer to anchor
// at `caret_bounds`, but may use `cursor_point` as a fallback. `caret_bounds`,
// `cursor_point`, `focused_window_bounds` and returned anchor bounds should be
// in screen coordinates.
gfx::Rect GetPickerAnchorBounds(const gfx::Rect& caret_bounds,
                                const gfx::Point& cursor_point,
                                const gfx::Rect& focused_window_bounds) {
  if (caret_bounds != gfx::Rect() &&
      focused_window_bounds.Contains(caret_bounds)) {
    gfx::Rect anchor_rect = caret_bounds;
    anchor_rect.Outset(kPaddingAroundCaret);
    return anchor_rect;
  } else {
    return gfx::Rect(cursor_point, gfx::Size());
  }
}

// Gets the preferred layout to use given `anchor_bounds` in screen coordinates.
PickerView::PickerLayoutType GetLayoutType(const gfx::Rect& anchor_bounds) {
  return anchor_bounds.bottom() + kPickerSize.height() <=
                 display::Screen::GetScreen()
                     ->GetDisplayMatching(anchor_bounds)
                     .work_area()
                     .bottom()
             ? PickerView::PickerLayoutType::kResultsBelowSearchField
             : PickerView::PickerLayoutType::kResultsAboveSearchField;
}

views::Widget::InitParams CreateInitParams(
    const gfx::Rect& caret_bounds,
    const gfx::Point& cursor_point,
    const gfx::Rect& focused_window_bounds,
    PickerViewDelegate* delegate,
    const base::TimeTicks trigger_event_timestamp) {
  // Create the Picker view and set its size. This will trigger a layout, so
  // that the position of the Picker view's search field can be used when
  // setting the Picker widget bounds below.
  const gfx::Rect anchor_bounds =
      GetPickerAnchorBounds(caret_bounds, cursor_point, focused_window_bounds);
  const PickerView::PickerLayoutType layout_type = GetLayoutType(anchor_bounds);
  auto picker_view = std::make_unique<PickerView>(
      delegate, trigger_event_timestamp, layout_type);
  picker_view->SetSize(kPickerSize);

  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.type = views::Widget::InitParams::TYPE_BUBBLE;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.bounds = picker_view->GetTargetBounds(anchor_bounds, layout_type);
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "Picker";
  params.delegate = picker_view.release();
  return params;
}

}  // namespace

views::UniqueWidgetPtr PickerWidget::Create(
    const gfx::Rect& caret_bounds,
    const gfx::Point& cursor_point,
    const gfx::Rect& focused_window_bounds,
    PickerViewDelegate* delegate,
    base::TimeTicks trigger_event_timestamp) {
  return base::WrapUnique(new PickerWidget(caret_bounds, cursor_point,
                                           focused_window_bounds, delegate,
                                           trigger_event_timestamp));
}

PickerWidget::PickerWidget(const gfx::Rect& caret_bounds,
                           const gfx::Point& cursor_point,
                           const gfx::Rect& focused_window_bounds,
                           PickerViewDelegate* delegate,
                           base::TimeTicks trigger_event_timestamp)
    : views::Widget(CreateInitParams(caret_bounds,
                                     cursor_point,
                                     focused_window_bounds,
                                     delegate,
                                     trigger_event_timestamp)) {
  SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_HIDE);
}

PickerWidget::~PickerWidget() = default;

}  // namespace ash
