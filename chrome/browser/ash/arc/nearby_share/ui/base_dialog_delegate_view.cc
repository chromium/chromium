// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/base_dialog_delegate_view.h"

#include <memory>
#include <string>

#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace arc {

BaseDialogDelegateView::BaseDialogDelegateView(views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::Arrow::FLOAT) {
  // Setup delegate
  set_margins(gfx::Insets());
  set_adjust_if_offscreen(true);
  set_close_on_deactivate(false);
  SetModalType(ui::mojom::ModalType::kChild);
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(
          views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true));
  // Don't show Close button
  SetShowCloseButton(false);
}

BaseDialogDelegateView::~BaseDialogDelegateView() = default;

void BaseDialogDelegateView::AddDialogMessage(std::u16string text) {
  AddChildView(views::Builder<views::Label>()
                   .SetText(text)
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .SetTextStyle(views::style::STYLE_PRIMARY)
                   .Build());
}

}  // namespace arc
