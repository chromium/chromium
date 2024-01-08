// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr gfx::Insets kResultPadding(8);

// TODO: b/310088338 - Get a relevant icon for each item.
const gfx::VectorIcon& kPlaceholderIcon = vector_icons::kGoogleColorIcon;

constexpr int kIconSizeDip = 20;
constexpr int kPaddingBetweenIconAndTitle = 12;

}  // namespace

PickerItemView::PickerItemView(views::Button::PressedCallback callback,
                               const std::u16string& text)
    : views::Button(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  auto* icon = AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      kPlaceholderIcon, cros_tokens::kCrosSysOnSurface, kIconSizeDip));
  icon->SetProperty(views::kMarginsKey,
                    gfx::Insets::TLBR(0, 0, 0, kPaddingBetweenIconAndTitle));

  AddChildView(bubble_utils::CreateLabel(TypographyToken::kCrosBody2, text,
                                         cros_tokens::kCrosSysOnSurface));

  SetBorder(views::CreateEmptyBorder(kResultPadding));
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);

  SetAccessibleName(text);
}

PickerItemView::~PickerItemView() = default;

BEGIN_METADATA(PickerItemView)
END_METADATA

}  // namespace ash
