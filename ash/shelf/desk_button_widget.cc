// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/desk_button_widget.h"

#include "ash/focus_cycler.h"
#include "ash/screen_util.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

constexpr int kDeskButtonLandscapeLargeWidth = 148;
constexpr int kDeskButtonLargeDisplayThreshold = 1280;
constexpr int kDeskButtonLandscapeSmallWidth = 108;
constexpr int kDeskButtonHeight = 48;
constexpr int kDeskButtonCornerRadius = 12;
constexpr int kDeskButtonInsets = 6;

}  // namespace

class DeskButtonWidget::DelegateView : public views::WidgetDelegateView,
                                       public views::ViewTargeterDelegate {
 public:
  DelegateView() {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);

    // TODO(b/272383056): Replace placeholder PillButton.
    std::unique_ptr<PillButton> desk_button =
        views::Builder<PillButton>().SetTooltipText(u"Show desk").Build();
    desk_button_ = GetContentsView()->AddChildView(std::move(desk_button));
    desk_button_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kDeskButtonCornerRadius));
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  DelegateView(const DelegateView&) = delete;
  DelegateView& operator=(const DelegateView&) = delete;

  ~DelegateView() override;

  // Initializes the view.
  void Init(DeskButtonWidget* desk_button_widget);

  // views::ViewTargetDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  }

  // views::WidgetDelegateView:
  bool CanActivate() const override;

 private:
  PillButton* desk_button_ = nullptr;
  DeskButtonWidget* desk_button_widget_ = nullptr;
};

DeskButtonWidget::DelegateView::~DelegateView() = default;

void DeskButtonWidget::DelegateView::Init(
    DeskButtonWidget* desk_button_widget) {
  desk_button_widget_ = desk_button_widget;
}

bool DeskButtonWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
}

DeskButtonWidget::DeskButtonWidget(Shelf* shelf) : shelf_(shelf) {
  CHECK(shelf_);
}

DeskButtonWidget::~DeskButtonWidget() = default;

int DeskButtonWidget::GetPreferredLength() const {
  if (!shelf_->IsHorizontalAlignment()) {
    return kDeskButtonHeight;
  }
  gfx::NativeWindow native_window = GetNativeWindow();
  if (!native_window) {
    return 0;
  }
  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(native_window);
  return display_bounds.width() > kDeskButtonLargeDisplayThreshold
             ? kDeskButtonLandscapeLargeWidth
             : kDeskButtonLandscapeSmallWidth;
}

bool DeskButtonWidget::ShouldBeVisible() const {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  const OverviewController* overview_controller =
      Shell::Get()->overview_controller();

  return layout_manager->is_active_session_state() &&
         !overview_controller->InOverviewSession() &&
         shelf_->hotseat_widget()->state() == HotseatState::kShownClamshell;
}

void DeskButtonWidget::CalculateTargetBounds() {
  gfx::Rect navigation_bounds = shelf_->navigation_widget()->GetTargetBounds();
  gfx::Insets shelf_padding =
      shelf_->hotseat_widget()
          ->scrollable_shelf_view()
          ->CalculateMirroredEdgePadding(/*use_target_bounds=*/true);
  gfx::Rect available_rect;

  if (shelf_->IsHorizontalAlignment()) {
    const int target_width = GetPreferredLength();
    available_rect.set_origin(
        gfx::Point(navigation_bounds.right() + shelf_padding.left(),
                   navigation_bounds.y()));
    available_rect.set_size(gfx::Size(target_width, kDeskButtonHeight));
  } else {
    available_rect.set_origin(
        gfx::Point(navigation_bounds.x(), navigation_bounds.y() +
                                              navigation_bounds.height() +
                                              shelf_padding.top()));
    available_rect.set_size(gfx::Size(kDeskButtonHeight, kDeskButtonHeight));
  }
  available_rect.Inset(kDeskButtonInsets);
  target_bounds_ = available_rect;
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
  if (shelf_->IsHorizontalAlignment()) {
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
}

}  // namespace ash
