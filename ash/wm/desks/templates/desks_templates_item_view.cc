// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_item_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/desks_templates_delete_button.h"
#include "base/notreached.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr int kMaxIcons = 5;

// TODO(richui): Replace these temporary values once specs come out.
constexpr gfx::Size kViewSize(250, 40);
constexpr gfx::Size kPreferredSize(250, 150);
constexpr int kIconSpacingDp = 10;
constexpr gfx::Size kPreviewIconSize(40, 40);
constexpr int kDeleteButtonMargin = 8;
constexpr int kDeleteButtonSize = 24;

}  // namespace

DesksTemplatesItemView::DesksTemplatesItemView() {
  // TODO(richui): Remove all the borders. It is only used for visualizing
  // bounds while it is a placeholder.
  auto delete_button_callback = base::BindRepeating(
      &DesksTemplatesItemView::OnDeleteButtonPressed, base::Unretained(this));

  views::View* spacer;
  views::BoxLayoutView* container;
  views::Builder<DesksTemplatesItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetBorder(views::CreateSolidBorder(/*thickness=*/2, SK_ColorDKGRAY))
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&container)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .AddChildren(views::Builder<views::View>()
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
                               .SetOrientation(
                                   views::BoxLayout::Orientation::kHorizontal)
                               .SetBetweenChildSpacing(kIconSpacingDp)),
          views::Builder<DesksTemplatesDeleteButton>()
              .CopyAddressTo(&delete_button_)
              .SetCallback(delete_button_callback))
      .BuildChildren();

  container->SetFlexForView(spacer, 1);
  UpdateDeleteButtonVisibility();
  SetIcons();
}

DesksTemplatesItemView::~DesksTemplatesItemView() = default;

void DesksTemplatesItemView::UpdateDeleteButtonVisibility() {
  // For switch access, setting the delete button to visible allows users to
  // navigate to it.
  // TODO(richui): update `force_show_delete_button_` based on touch events.
  delete_button_->SetVisible(
      (IsMouseHovered() || force_show_delete_button_ ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning()));
}

void DesksTemplatesItemView::Layout() {
  views::View::Layout();

  delete_button_->SetBoundsRect(
      gfx::Rect(width() - kDeleteButtonSize - kDeleteButtonMargin,
                kDeleteButtonMargin, kDeleteButtonSize, kDeleteButtonSize));
}

void DesksTemplatesItemView::SetIcons() {
  for (int i = 0; i < kMaxIcons; ++i) {
    preview_view_->AddChildView(views::Builder<views::View>()
                                    .SetPreferredSize(kPreviewIconSize)
                                    .SetBorder(views::CreateSolidBorder(
                                        /*thickness=*/2, SK_ColorLTGRAY))
                                    .Build());
  }
}

void DesksTemplatesItemView::OnDeleteButtonPressed() {
  // TODO(richui): Hook this up to the presenter.
  NOTIMPLEMENTED();
}

BEGIN_METADATA(DesksTemplatesItemView, views::View)
END_METADATA

}  // namespace ash
