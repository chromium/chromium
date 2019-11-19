// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble_view.h"

#include <algorithm>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/scroll_arrow_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_container_view.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/window_util.h"
#include "base/i18n/rtl.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Padding between the end of the shelf in overflow mode and the arrow button
// (if any).
int GetDistanceToArrowButton() {
  return ShelfConfig::Get()->button_spacing();
}

// Distance between overflow bubble and the main shelf.
constexpr int kDistanceToMainShelf = 4;

// Sum of the shelf button size and the gap between shelf buttons.
int GetUnit() {
  return ShelfConfig::Get()->button_size() +
         ShelfConfig::Get()->button_spacing();
}

// Decides whether the current first visible shelf icon of the overflow shelf
// should be hidden or fully shown when gesture scroll ends.
int GetGestureDragTheshold() {
  return ShelfConfig::Get()->button_size() / 2;
}

int GetBubbleCornerRadius() {
  return ShelfConfig::Get()->button_size() / 2;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// OverflowScrollArrowView impl

class OverflowBubbleView::OverflowScrollArrowView : public ScrollArrowView {
 public:
  OverflowScrollArrowView(ArrowType arrow_type,
                          bool is_horizontal_alignment,
                          Shelf* shelf,
                          ShelfButtonDelegate* button_delegate)
      : ScrollArrowView(arrow_type,
                        is_horizontal_alignment,
                        shelf,
                        button_delegate) {}
  ~OverflowScrollArrowView() override = default;

  // views::View:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    ScrollArrowView::PaintButtonContents(canvas);

    float ring_radius_dp = width() / 2;
    gfx::PointF circle_center(width() / 2.f, height() / 2.f);

    {
      gfx::ScopedCanvas scoped_canvas(canvas);
      const float dsf = canvas->UndoDeviceScaleFactor();
      cc::PaintFlags fg_flags;
      fg_flags.setAntiAlias(true);
      fg_flags.setColor(
          ShelfConfig::Get()->shelf_control_permanent_highlight_background());

      const float radius = std::ceil(ring_radius_dp * dsf);
      canvas->DrawCircle(gfx::ScalePoint(circle_center, dsf), radius, fg_flags);
    }
  }

  const char* GetClassName() const override {
    return "OverflowScrollArrowView";
  }
};

////////////////////////////////////////////////////////////////////////////////
// OverflowShelfContainerView impl

class OverflowBubbleView::OverflowShelfContainerView
    : public ShelfContainerView {
 public:
  OverflowShelfContainerView(ShelfView* shelf_view,
                             OverflowBubbleView* bubble_view);
  ~OverflowShelfContainerView() override = default;

  // Update the shelf icons' opacity.
  void UpdateShelfIconsOpacity();

  // views::View:
  void Layout() override;
  const char* GetClassName() const override;

  int first_visible_index() const { return first_visible_index_; }
  int last_visible_index() const { return last_visible_index_; }

 private:
  // Update the opacity of shelf icons on both edges based on the drag offset.
  void UpdateOpacityOfEdgeIcons(int offset_distance);

  // Set all of shelf icons within the bounds of the shelf container view to be
  // fully opaque.
  void ShowShelfIconsWithinBounds();

  // Parent view. Not owned.
  OverflowBubbleView* bubble_view_;

  // The index of the leftmost/rightmost shelf icon within the bounds of
  // |shelf_container_view_|. Different from the |first_visible_index_| or
  // |last_visible_index_| in ShelfView which specifies the range of shelf icons
  // belonging to the shelf view, the two attributes in OverflowBubbleView
  // indicate the shelf icons not hidden by the parent view.
  int first_visible_index_ = -1;
  int last_visible_index_ = -1;

  DISALLOW_COPY_AND_ASSIGN(OverflowShelfContainerView);
};

OverflowBubbleView::OverflowShelfContainerView::OverflowShelfContainerView(
    ShelfView* shelf_view,
    OverflowBubbleView* bubble_view)
    : ShelfContainerView(shelf_view), bubble_view_(bubble_view) {}

void OverflowBubbleView::OverflowShelfContainerView::Layout() {
  shelf_view_->SetBoundsRect(gfx::Rect(CalculateIdealSize()));
}

const char* OverflowBubbleView::OverflowShelfContainerView::GetClassName()
    const {
  return "OverflowShelfContainerView";
}

void OverflowBubbleView::OverflowShelfContainerView::UpdateShelfIconsOpacity() {
  gfx::Vector2dF scroll_offset = bubble_view_->scroll_offset();
  LayoutStrategy layout_strategy = bubble_view_->layout_strategy();

  int updated_first_visible_index = shelf_view_->first_visible_index();
  if (layout_strategy == NOT_SHOW_ARROW_BUTTONS) {
    first_visible_index_ = updated_first_visible_index;
    last_visible_index_ = shelf_view_->last_visible_index();
    return;
  }

  const bool is_horizontal_aligned =
      shelf_view_->shelf()->IsHorizontalAlignment();

  const int scroll_distance =
      is_horizontal_aligned ? scroll_offset.x() : scroll_offset.y();
  updated_first_visible_index += scroll_distance / GetUnit();
  if (layout_strategy == SHOW_LEFT_ARROW_BUTTON ||
      layout_strategy == SHOW_BUTTONS) {
    updated_first_visible_index++;
  }

  const int offset = (is_horizontal_aligned ? bubble_view_->bounds().width()
                                            : bubble_view_->bounds().height()) -
                     2 * GetUnit();
  int updated_last_visible_index =
      updated_first_visible_index + offset / GetUnit();
  if (layout_strategy == SHOW_BUTTONS)
    updated_last_visible_index--;

  if (updated_first_visible_index != first_visible_index_ ||
      updated_last_visible_index != last_visible_index_) {
    first_visible_index_ = updated_first_visible_index;
    last_visible_index_ = updated_last_visible_index;
    ShowShelfIconsWithinBounds();
  }

  UpdateOpacityOfEdgeIcons(scroll_distance);
}

void OverflowBubbleView::OverflowShelfContainerView::UpdateOpacityOfEdgeIcons(
    int offset_distance) {
  const int remainder = offset_distance % GetUnit();
  const int complement = GetUnit() - remainder;

  views::ViewModel* shelf_view_model = shelf_view_->view_model();

  // Calculate the opacity of the leftmost visible shelf icon.
  views::View* leftmost_view = shelf_view_model->view_at(first_visible_index_);
  leftmost_view->layer()->SetOpacity(
      remainder >= kFadingZone ? 0 : (1.0f - remainder / (float)kFadingZone));

  // Instead of the shelf icon denoted by |last_visible_index_|, we should
  // update the opacity of the icon right after if any. Because
  // |last_visible_index_| is calculated with flooring.
  if (last_visible_index_ + 1 < shelf_view_model->view_size()) {
    views::View* rightmost_view =
        shelf_view_model->view_at(last_visible_index_ + 1);
    rightmost_view->layer()->SetOpacity(complement >= kFadingZone
                                            ? 0.f
                                            : (kFadingZone - complement) /
                                                  (float)(kFadingZone));
  }
}

void OverflowBubbleView::OverflowShelfContainerView::
    ShowShelfIconsWithinBounds() {
  for (int i = first_visible_index_; i <= last_visible_index_; i++) {
    views::View* shelf_icon = shelf_view_->view_model()->view_at(i);
    shelf_icon->layer()->SetOpacity(1);
  }
}

////////////////////////////////////////////////////////////////////////////////
// OverflowBubbleView

OverflowBubbleView::OverflowBubbleView(ShelfView* shelf_view,
                                       views::View* anchor,
                                       SkColor background_color)
    : ShelfBubble(anchor, shelf_view->shelf()->alignment(), background_color),
      shelf_view_(shelf_view) {
  DCHECK(shelf_view_);
  DCHECK(GetShelf());

  const int shelf_size = ShelfConfig::Get()->shelf_size();
  set_border_radius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, {shelf_size, shelf_size}));
  SetArrow(views::BubbleBorder::NONE);
  SetBackground(nullptr);
  set_shadow(views::BubbleBorder::NO_ASSETS);
  set_close_on_deactivate(false);
  set_accept_events(true);
  set_margins(gfx::Insets(0, 0));

  // Makes bubble view has a layer and clip its children layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(GetBubbleCornerRadius()));

  // Initialize the left arrow button.
  left_arrow_ = AddChildView(std::make_unique<OverflowScrollArrowView>(
      ScrollArrowView::kLeft, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  // Initialize the right arrow button.
  right_arrow_ = AddChildView(std::make_unique<OverflowScrollArrowView>(
      ScrollArrowView::kRight, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  // Initialize the shelf container view.
  shelf_container_view_ = AddChildView(
      std::make_unique<OverflowShelfContainerView>(shelf_view, this));
  shelf_container_view_->Initialize();

  CreateBubble();
}

OverflowBubbleView::~OverflowBubbleView() = default;

bool OverflowBubbleView::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Handle scroll-related events, but don't do anything special for begin and
  // end.
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    return true;
  }

  // Make sure that no visible shelf button is partially shown after gestures.
  if (event.type() == ui::ET_GESTURE_END ||
      event.type() == ui::ET_GESTURE_SCROLL_END) {
    int current_scroll_distance = GetShelf()->IsHorizontalAlignment()
                                      ? scroll_offset_.x()
                                      : scroll_offset_.y();
    const int residue = current_scroll_distance % GetUnit();

    // if it does not need to adjust the location of the shelf view,
    // return early.
    if (current_scroll_distance == CalculateScrollUpperBound() || residue == 0)
      return true;

    int offset =
        residue > GetGestureDragTheshold() ? GetUnit() - residue : -residue;
    if (GetShelf()->IsHorizontalAlignment())
      ScrollByXOffset(offset, /*animate=*/true);
    else
      ScrollByYOffset(offset, /*animate=*/true);
    return true;
  }

  if (event.type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return false;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(-event.details().scroll_x(), /*animate=*/false);
  else
    ScrollByYOffset(-event.details().scroll_y(), /*animate=*/false);
  return true;
}

int OverflowBubbleView::ScrollByXOffset(float x_offset, bool animating) {
  const float old_x = scroll_offset_.x();
  const float x = CalculateLayoutStrategyAfterScroll(x_offset);
  scroll_offset_.set_x(x);
  Layout();
  const float diff = x - old_x;
  if (animating)
    StartShelfScrollAnimation(diff);
  return diff;
}

int OverflowBubbleView::ScrollByYOffset(float y_offset, bool animating) {
  const int old_y = scroll_offset_.y();
  const int y = CalculateLayoutStrategyAfterScroll(y_offset);
  scroll_offset_.set_y(y);
  Layout();
  const float diff = y - old_y;
  if (animating)
    StartShelfScrollAnimation(diff);
  return diff;
}

int OverflowBubbleView::GetFirstVisibleIndex() const {
  return shelf_container_view_->first_visible_index();
}

int OverflowBubbleView::GetLastVisibleIndex() const {
  return shelf_container_view_->last_visible_index();
}

int OverflowBubbleView::CalculateScrollUpperBound() const {
  const bool is_horizontal = GetShelf()->IsHorizontalAlignment();

  // Calculate the length of the available space.
  const gfx::Rect content_size = GetContentsBounds();
  const int available_length =
      (is_horizontal ? content_size.width() : content_size.height()) -
      2 * kEndPadding;

  // Calculate the length of the preferred size.
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length = (is_horizontal ? shelf_preferred_size.width()
                                              : shelf_preferred_size.height());

  DCHECK_GE(preferred_length, available_length);
  return preferred_length - available_length;
}

float OverflowBubbleView::CalculateLayoutStrategyAfterScroll(float scroll) {
  const float old_scroll = GetShelf()->IsHorizontalAlignment()
                               ? scroll_offset_.x()
                               : scroll_offset_.y();

  const float scroll_upper_bound =
      static_cast<float>(CalculateScrollUpperBound());
  scroll = std::min(scroll_upper_bound, std::max(0.f, old_scroll + scroll));
  if (layout_strategy_ != NOT_SHOW_ARROW_BUTTONS) {
    if (scroll <= 0.f)
      layout_strategy_ = SHOW_RIGHT_ARROW_BUTTON;
    else if (scroll >= scroll_upper_bound)
      layout_strategy_ = SHOW_LEFT_ARROW_BUTTON;
    else
      layout_strategy_ = SHOW_BUTTONS;
  }
  return scroll;
}

void OverflowBubbleView::AdjustToEnsureIconsFullyVisible(
    gfx::Rect* bubble_bounds) const {
  if (layout_strategy_ == NOT_SHOW_ARROW_BUTTONS)
    return;

  int width = GetShelf()->IsHorizontalAlignment() ? bubble_bounds->width()
                                                  : bubble_bounds->height();
  const int rd = width % GetUnit();
  width -= rd;

  // Offset to ensure that the bubble view is shown at the center of the screen.
  if (GetShelf()->IsHorizontalAlignment()) {
    bubble_bounds->set_width(width);
    bubble_bounds->Offset(rd / 2, 0);
  } else {
    bubble_bounds->set_height(width);
    bubble_bounds->Offset(0, rd / 2);
  }
}

void OverflowBubbleView::StartShelfScrollAnimation(float scroll_distance) {
  const gfx::Transform current_transform = shelf_view()->GetTransform();
  gfx::Transform reverse_transform = current_transform;
  if (GetShelf()->IsHorizontalAlignment())
    reverse_transform.Translate(gfx::Vector2dF(scroll_distance, 0));
  else
    reverse_transform.Translate(gfx::Vector2dF(0, scroll_distance));
  shelf_view()->layer()->SetTransform(reverse_transform);
  ui::ScopedLayerAnimationSettings animation_settings(
      shelf_view()->layer()->GetAnimator());
  animation_settings.SetTweenType(gfx::Tween::EASE_OUT);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  shelf_view()->layer()->SetTransform(current_transform);
}

void OverflowBubbleView::UpdateLayoutStrategy() {
  const int available_length =
      GetShelf()->IsHorizontalAlignment() ? width() : height();
  gfx::Size preferred_size = shelf_container_view_->GetPreferredSize();
  int preferred_length = GetShelf()->IsHorizontalAlignment()
                             ? preferred_size.width()
                             : preferred_size.height();
  preferred_length += 2 * kEndPadding;
  const int scroll_length = GetShelf()->IsHorizontalAlignment()
                                ? scroll_offset_.x()
                                : scroll_offset_.y();

  if (preferred_length <= available_length) {
    // Enough space to accommodate all of shelf buttons. So hide arrow buttons.
    layout_strategy_ = NOT_SHOW_ARROW_BUTTONS;
  } else if (scroll_length == 0) {
    // No invisible shelf buttons at the left side. So hide the left button.
    layout_strategy_ = SHOW_RIGHT_ARROW_BUTTON;
  } else if (scroll_length == CalculateScrollUpperBound()) {
    // If there is no invisible shelf button at the right side, hide the right
    // button.
    layout_strategy_ = SHOW_LEFT_ARROW_BUTTON;
  } else {
    // There are invisible shelf buttons at both sides. So show two buttons.
    layout_strategy_ = SHOW_BUTTONS;
  }
}

void OverflowBubbleView::ScrollToNewPage(bool forward) {
  // Implement the arrow button handler in the same way with scrolling the
  // bubble view. The key is to calculate the suitable scroll distance.
  int offset = (GetShelf()->IsHorizontalAlignment() ? bounds().width()
                                                    : bounds().height()) -
               2 * GetUnit();
  DCHECK_GT(offset, 0);

  if (!forward)
    offset = -offset;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, true);
  else
    ScrollByYOffset(offset, true);
}

void OverflowBubbleView::ScrollToBeginning() {
  scroll_offset_ = gfx::Vector2dF();
  Layout();
}

void OverflowBubbleView::ScrollToEnd() {
  scroll_offset_ = GetShelf()->IsHorizontalAlignment()
                       ? gfx::Vector2dF(CalculateScrollUpperBound(), 0)
                       : gfx::Vector2dF(0, CalculateScrollUpperBound());
  Layout();
}

gfx::Size OverflowBubbleView::CalculatePreferredSize() const {
  gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(GetAnchorRect().CenterPoint())
          .work_area();
  monitor_rect.Inset(gfx::Insets(kMinimumMargin));

  gfx::Size preferred_size = shelf_container_view_->GetPreferredSize();
  int preferred_length = GetShelf()->IsHorizontalAlignment()
                             ? preferred_size.width()
                             : preferred_size.height();
  preferred_length += 2 * kEndPadding;

  if (GetShelf()->IsHorizontalAlignment()) {
    preferred_size.set_width(std::min(preferred_length, monitor_rect.width()));
  } else {
    preferred_size.set_height(
        std::min(preferred_length, monitor_rect.height()));
  }
  return preferred_size;
}

void OverflowBubbleView::Layout() {
  UpdateLayoutStrategy();

  const gfx::Size shelf_button_size(ShelfConfig::Get()->button_size(),
                                    ShelfConfig::Get()->button_size());
  const gfx::Size arrow_button_size(GetArrowButtonSize(), GetArrowButtonSize());

  bool is_horizontal = GetShelf()->IsHorizontalAlignment();
  gfx::Rect shelf_container_bounds = GetLocalBounds();

  // Transpose and layout as if it is horizontal.
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  // The bounds of |left_arrow_| and |right_arrow_| in the parent coordinates.
  gfx::Rect left_arrow_bounds, right_arrow_bounds;

  // Widen the shelf container view a little bit to ensure enough space for
  // the fading out zone.
  const int fading_zone_inset =
      std::max(0, kFadingZone - GetDistanceToArrowButton() - kEndPadding);

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == SHOW_LEFT_ARROW_BUTTON ||
      layout_strategy_ == SHOW_BUTTONS) {
    left_arrow_bounds = gfx::Rect(shelf_button_size);
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(ShelfConfig::Get()->button_size() +
                                     GetDistanceToArrowButton() -
                                     fading_zone_inset,
                                 0, 0, 0);
  }

  if (layout_strategy_ == SHOW_RIGHT_ARROW_BUTTON ||
      layout_strategy_ == SHOW_BUTTONS) {
    shelf_container_bounds.Inset(0, 0, ShelfConfig::Get()->button_size(), 0);
    right_arrow_bounds =
        gfx::Rect(shelf_container_bounds.top_right(), shelf_button_size);
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
    shelf_container_bounds.Inset(
        0, 0, GetDistanceToArrowButton() - fading_zone_inset, 0);
  }

  shelf_container_bounds.Inset(kEndPadding, 0, kEndPadding, 0);

  // Adjust the bounds when not showing in the horizontal alignment.
  if (!GetShelf()->IsHorizontalAlignment()) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  // Draw |left_arrow| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  if (left_arrow_->GetVisible())
    left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Draw |right_arrow| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  if (right_arrow_->GetVisible())
    right_arrow_->SetBoundsRect(right_arrow_bounds);

  // Draw |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);

  // When the left button shows, the origin of |shelf_container_view_| changes.
  // So translate |shelf_container_view| to show the shelf view correctly.
  gfx::Vector2d translate_vector;
  if (!left_arrow_bounds.IsEmpty()) {
    translate_vector =
        GetShelf()->IsHorizontalAlignment()
            ? gfx::Vector2d(shelf_container_bounds.x() - kEndPadding, 0)
            : gfx::Vector2d(0, shelf_container_bounds.y() - kEndPadding);
  }

  shelf_container_view_->TranslateShelfView(scroll_offset_ + translate_vector);
  shelf_container_view_->UpdateShelfIconsOpacity();
}

void OverflowBubbleView::ChildPreferredSizeChanged(views::View* child) {
  // When contents size is changed, ContentsBounds should be updated before
  // calculating scroll offset.
  SizeToContents();

  // Ensures |shelf_view_| is still visible.
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(0, /*animate=*/false);
  else
    ScrollByYOffset(0, /*animate=*/false);
}

bool OverflowBubbleView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // The MouseWheelEvent was changed to support both X and Y offsets
  // recently, but the behavior of this function was retained to continue
  // using Y offsets only. Might be good to simply scroll in both
  // directions as in OverflowBubbleView::OnScrollEvent.
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(-event.y_offset(), /*animate=*/false);
  else
    ScrollByYOffset(-event.y_offset(), /*animate=*/false);

  return true;
}

const char* OverflowBubbleView::GetClassName() const {
  return "OverflowBubbleView";
}

void OverflowBubbleView::ScrollRectToVisible(const gfx::Rect& rect) {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();

  // |rect| should be a shelf app icon's bounds in OverflowBubbleView's
  // coordinates. Calculates the index of this app icon.
  const int start_location = is_horizontal_alignment ? rect.x() : rect.y();
  const int shelf_container_start_location =
      is_horizontal_alignment ? shelf_container_view_->bounds().x()
                              : shelf_container_view_->bounds().y();
  const int index =
      (start_location - shelf_container_start_location) / GetUnit() +
      shelf_view_->first_visible_index();

  if (index <= GetLastVisibleIndex() && index >= GetFirstVisibleIndex())
    return;

  if (index == shelf_view_->last_visible_index())
    ScrollToEnd();
  else if (index == shelf_view_->first_visible_index())
    ScrollToBeginning();
  else if (index > GetLastVisibleIndex())
    ScrollToNewPage(/*forward=*/true);
  else if (index < GetFirstVisibleIndex())
    ScrollToNewPage(/*forward=*/false);
}

void OverflowBubbleView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {}

void OverflowBubbleView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event,
                                       views::InkDrop* ink_drop) {
  // Verfies that |sender| is either |left_arrow_| or |right_arrow_|.
  views::View* sender_view = sender;
  DCHECK((sender_view == left_arrow_) || (sender_view == right_arrow_));

  ScrollToNewPage(sender_view == right_arrow_);
}

void OverflowBubbleView::OnScrollEvent(ui::ScrollEvent* event) {
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(static_cast<int>(-event->x_offset()), /*animate=*/false);
  else
    ScrollByYOffset(static_cast<int>(-event->y_offset()), /*animate=*/false);
  event->SetHandled();
}

gfx::Rect OverflowBubbleView::GetBubbleBounds() {
  const gfx::Size content_size = GetPreferredSize();
  const gfx::Rect anchor_rect = GetAnchorRect();
  const int distance_to_overflow_button =
      kDistanceToMainShelf +
      (ShelfConfig::Get()->shelf_size() - ShelfConfig::Get()->control_size()) /
          2;
  gfx::Rect monitor_rect =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  // Make sure no part of the bubble touches a screen edge.
  monitor_rect.Inset(gfx::Insets(kMinimumMargin));

  gfx::Rect bounds;
  if (GetShelf()->IsHorizontalAlignment()) {
    bounds = gfx::Rect(
        base::i18n::IsRTL() ? anchor_rect.x()
                            : anchor_rect.right() - content_size.width(),
        anchor_rect.y() - distance_to_overflow_button - content_size.height(),
        content_size.width(), content_size.height());
    if (bounds.x() < monitor_rect.x())
      bounds.Offset(monitor_rect.x() - bounds.x(), 0);
    if (bounds.right() > monitor_rect.right())
      bounds.set_width(monitor_rect.right() - bounds.x());
  } else {
    bounds = gfx::Rect(0, anchor_rect.bottom() - content_size.height(),
                       content_size.width(), content_size.height());
    if (GetShelf()->alignment() == SHELF_ALIGNMENT_LEFT)
      bounds.set_x(anchor_rect.right() + distance_to_overflow_button);
    else
      bounds.set_x(anchor_rect.x() - distance_to_overflow_button -
                   content_size.width());
    if (bounds.y() < monitor_rect.y())
      bounds.Offset(0, monitor_rect.y() - bounds.y());
    if (bounds.bottom() > monitor_rect.bottom())
      bounds.set_height(monitor_rect.bottom() - bounds.y());
  }

  AdjustToEnsureIconsFullyVisible(&bounds);
  return bounds;
}

bool OverflowBubbleView::CanActivate() const {
  if (!GetWidget())
    return false;

  // Do not activate the bubble unless the current active window is the hotseat
  // window or the status widget window.
  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window* bubble_window = GetWidget()->GetNativeWindow();
  aura::Window* hotseat_window =
      GetShelf()->shelf_widget()->hotseat_widget()->GetNativeWindow();
  aura::Window* status_area_window =
      GetShelf()->shelf_widget()->status_area_widget()->GetNativeWindow();
  return active_window == bubble_window || active_window == hotseat_window ||
         active_window == status_area_window;
}

bool OverflowBubbleView::ShouldCloseOnPressDown() {
  return false;
}

bool OverflowBubbleView::ShouldCloseOnMouseExit() {
  return false;
}

int OverflowBubbleView::GetArrowButtonSize() {
  static int kArrowButtonSize = ShelfConfig::Get()->control_size();
  return kArrowButtonSize;
}

Shelf* OverflowBubbleView::GetShelf() {
  return shelf_view_->shelf();
}

const Shelf* OverflowBubbleView::GetShelf() const {
  return shelf_view_->shelf();
}

}  // namespace ash
