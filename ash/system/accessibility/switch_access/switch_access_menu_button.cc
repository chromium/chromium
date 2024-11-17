// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access/switch_access_menu_button.h"

#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {
constexpr int kButtonBottomPaddingDefaultDip = 8;
constexpr int kButtonBottomPaddingSmallDip = 1;
constexpr int kButtonTopPaddingDip = 16;
constexpr int kIconSizeDip = 20;
constexpr int kLabelMinSidePaddingDip = 8;
constexpr int kLabelMaxWidthDip =
    SwitchAccessMenuButton::kWidthDip - 2 * kLabelMinSidePaddingDip;
constexpr int kLabelTopPaddingDefaultDip = 16;
constexpr int kLabelTopPaddingSmallDip = 8;
constexpr int kTextLineHeightDip = 20;
}  // namespace

SwitchAccessMenuButton::SwitchAccessMenuButton(std::string action_name,
                                               const gfx::VectorIcon& icon,
                                               int label_text_id)
    : views::Button(
          base::BindRepeating(&SwitchAccessMenuButton::OnButtonPressed,
                              base::Unretained(this))) {
  std::u16string label_text = l10n_util::GetStringUTF16(label_text_id);
  views::Builder<SwitchAccessMenuButton>(this)
      .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
      .AddChildren(views::Builder<views::ImageView>()
                       .CopyAddressTo(&image_view_)
                       .SetImage(ui::ImageModel::FromVectorIcon(
                           icon, kColorAshIconColorPrimary, kIconSizeDip)),
                   views::Builder<views::Label>()
                       .CopyAddressTo(&label_)
                       .SetText(label_text)
                       .SetTextContext(views::style::CONTEXT_BUTTON)
                       .SetAutoColorReadabilityEnabled(false)
                       .SetEnabledColorId(kColorAshTextColorPrimary)
                       .SetMultiLine(true)
                       .SetMaximumWidth(kLabelMaxWidthDip))
      .BuildChildren();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(kButtonTopPaddingDip, kLabelMinSidePaddingDip,
                        kButtonBottomPaddingDefaultDip,
                        kLabelMinSidePaddingDip),
      kLabelTopPaddingDefaultDip);

  // The layout padding changes with the size of the text label.
  gfx::Size label_size =
      label_->CalculatePreferredSize(views::SizeBounds(label_->width(), {}));
  int left_padding_dip = (kWidthDip - label_size.width()) / 2;
  int right_padding_dip = kWidthDip - left_padding_dip - label_size.width();
  int bottom_padding_dip = kButtonBottomPaddingDefaultDip;
  if (label_size.height() > kTextLineHeightDip) {
    bottom_padding_dip = kButtonBottomPaddingSmallDip;
    layout->set_between_child_spacing(kLabelTopPaddingSmallDip);
  }
  layout->set_inside_border_insets(
      gfx::Insets::TLBR(kButtonTopPaddingDip, left_padding_dip,
                        bottom_padding_dip, right_padding_dip));
  SetLayoutManager(std::move(layout));

  GetViewAccessibility().SetName(label_text, ax::mojom::NameFrom::kAttribute);
  GetViewAccessibility().SetIsLeaf(true);
  GetViewAccessibility().SetValue(action_name);
}

void SwitchAccessMenuButton::OnButtonPressed() {
  NotifyAccessibilityEvent(ax::mojom::Event::kClicked,
                           /*send_native_event=*/false);
}

BEGIN_METADATA(SwitchAccessMenuButton)
END_METADATA

}  // namespace ash
