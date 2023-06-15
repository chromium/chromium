// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace arc::input_overlay {

// static
std::unique_ptr<NameTag> NameTag::CreateNameTag(
    const std::u16string& title,
    const std::u16string& sub_title) {
  auto name_tag = std::make_unique<NameTag>();
  name_tag->SetTitle(title);
  name_tag->SetSubtitle(sub_title);
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

void NameTag::Init() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  title_label_ = AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosButton1, u"",
                                     cros_tokens::kCrosRefNeutral100));
  subtitle_label_ = AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosAnnotation2,
                                     u"", cros_tokens::kCrosSysSecondary));
}

}  // namespace arc::input_overlay
