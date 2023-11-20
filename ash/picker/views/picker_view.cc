// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <memory>

#include "ash/picker/views/picker_search_field_view.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
namespace {

constexpr gfx::Size kPickerSize(420, 480);
constexpr int kBorderRadius = 20;
constexpr int kShadowElevation = 3;
constexpr ui::ColorId kBackgroundColor = cros_tokens::kCrosSysBaseElevated;
constexpr auto kSearchFieldMargins = gfx::Insets::TLBR(16, 16, 8, 16);

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
  border->SetCornerRadius(kBorderRadius);
  border->set_md_shadow_elevation(kShadowElevation);
  return border;
}

std::unique_ptr<AshWebView> CreateWebView(PickerView::Delegate& delegate) {
  std::unique_ptr<AshWebView> view =
      delegate.CreateWebView(AshWebView::InitParams{
          .rounded_corners = gfx::RoundedCornersF(
              /*upper_left=*/0, /*upper_right*/ 0,
              /*lower_right=*/kBorderRadius, /*lower_left=*/kBorderRadius)});
  view->Navigate(GURL("chrome://picker"));
  view->SetLayoutManager(std::make_unique<views::FillLayout>());
  // Fill up all remaining space with the view.
  view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  return view;
}

}  // namespace

PickerView::PickerView(std::unique_ptr<Delegate> delegate) {
  SetShowCloseButton(false);
  SetBackground(views::CreateThemedSolidBackground(kBackgroundColor));
  SetPreferredSize(kPickerSize);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  // TODO(b/310088250): Perform a search when the search callback is called.
  search_field_view_ =
      AddChildView(std::make_unique<PickerSearchFieldView>(base::DoNothing()));
  search_field_view_->SetProperty(views::kMarginsKey, kSearchFieldMargins);

  // Automatically focus on the search field.
  SetInitiallyFocusedView(search_field_view_);

  web_view_ = AddChildView(CreateWebView(*delegate));
}

PickerView::~PickerView() = default;

views::UniqueWidgetPtr PickerView::CreateWidget(
    std::unique_ptr<PickerView::Delegate> delegate) {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.delegate = new PickerView(std::move(delegate));
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.type = views::Widget::InitParams::TYPE_BUBBLE;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "Picker";

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetVisibilityAnimationTransition(
      views::Widget::VisibilityTransition::ANIMATE_HIDE);
  return widget;
}

std::unique_ptr<views::NonClientFrameView> PickerView::CreateNonClientFrameView(
    views::Widget* widget) {
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
  frame->SetBubbleBorder(CreateBorder());
  return frame;
}

BEGIN_METADATA(PickerView, views::View)
END_METADATA

}  // namespace ash
