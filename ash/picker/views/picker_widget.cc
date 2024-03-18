// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_widget.h"

#include "ash/bubble/bubble_event_filter.h"
#include "ash/picker/views/picker_view.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr gfx::Size kPickerSize(320, 340);

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
    PickerViewDelegate* delegate,
    const gfx::Rect& anchor_bounds,
    const base::TimeTicks trigger_event_timestamp) {
  const PickerView::PickerLayoutType layout_type = GetLayoutType(anchor_bounds);
  auto picker_view = std::make_unique<PickerView>(delegate, layout_type,
                                                  trigger_event_timestamp);
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
    PickerViewDelegate* delegate,
    const gfx::Rect& anchor_bounds,
    base::TimeTicks trigger_event_timestamp) {
  return base::WrapUnique(
      new PickerWidget(delegate, anchor_bounds, trigger_event_timestamp));
}

void PickerWidget::OnNativeBlur() {
  SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_NONE);
  // TODO: b/322280416 - Add a close reason here for metrics.
  Close();
}

PickerWidget::PickerWidget(PickerViewDelegate* delegate,
                           const gfx::Rect& anchor_bounds,
                           base::TimeTicks trigger_event_timestamp)
    : views::Widget(
          CreateInitParams(delegate, anchor_bounds, trigger_event_timestamp)),
      bubble_event_filter_(
          /*widget=*/this,
          /*button=*/nullptr,
          // base::Unretained is safe here because this class owns
          // `bubble_event_filter_.
          base::BindRepeating(&PickerWidget::OnClickOutsideWidget,
                              base::Unretained(this))) {
  SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_HIDE);
}

void PickerWidget::OnClickOutsideWidget(const ui::LocatedEvent& event) {
  Close();
}

PickerWidget::~PickerWidget() = default;

}  // namespace ash
