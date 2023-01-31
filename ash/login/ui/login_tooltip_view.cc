// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_tooltip_view.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/style/ash_color_id.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

// The size of the info icon in the tooltip view.
constexpr int kInfoIconSizeDp = 20;

}  // namespace

LoginTooltipView::LoginTooltipView(const std::u16string& message,
                                   base::WeakPtr<views::View> anchor_view)
    : LoginBaseBubbleView(std::move(anchor_view)) {
  info_icon_ = AddChildView(std::make_unique<views::ImageView>());
  info_icon_->SetPreferredSize(gfx::Size(kInfoIconSizeDp, kInfoIconSizeDp));
  info_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      views::kInfoIcon, kColorAshIconColorPrimary));

  label_ = AddChildView(login_views_utils::CreateBubbleLabel(message, this));
  label_->SetEnabledColorId(kColorAshTextColorPrimary);
}

LoginTooltipView::~LoginTooltipView() = default;

void LoginTooltipView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTooltip;
}

}  // namespace ash
