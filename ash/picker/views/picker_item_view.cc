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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
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

constexpr gfx::Insets kPickerItemPadding(8);

constexpr int kIconSizeDip = 20;
constexpr int kPaddingBetweenIconAndTitle = 12;

}  // namespace

PickerItemView::PickerItemView(views::Button::PressedCallback callback)
    : views::Button(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  text_label_ = AddChildView(bubble_utils::CreateLabel(
      TypographyToken::kCrosBody2, u"", cros_tokens::kCrosSysOnSurface));

  SetBorder(views::CreateEmptyBorder(kPickerItemPadding));
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
}

PickerItemView::~PickerItemView() = default;

void PickerItemView::SetText(const std::u16string& text) {
  text_label_->SetText(text);
  SetAccessibleName(text);
}

void PickerItemView::SetIcon(const gfx::VectorIcon& icon) {
  if (icon_view_ == nullptr) {
    icon_view_ = AddChildViewAt(
        views::Builder<views::ImageView>()
            .SetCanProcessEventsWithinSubtree(false)
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(0, 0, 0, kPaddingBetweenIconAndTitle))
            .Build(),
        0);
  }
  icon_view_->SetImage(
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, kIconSizeDip));
}

void PickerItemView::SetImageContents(
    std::unique_ptr<views::ImageView> image_contents) {
  if (image_contents_ != nullptr) {
    RemoveChildViewT(image_contents_);
  }
  image_contents_ = AddChildView(std::move(image_contents));
  image_contents_->SetCanProcessEventsWithinSubtree(false);
  // TODO: b/316936418 - Get accessible name for image contents.
  SetAccessibleName(u"image contents");
}

BEGIN_METADATA(PickerItemView)
END_METADATA

}  // namespace ash
