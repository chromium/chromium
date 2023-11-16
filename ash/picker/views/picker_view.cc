// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <memory>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
namespace {

constexpr gfx::Size kPickerSize(420, 480);
constexpr int kBorderRadius = 20;
constexpr int kShadowElevation = 3;
constexpr ui::ColorId kBackgroundColor = cros_tokens::kCrosSysBaseElevated;

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
  border->SetCornerRadius(kBorderRadius);
  border->set_md_shadow_elevation(kShadowElevation);
  return border;
}

}  // namespace

PickerView::PickerView() {
  SetShowCloseButton(false);
  SetBackground(views::CreateThemedSolidBackground(kBackgroundColor));
  SetPreferredSize(kPickerSize);
}

PickerView::~PickerView() = default;

views::UniqueWidgetPtr PickerView::CreateWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.delegate = new PickerView;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.type = views::Widget::InitParams::TYPE_BUBBLE;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "Picker";

  return std::make_unique<views::Widget>(std::move(params));
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
