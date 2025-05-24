// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_tooltip_view.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  info_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      views::kInfoIcon,
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface),
      kInfoIconSizeDp));

  label_ = AddChildView(login_views_utils::CreateBubbleLabel(message, this));
  label_->SetEnabledColor(
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface));

  GetViewAccessibility().SetRole(ax::mojom::Role::kTooltip);
}

LoginTooltipView::~LoginTooltipView() = default;

BEGIN_METADATA(LoginTooltipView)
END_METADATA

}  // namespace ash
