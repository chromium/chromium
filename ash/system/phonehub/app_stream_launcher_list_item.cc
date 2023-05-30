// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_list_item.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/hash/hash.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

constexpr int kEcheAppListItemHeight = 40;
constexpr int kEcheAppLIstItemIconSize = 32;
constexpr double kAlphaValueForInhibitedIconOpacity = 0.38;

}  // namespace

AppStreamLauncherListItem::AppButton::AppButton(
    views::LabelButton::PressedCallback callback,
    const std::u16string& text)
    : views::LabelButton(std::move(callback), text) {
  if (chromeos::features::IsJellyrollEnabled()) {
    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                          *label());
  }
}

AppStreamLauncherListItem::AppButton::~AppButton() = default;

const char* AppStreamLauncherListItem::AppButton::GetClassName() const {
  return "AppStreamLauncherListItemAppButton";
}

AppStreamLauncherListItem::AppStreamLauncherListItem(
    views::LabelButton::PressedCallback callback,
    const phonehub::Notification::AppMetadata& app_metadata) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCollapseMargins(false)
      .SetMinimumCrossAxisSize(kEcheAppListItemHeight)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded,
                                       /*adjust_height_for_width =*/true)
                  .WithWeight(1));

  const bool is_enabled = app_metadata.app_streamability_status ==
                          phonehub::proto::AppStreamabilityStatus::STREAMABLE;

  std::u16string accessible_name = GetAppAccessibleName(app_metadata);

  app_button_ = AddChildView(
      std::make_unique<AppButton>(callback, app_metadata.visible_app_name));

  gfx::ImageSkia resized_app_icon =
      gfx::ImageSkiaOperations::CreateResizedImage(
          app_metadata.color_icon.AsImageSkia(),
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kEcheAppLIstItemIconSize, kEcheAppLIstItemIconSize));

  app_button_->SetImage(views::Button::ButtonState::STATE_NORMAL,
                        resized_app_icon);

  // Fade the image in order to make it look like grayed out.
  app_button_->SetImage(
      views::Button::ButtonState::STATE_DISABLED,
      gfx::ImageSkiaOperations::CreateTransparentImage(
          resized_app_icon, kAlphaValueForInhibitedIconOpacity));

  if (chromeos::features::IsJellyrollEnabled()) {
    ash::StyleUtil::SetUpInkDropForButton(app_button_.get(), gfx::Insets(),
                                          /*highlight_on_hover=*/false,
                                          /*highlight_on_focus=*/true);
    views::FocusRing::Get(app_button_.get())
        ->SetColorId(static_cast<ui::ColorId>(cros_tokens::kCrosSysFocusRing));
    views::InstallRectHighlightPathGenerator(app_button_.get());
  }

  app_button_->SetTooltipText(accessible_name);
  app_button_->SetEnabled(is_enabled);
}

AppStreamLauncherListItem::~AppStreamLauncherListItem() = default;

std::u16string AppStreamLauncherListItem::GetAppAccessibleName(
    const phonehub::Notification::AppMetadata& app_metadata) {
  switch (app_metadata.app_streamability_status) {
    case phonehub::proto::STREAMABLE:
      return app_metadata.visible_app_name;
    case phonehub::proto::BLOCKED_BY_APP:
      return l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_STREAM_NOT_SUPPORTED_BY_APP);
    case phonehub::proto::BLOCK_LISTED:
    default:
      return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_STREAM_NOT_SUPPORTED);
  }
}

bool AppStreamLauncherListItem::HasFocus() const {
  return app_button_->HasFocus();
}

void AppStreamLauncherListItem::RequestFocus() {
  app_button_->RequestFocus();
}

const char* AppStreamLauncherListItem::GetClassName() const {
  return "AppStreamLauncherListItem";
}

views::LabelButton* AppStreamLauncherListItem::GetAppButtonForTest() {
  return app_button_;
}

}  // namespace ash
