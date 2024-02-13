// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_list_item.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {
constexpr int kEcheAppLIstItemIconSize = 32;
constexpr double kAlphaValueForInhibitedIconOpacity = 0.38;
}  // namespace

AppStreamLauncherListItem::AppStreamLauncherListItem(
    PressedCallback callback,
    const phonehub::Notification::AppMetadata& app_metadata)
    : LabelButton(std::move(callback), app_metadata.visible_app_name) {
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                        *label());

  gfx::ImageSkia resized_app_icon =
      gfx::ImageSkiaOperations::CreateResizedImage(
          app_metadata.color_icon.AsImageSkia(),
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kEcheAppLIstItemIconSize, kEcheAppLIstItemIconSize));

  SetImageModel(STATE_NORMAL, ui::ImageModel::FromImageSkia(resized_app_icon));
  // Fade the image in order to make it look like grayed out.
  SetImageModel(views::Button::ButtonState::STATE_DISABLED,
                ui::ImageModel::FromImageSkia(
                    gfx::ImageSkiaOperations::CreateTransparentImage(
                        resized_app_icon, kAlphaValueForInhibitedIconOpacity)));

  ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                        /*highlight_on_hover=*/false,
                                        /*highlight_on_focus=*/true);
  views::FocusRing::Get(this)->SetColorId(
      static_cast<ui::ColorId>(cros_tokens::kCrosSysFocusRing));
  views::InstallRectHighlightPathGenerator(this);

  SetTooltipText(GetAppAccessibleName(app_metadata));
  SetEnabled(app_metadata.app_streamability_status ==
             phonehub::proto::AppStreamabilityStatus::STREAMABLE);
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

BEGIN_METADATA(AppStreamLauncherListItem)
END_METADATA

}  // namespace ash
