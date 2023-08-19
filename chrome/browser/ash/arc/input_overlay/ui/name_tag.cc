// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace arc::input_overlay {

// static
std::unique_ptr<NameTag> NameTag::CreateNameTag(const std::u16string& title) {
  auto name_tag = std::make_unique<NameTag>();
  name_tag->SetTitle(title);
  return name_tag;
}

NameTag::NameTag() {
  Init();
}

NameTag::~NameTag() = default;

void NameTag::SetTitle(const std::u16string& title) {
  title_label_->SetText(title);
}

void NameTag::SetSubtitle(const std::u16string& subtitle) {
  subtitle_label_->SetText(subtitle);
}

void NameTag::SetState(bool is_error, const std::u16string& error_tooltip) {
  error_icon_->SetTooltipText(error_tooltip);
  error_icon_->SetVisible(is_error);
  SetTextColor(is_error ? cros_tokens::kCrosSysError
                        : cros_tokens::kCrosSysSecondary);
}

void NameTag::Init() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart);

  error_icon_ = AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorOutlineIcon, cros_tokens::kCrosSysError, 12)));
  error_icon_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 12));
  error_icon_->SetVisible(false);

  auto* text_container = AddChildView(std::make_unique<views::View>());
  text_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  title_label_ = text_container->AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosButton1, u"",
                                     cros_tokens::kCrosRefNeutral100));
  subtitle_label_ = text_container->AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosAnnotation2,
                                     u"", cros_tokens::kCrosSysSecondary));
}

void NameTag::SetTextColor(ui::ColorId color_id) {
  title_label_->SetEnabledColorId(color_id);
  subtitle_label_->SetEnabledColorId(color_id);
}

}  // namespace arc::input_overlay
