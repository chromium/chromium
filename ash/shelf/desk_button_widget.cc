// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/desk_button_widget.h"

#include "ash/focus_cycler.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
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
}

bool DeskButtonWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
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
  return is_expanded_ ? GetPreferredExpandedWidth() : kDeskButtonHeight;
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
  is_expanded_ = expanded;

  if (is_horizontal_shelf_ && ShouldBeVisible()) {
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
  // Even if the expanded state changed, do not update the widget bounds.
  // `PrepareForAlignmentChange()` is bound to be followed by the shelf
  // layout, at which point desk button widget bounds will be updated to
  // match the current expanded state.
}

void DeskButtonWidget::CalculateTargetBounds() {
  target_bounds_ =
      is_expanded_ ? GetTargetExpandedBounds() : GetTargetShrunkBounds();
}

gfx::Rect DeskButtonWidget::GetTargetBounds() const {
  return target_bounds_;
}

void DeskButtonWidget::UpdateLayout(bool animate) {
  if (ShouldBeVisible()) {
    SetBounds(GetTargetBounds());
    ShowInactive();
  } else {
    Hide();
  }
}

void DeskButtonWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (is_horizontal_shelf_) {
    target_bounds_.set_y(shelf_position);
  } else {
    target_bounds_.set_x(shelf_position);
  }
}

void DeskButtonWidget::HandleLocaleChange() {}

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
    return gfx::Point(navigation_bounds.right() + shelf_padding.left(),
                      navigation_bounds.y() + kDeskButtonInsets);
  }

  // TODO(b/272383056): We might want to find a better way of calculating this
  // because shelf_padding is not sufficient.
  return gfx::Point(
      navigation_bounds.x() + kDeskButtonInsets,
      navigation_bounds.y() + navigation_bounds.height() + shelf_padding.top());
}

}  // namespace ash
