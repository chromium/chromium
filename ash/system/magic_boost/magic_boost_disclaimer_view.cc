// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>
#include <string>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kWidgetName[] = "MagicBoostDisclaimerViewWidget";

// Paddings, sizes and insets.
constexpr int kImageWidth = 512;
constexpr int kWidgetWidth = kImageWidth;
constexpr int kWidgetHeight = 650;

}  // namespace

MagicBoostDisclaimerView::MagicBoostDisclaimerView() = default;

MagicBoostDisclaimerView::~MagicBoostDisclaimerView() = default;

// static
views::UniqueWidgetPtr MagicBoostDisclaimerView::CreateWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = GetWidgetName();

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<MagicBoostDisclaimerView>());

  // Shows the widget in the middle of the screen.
  // TODO(b/339044721): Set the widget bounds based on different screen size.
  auto bounds = display::Screen::GetScreen()
                    ->GetPrimaryDisplay()
                    .work_area()
                    .CenterPoint();
  widget->SetBounds(gfx::Rect(bounds.x() - kWidgetWidth / 2,
                              bounds.y() - kWidgetHeight / 2, kWidgetWidth,
                              kWidgetHeight));

  return widget;
}

// static
const char* MagicBoostDisclaimerView::GetWidgetName() {
  return kWidgetName;
}

BEGIN_METADATA(MagicBoostDisclaimerView)
END_METADATA

}  // namespace ash
