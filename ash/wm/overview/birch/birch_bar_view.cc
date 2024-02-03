// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kMaxChipsNum = 4;
constexpr int kChipSpacing = 8;
constexpr int kBarBottomPadding = 10;

// ----- For test use -----------
std::unique_ptr<views::Widget> g_widget_for_testing;

}  // namespace

BirchBarView::BirchBarView() {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBetweenChildSpacing(kChipSpacing);
}

BirchBarView::~BirchBarView() = default;

void BirchBarView::ShowWidgetForTesting(
    std::unique_ptr<BirchBarView> bar_view) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  const gfx::Rect work_area =
      WorkAreaInsets::ForWindow(root_window)->user_work_area_bounds();
  const gfx::Size bar_size = bar_view->GetPreferredSize();
  params.bounds =
      gfx::Rect(gfx::Point(work_area.bottom_center().x() - bar_size.width() / 2,
                           work_area.bottom_center().y() - bar_size.height() -
                               kBarBottomPadding),
                bar_size);
  params.parent = Shell::Get()->GetContainer(
      root_window, kShellWindowId_AlwaysOnTopContainer);

  g_widget_for_testing = std::make_unique<views::Widget>(std::move(params));
  g_widget_for_testing->SetContentsView(std::move(bar_view));
  g_widget_for_testing->Show();
}

void BirchBarView::HideWidgetForTesting() {
  if (g_widget_for_testing) {
    g_widget_for_testing->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    g_widget_for_testing.reset();
  }
}

void BirchBarView::AddChip(
    const ui::ImageModel& icon,
    const std::u16string& title,
    const std::u16string& sub_title,
    views::Button::PressedCallback callback,
    std::optional<std::u16string> button_title,
    std::optional<views::Button::PressedCallback> button_callback) {
  if (static_cast<int>(chips_.size()) == kMaxChipsNum) {
    NOTREACHED() << "The number of birch chips reaches the limit of 4";
    return;
  }
  auto chip = views::Builder<BirchChipButton>()
                  .SetIconImage(icon)
                  .SetTitleText(title)
                  .SetSubtitleText(sub_title)
                  .SetCallback(std::move(callback))
                  .SetDelegate(this)
                  .Build();
  if (button_title.has_value() && button_callback.has_value()) {
    chip->SetActionButton(button_title.value(),
                          std::move(button_callback.value()));
  }
  chips_.emplace_back(AddChildView(std::move(chip)));
}

void BirchBarView::RemoveChip(BirchChipButton* chip) {
  RemoveChildViewT(chip);
}

BEGIN_METADATA(BirchBarView, views::BoxLayoutView)
END_METADATA

}  // namespace ash
