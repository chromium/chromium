// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_item_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"

namespace ash {

namespace {

constexpr int kMaxIcons = 5;

// TODO(richui): Replace these temporary values once specs come out.
constexpr gfx::Size kViewSize(250, 40);
constexpr gfx::Size kPreferredSize(250, 150);
constexpr int kIconSpacingDp = 10;
constexpr gfx::Size kPreviewIconSize(40, 40);

}  // namespace

DesksTemplatesItemView::DesksTemplatesItemView() {
  // TODO(richui): Remove all the borders. It is only used for visualizing
  // bounds while it is a placeholder.
  views::View* spacer;
  views::Builder<DesksTemplatesItemView>(this)
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
      .SetPreferredSize(kPreferredSize)
      .SetBorder(views::CreateSolidBorder(/*thickness=*/2, SK_ColorDKGRAY))
      .AddChildren(
          views::Builder<views::View>()
              .CopyAddressTo(&name_view_)
              .SetPreferredSize(kViewSize)
              .SetBorder(views::CreateSolidBorder(
                  /*thickness=*/2, SK_ColorGRAY)),
          views::Builder<views::View>()
              .CopyAddressTo(&time_view_)
              .SetPreferredSize(kViewSize)
              .SetBorder(views::CreateSolidBorder(
                  /*thickness=*/2, SK_ColorGRAY)),
          views::Builder<views::View>().CopyAddressTo(&spacer),
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&preview_view_)
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(kIconSpacingDp))
      .BuildChildren();

  SetFlexForView(spacer, 1);
  SetIcons();
}

DesksTemplatesItemView::~DesksTemplatesItemView() = default;

void DesksTemplatesItemView::SetIcons() {
  for (int i = 0; i < kMaxIcons; ++i) {
    preview_view_->AddChildView(views::Builder<views::View>()
                                    .SetPreferredSize(kPreviewIconSize)
                                    .SetBorder(views::CreateSolidBorder(
                                        /*thickness=*/2, SK_ColorLTGRAY))
                                    .Build());
  }
}

BEGIN_METADATA(DesksTemplatesItemView, views::BoxLayoutView)
END_METADATA

}  // namespace ash
