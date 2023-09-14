// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/desk_button_widget.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/i18n/rtl.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

constexpr int kDeskButtonLargeWidth = 136;
constexpr int kDeskButtonSmallWidth = 96;
constexpr int kDeskButtonHeight = 36;
constexpr int kDeskButtonLargeDisplayThreshold = 1280;
constexpr int kDeskButtonInsets = 6;

}  // namespace

class DeskButtonWidget::DelegateView : public views::WidgetDelegateView {
 public:
  DelegateView() {
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  DelegateView(const DelegateView&) = delete;
  DelegateView& operator=(const DelegateView&) = delete;

  ~DelegateView() override;

  DeskButton* desk_button() const { return desk_button_; }

  // Initializes the view.
  void Init(DeskButtonWidget* desk_button_widget);

  // views::WidgetDelegateView:
  bool CanActivate() const override;

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Notifies the `desk_button_` to update layout and values based on the new
  // expanded state.
  void OnExpandedStateUpdate(bool expanded);

  // Tells the `desk_button_` whether it should stay expanded regardless of
  // interactions with the button.
  void SetForceExpandedState(bool force_expanded_state);

 private:
  raw_ptr<DeskButton> desk_button_ = nullptr;
  raw_ptr<DeskButtonWidget> desk_button_widget_ = nullptr;
};

DeskButtonWidget::DelegateView::~DelegateView() = default;

void DeskButtonWidget::DelegateView::Init(
    DeskButtonWidget* desk_button_widget) {
  desk_button_widget_ = desk_button_widget;
  desk_button_ = GetContentsView()->AddChildView(
      std::make_unique<DeskButton>(desk_button_widget_));
  OnExpandedStateUpdate(desk_button_widget->is_expanded());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

bool DeskButtonWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
}

bool DeskButtonWidget::DelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  GetWidget()->Deactivate();
  return true;
}

void DeskButtonWidget::DelegateView::OnExpandedStateUpdate(bool expanded) {
  desk_button_->OnExpandedStateUpdate(expanded);
}

void DeskButtonWidget::DelegateView::SetForceExpandedState(
    bool force_expanded_state) {
  desk_button_->set_force_expanded_state(force_expanded_state);
}

DeskButtonWidget::DeskButtonWidget(Shelf* shelf)
    : shelf_(shelf),
      is_horizontal_shelf_(shelf_->IsHorizontalAlignment()),
      is_expanded_(is_horizontal_shelf_) {
  CHECK(shelf_);
}

DeskButtonWidget::~DeskButtonWidget() = default;

int DeskButtonWidget::GetPreferredLength() const {
  return is_expanded_ && is_horizontal_shelf_ ? GetPreferredExpandedWidth()
                                              : kDeskButtonHeight;
}

int DeskButtonWidget::GetPreferredExpandedWidth() const {
  gfx::NativeWindow native_window = GetNativeWindow();
  if (!native_window) {
    return 0;
  }
  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(native_window);
  return display_bounds.width() > kDeskButtonLargeDisplayThreshold
             ? kDeskButtonLargeWidth
             : kDeskButtonSmallWidth;
}

gfx::Rect DeskButtonWidget::GetTargetShrunkBounds() const {
  return gfx::Rect(GetCenteredOrigin(),
                   gfx::Size(kDeskButtonHeight, kDeskButtonHeight));
}

gfx::Rect DeskButtonWidget::GetTargetExpandedBounds() const {
  gfx::Rect current_bounds = GetTargetShrunkBounds();
  const int width = GetPreferredExpandedWidth();
  gfx::Point new_origin = current_bounds.top_right();
  current_bounds.set_width(width);

  // We need to change the origin only when the alignment is on the right side
  // because the bounds expand rightward.
  if (shelf_->alignment() == ShelfAlignment::kRight) {
    new_origin.Offset(-width, 0);
    current_bounds.set_origin(new_origin);
  }

  return current_bounds;
}

void DeskButtonWidget::MaybeFocusOut(bool reverse) {
  // The focus order is the previous desk button, the desk button, then the next
  // desk button.
  views::View* views[] = {GetDeskButton()->prev_desk_button(), GetDeskButton(),
                          GetDeskButton()->next_desk_button()};

  // The desk button will still be drawn in LTR, with the previous desk button
  // on the left, when in RTL mode.
  if (base::i18n::IsRTL()) {
    std::swap(views[0], views[2]);
  }

  views::View* focused_view = GetFocusManager()->GetFocusedView();

  const int count = std::size(views);
  int focused = base::ranges::find(views, focused_view) - std::begin(views);
  // This method is only called if the desk button widget already has focus.
  CHECK(focused != count);

  int next = focused + (reverse ? -1 : 1);
  // Only the previous and next desk buttons can be disabled. If they are next,
  // the current focus is on the desk button and focus should leave the desk
  // button widget.
  if (next < 0 || next >= count || !views[next]->GetEnabled()) {
    FocusOut(reverse);
    return;
  }
  views[next]->RequestFocus();
}

bool DeskButtonWidget::ShouldBeVisible() const {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  const OverviewController* overview_controller =
      Shell::Get()->overview_controller();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  return layout_manager->is_active_session_state() &&
         !overview_controller->InOverviewSession() &&
         shelf_->hotseat_widget()->state() == HotseatState::kShownClamshell &&
         GetDeskButtonVisibility(prefs);
}

void DeskButtonWidget::SetExpanded(bool expanded) {
  if (is_expanded_ == expanded || !ShouldBeVisible()) {
    return;
  }

  is_expanded_ = expanded;

  if (is_horizontal_shelf_) {
    // If we are in horizontal alignment, then we need to recalculate and update
    // the hotseat bounds with the new button state before recalculating and
    // updating the desk button bounds so that the hotseat provides the correct
    // shelf padding and so that it does not think that it is still overflown
    // when the desk button shrinks. We call `LayoutShelf` to achieve this.
    shelf_->shelf_layout_manager()->LayoutShelf();
  } else {
    // For vertical shelf, the desk button expanded state does not affect
    // overall shelf layout, as it always uses up the same amount of space.
    // In this case, it's sufficient to update the `DeskButtonWidget` bounds
    // only.
    CalculateTargetBounds();
    SetBounds(GetTargetBounds());
  }

  delegate_view_->OnExpandedStateUpdate(is_expanded_);
}

void DeskButtonWidget::PrepareForAlignmentChange(ShelfAlignment new_alignment) {
  is_horizontal_shelf_ = new_alignment == ShelfAlignment::kBottom;
  delegate_view_->SetForceExpandedState(is_horizontal_shelf_);
  is_expanded_ = is_horizontal_shelf_;
  delegate_view_->OnExpandedStateUpdate(is_expanded_);

  // Hide the widget first to avoid unneeded animation.
  Hide();
}

void DeskButtonWidget::CalculateTargetBounds() {
  if (ShouldBeVisible()) {
    target_bounds_ =
        is_expanded_ ? GetTargetExpandedBounds() : GetTargetShrunkBounds();
  } else {
    target_bounds_ = gfx::Rect();
  }
}

gfx::Rect DeskButtonWidget::GetTargetBounds() const {
  return target_bounds_;
}

void DeskButtonWidget::UpdateLayout(bool animate) {
  const gfx::Rect initial_bounds = GetWindowBoundsInScreen();
  const bool visibility = GetVisible();
  const bool target_visibility = ShouldBeVisible();
  if (initial_bounds == target_bounds_ && visibility == target_visibility) {
    return;
  }

  if (!animate || visibility != target_visibility) {
    if (target_visibility) {
      SetBounds(target_bounds_);
      ShowInactive();
    } else {
      Hide();
    }

    return;
  }

  // We only animate x axis movement for bottom shelf and y axis movement for
  // side shelf when the widget size remains the same and non empty.
  const bool animate_transform =
      initial_bounds.size() == target_bounds_.size() &&
      !target_bounds_.IsEmpty() &&
      ((is_horizontal_shelf_ && initial_bounds.y() == target_bounds_.y()) ||
       (!is_horizontal_shelf_ && initial_bounds.x() == target_bounds_.x()));

  if (animate_transform) {
    const gfx::Transform initial_transform = gfx::TransformBetweenRects(
        gfx::RectF(target_bounds_), gfx::RectF(initial_bounds));
    SetBounds(target_bounds_);
    GetNativeView()->layer()->SetTransform(initial_transform);
  }

  ui::ScopedLayerAnimationSettings animation_setter(
      GetNativeView()->layer()->GetAnimator());
  animation_setter.SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (animate_transform) {
    GetNativeView()->layer()->SetTransform(gfx::Transform());
  } else {
    SetBounds(target_bounds_);
  }
}

void DeskButtonWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (is_horizontal_shelf_) {
    target_bounds_.set_y(shelf_position);
  } else {
    target_bounds_.set_x(shelf_position);
  }
}

void DeskButtonWidget::HandleLocaleChange() {
  // The desk button be laid out LTR even in RTL mode.
  GetDeskButton()->ReorderChildView(GetDeskButton()->prev_desk_button(),
                                    base::i18n::IsRTL() ? 3 : 1);
  GetDeskButton()->ReorderChildView(GetDeskButton()->next_desk_button(),
                                    base::i18n::IsRTL() ? 1 : 3);
}

void DeskButtonWidget::Initialize(aura::Window* container) {
  CHECK(container);
  delegate_view_ = new DelegateView();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "DeskButtonWidget";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = delegate_view_;
  params.parent = container;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  Init(std::move(params));
  set_focus_on_creation(false);
  delegate_view_->SetEnableArrowKeyTraversal(true);

  delegate_view_->Init(this);
  delegate_view_->SetForceExpandedState(is_horizontal_shelf_);

  CalculateTargetBounds();
  UpdateLayout(/*animate=*/false);
}

DeskButton* DeskButtonWidget::GetDeskButton() const {
  return delegate_view_->desk_button();
}

gfx::Point DeskButtonWidget::GetCenteredOrigin() const {
  const gfx::Rect navigation_bounds =
      shelf_->navigation_widget()->GetTargetBounds();
  const gfx::Insets shelf_padding =
      shelf_->hotseat_widget()
          ->scrollable_shelf_view()
          ->CalculateMirroredEdgePadding(/*use_target_bounds=*/true);

  if (is_horizontal_shelf_) {
    // TODO(b/272383056): We might want to find a better way of calculating this
    // because shelf_padding is not sufficient.
    const int shelf_padding_offset =
        base::i18n::IsRTL() ? -navigation_bounds.width() -
                                  shelf_padding.right() - GetPreferredLength()
                            : shelf_padding.left();
    return gfx::Point(navigation_bounds.right() + shelf_padding_offset,
                      navigation_bounds.y() + kDeskButtonInsets);
  }

  // TODO(b/272383056): We might want to find a better way of calculating this
  // because shelf_padding is not sufficient.
  return gfx::Point(
      navigation_bounds.x() + kDeskButtonInsets,
      navigation_bounds.y() + navigation_bounds.height() + shelf_padding.top());
}

void DeskButtonWidget::FocusOut(bool reverse) {
  GetDeskButton()->MaybeContract();
  shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kDeskButton);
}

bool DeskButtonWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active)) {
    return false;
  }

  // The next desk button will always be on the right, even in RTL, so it should
  // default focus if `default_last_focusable_child_` is true (meaning we are
  // reverse tab cycling) or we are in RTL. If both are true or neither are true
  // then the previous desk button should default focus.
  const bool default_focus_right =
      default_last_focusable_child_ != base::i18n::IsRTL();

  if (active) {
    if (default_focus_right &&
        GetDeskButton()->next_desk_button()->GetEnabled()) {
      GetDeskButton()->next_desk_button()->RequestFocus();
    } else if (!default_focus_right &&
               GetDeskButton()->prev_desk_button()->GetEnabled()) {
      GetDeskButton()->prev_desk_button()->RequestFocus();
    } else {
      GetDeskButton()->RequestFocus();
    }
  }
  return true;
}

}  // namespace ash
