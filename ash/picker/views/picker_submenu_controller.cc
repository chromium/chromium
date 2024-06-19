// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_submenu_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_style.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
namespace {

constexpr int kCornerRadius = 12;
constexpr int kSubmenuWidth = 256;
constexpr auto kInsets = gfx::Insets::VH(8, 0);

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::LEFT_TOP,
      views::BubbleBorder::CHROMEOS_SYSTEM_UI_SHADOW);
  border->SetCornerRadius(kPickerContainerBorderRadius);
  border->SetColor(SK_ColorTRANSPARENT);
  return border;
}

class PickerSubmenuView : public views::WidgetDelegateView {
  METADATA_HEADER(PickerSubmenuView, views::WidgetDelegateView)

 public:
  PickerSubmenuView(const gfx::Rect& anchor_rect,
                    std::vector<std::unique_ptr<PickerItemView>> items) {
    SetShowCloseButton(false);
    set_desired_bounds_delegate(
        base::BindRepeating(&PickerSubmenuView::GetDesiredBounds,
                            base::Unretained(this), anchor_rect));
    SetLayoutManager(std::make_unique<views::BoxLayout>(
                         views::BoxLayout::Orientation::kVertical,
                         /*inside_border_insets=*/kInsets))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kStretch);
    SetBackground(views::CreateThemedRoundedRectBackground(
        kPickerContainerBackgroundColor, kCornerRadius));

    // Don't allow submenus within submenus.
    auto* section_view = AddChildView(std::make_unique<PickerSectionView>(
        kSubmenuWidth, /*asset_fetcher=*/nullptr,
        /*submenu_controller=*/nullptr));

    for (std::unique_ptr<PickerItemView>& item : items) {
      section_view->AddItem(std::move(item));
    }
  }

  PickerSubmenuView(const PickerSubmenuView&) = delete;
  PickerSubmenuView& operator=(const PickerSubmenuView&) = delete;
  ~PickerSubmenuView() override = default;

  gfx::Rect GetDesiredBounds(const gfx::Rect& anchor_rect) {
    auto* bubble_frame_view = static_cast<views::BubbleFrameView*>(
        GetWidget()->non_client_view()->frame_view());
    gfx::Rect bounds = bubble_frame_view->GetUpdatedWindowBounds(
        anchor_rect, bubble_frame_view->GetArrow(), GetPreferredSize({}),
        /*adjust_to_fit_available_bounds=*/true);

    // Adjust the bounds to be relative to the parent's bounds.
    const gfx::Rect parent_bounds =
        GetWidget()->parent()->GetWindowBoundsInScreen();
    bounds.Offset(-parent_bounds.OffsetFromOrigin());

    // Shift by the insets to align the first item with the anchor rect.
    bounds.Offset(0, -kInsets.top());
    return bounds;
  }

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    auto frame =
        std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
    frame->SetBubbleBorder(CreateBorder());
    return frame;
  }
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int preferred_height =
        views::View::CalculatePreferredSize(available_size).height();
    return gfx::Size(kSubmenuWidth, preferred_height);
  }
};

BEGIN_METADATA(PickerSubmenuView)
END_METADATA

views::Widget::InitParams CreateInitParams(
    views::View* anchor_view,
    std::unique_ptr<views::WidgetDelegate> delegate) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_BUBBLE);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.autosize = true;
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "PickerSubmenu";
  params.parent = anchor_view->GetWidget()->GetNativeWindow();
  params.delegate = delegate.release();
  return params;
}

}  // namespace

PickerSubmenuController::PickerSubmenuController() = default;

PickerSubmenuController::~PickerSubmenuController() = default;

void PickerSubmenuController::Show(
    views::View* anchor_view,
    std::vector<std::unique_ptr<PickerItemView>> items) {
  widget_ = std::make_unique<views::Widget>(CreateInitParams(
      anchor_view, std::make_unique<PickerSubmenuView>(
                       anchor_view->GetBoundsInScreen(), std::move(items))));
  views::Widget::ReparentNativeView(widget_->GetNativeWindow(),
                                    anchor_view->GetWidget()->GetNativeView());
  widget_->Show();

  // This forces the Widget to reposition itself based on the anchor.
  widget_->OnRootViewLayoutInvalidated();
}

void PickerSubmenuController::Close() {
  if (widget_) {
    widget_->Close();
  }
}

}  // namespace ash
