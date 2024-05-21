// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_badge_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr gfx::Size kIconSize(10, 10);
constexpr auto kIconPadding = gfx::Insets::VH(5, 5);
constexpr int kBadgeCornerRadius = 4;

}  // namespace

PickerBadgeView::PickerBadgeView() {
  views::Builder<views::FlexLayoutView>(this)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysHoverOnSubtle, kBadgeCornerRadius))
      .AddChildren(views::Builder<views::ImageView>()
                       .SetBorder(views::CreateEmptyBorder(kIconPadding))
                       .SetImage(ui::ImageModel::FromVectorIcon(
                           kPickerReturnIcon, cros_tokens::kCrosSysOnSurface))
                       .SetImageSize(kIconSize))
      .BuildChildren();
}

PickerBadgeView::~PickerBadgeView() = default;

BEGIN_METADATA(PickerBadgeView)
END_METADATA

}  // namespace ash
