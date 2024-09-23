// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

namespace {

constexpr int kErrorIconSize = 16;
constexpr int kErrorIconSpacing = 12;
constexpr int kHeaderSpacing = 4;

}  // namespace

// static
std::unique_ptr<NameTag> NameTag::CreateNameTag(const std::u16string& title,
                                                bool for_editing_list) {
  auto name_tag = std::make_unique<NameTag>(for_editing_list);
  name_tag->SetTitle(title);
  return name_tag;
}

NameTag::NameTag(bool for_editing_list) : for_editing_list_(for_editing_list) {
  Init();
}

NameTag::~NameTag() = default;

void NameTag::SetTitle(const std::u16string& title) {
  title_label_->SetText(title);
}

void NameTag::SetAvailableWidth(size_t available_width) {
  available_width_ = available_width;
  UpdateLabelsFitWidth();
}

void NameTag::SetState(bool is_error, const std::u16string& error_tooltip) {
  error_icon_->SetTooltipText(error_tooltip);
  error_icon_->SetVisible(is_error);

  subtitle_label_->SetText(error_tooltip);
  subtitle_label_->SetVisible(is_error);

  title_label_->SetEnabledColorId(for_editing_list_ && is_error
                                      ? cros_tokens::kCrosSysError
                                      : cros_tokens::kCrosSysOnSurface);
  UpdateLabelsFitWidth();

  // The widget may need a resize.
  auto* widget = GetWidget();
  // No need to update widget when the view is not added to the widget yet.
  if (!widget) {
    return;
  }
  if (auto* editing_list =
          views::AsViewClass<EditingList>(widget->GetContentsView())) {
    editing_list->UpdateWidget();
  } else if (auto* menu = views::AsViewClass<ButtonOptionsMenu>(
                 widget->GetContentsView())) {
    menu->UpdateWidget();
  }
}

void NameTag::Init() {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          for_editing_list_ ? views::BoxLayout::Orientation::kHorizontal
                            : views::BoxLayout::Orientation::kVertical,
          gfx::Insets(),
          /*between_child_spacing=*/for_editing_list_ ? 0 : kHeaderSpacing))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  auto title_label = ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosButton2, u"", cros_tokens::kCrosSysOnSurface);
  auto error_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorOutlineIcon, cros_tokens::kCrosSysError,
          kErrorIconSize));

  if (for_editing_list_) {
    error_icon_ = AddChildView(std::move(error_icon));
  } else {
    title_label_ = AddChildView(std::move(title_label));
  }

  auto* sub_container = AddChildView(std::make_unique<views::View>());
  sub_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          for_editing_list_ ? views::BoxLayout::Orientation::kVertical
                            : views::BoxLayout::Orientation::kHorizontal))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  if (for_editing_list_) {
    title_label_ = sub_container->AddChildView(std::move(title_label));
  } else {
    error_icon_ = sub_container->AddChildView(std::move(error_icon));
  }

  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  error_icon_->SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(0, 0, 0, kErrorIconSpacing));
  // Set height to 20 to add extra internal padding for `error_icon_`.
  error_icon_->SetPreferredSize(gfx::Size(kErrorIconSize, 20));
  error_icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  error_icon_->SetVisible(false);

  subtitle_label_ = sub_container->AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosAnnotation2,
                                     u"", cros_tokens::kCrosSysSecondary));
  subtitle_label_->SetEnabledColorId(cros_tokens::kCrosSysError);
  subtitle_label_->SetMultiLine(true);
  subtitle_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle_label_->SetVisible(false);
}

void NameTag::UpdateLabelsFitWidth() {
  const int error_space =
      error_icon_->GetVisible() ? kErrorIconSize + kErrorIconSpacing : 0;
  title_label_->SizeToFit(for_editing_list_ ? available_width_ - error_space
                                            : available_width_);
  subtitle_label_->SizeToFit(available_width_ - error_space);
}

BEGIN_METADATA(NameTag)
END_METADATA

}  // namespace arc::input_overlay
