// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_style_utils.h"
#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/pill_button.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash::notification_style_utils {

gfx::ImageSkia CreateNotificationAppIcon(
    const message_center::Notification* notification) {
  SkColor icon_color = GetColorProviderForNativeTheme()->GetColor(
      cros_tokens::kCrosSysOnPrimary);
  SkColor icon_background_color = CalculateIconBackgroundColor(notification);

  // TODO(crbug.com/40541732): figure out if this has a performance impact and
  // cache images if so.
  gfx::Image masked_small_icon = notification->GenerateMaskedSmallIcon(
      kNotificationAppIconImageSize, icon_color, icon_background_color,
      icon_color);

  gfx::ImageSkia app_icon =
      masked_small_icon.IsEmpty()
          ? gfx::CreateVectorIcon(message_center::kProductIcon,
                                  kNotificationAppIconImageSize, icon_color)
          : masked_small_icon.AsImageSkia();

  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      kNotificationAppIconViewSize / 2, icon_background_color, app_icon);
}

gfx::ImageSkia CreateNotificationItemIcon(
    const message_center::NotificationItem* item) {
  if (item->icon().has_value()) {
    gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
        item->icon().value().GetImage().AsImageSkia(),
        skia::ImageOperations::ResizeMethod::RESIZE_BEST,
        gfx::Size(kNotificationAppIconViewSize, kNotificationAppIconViewSize));

    return resized;
  }
  // TODO(b/284512022): Remove the temporary implementation returning a
  // hardcoded chrome icon as a default icon.
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      kNotificationAppIconViewSize / 2, SK_ColorRED,
      gfx::CreateVectorIcon(message_center::kProductIcon,
                            kNotificationAppIconImageSize, SK_ColorBLACK));
}

SkColor CalculateIconBackgroundColor(
    const message_center::Notification* notification) {
  SkColor default_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);

  if (!notification) {
    return default_color;
  }

  auto color_id = notification->accent_color_id();
  std::optional<SkColor> accent_color = notification->accent_color();

  if (!color_id || !accent_color.has_value()) {
    return default_color;
  }

  SkColor fg_color;
  // ColorProvider needs widget to be created.
  if (color_id) {
    fg_color = GetColorProviderForNativeTheme()->GetColor(color_id.value());
  } else {
    fg_color = accent_color.value();
  }

  // TODO(crbug/1351205): move color calculation logic to color mixer.
  // TODO(crbug/1294459): re-evaluate contrast, maybe increase or use fixed HSL
  float minContrastRatio =
      DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
          ? minContrastRatio = kDarkModeMinContrastRatio
          : color_utils::kMinimumReadableContrastRatio;

  // Actual color is kTransparent80, but BlendForMinContrast requires opaque.
  // GetColorProvider might be nullptr in tests.
  const auto* color_provider = GetColorProviderForNativeTheme();
  const SkColor bg_color =
      color_provider ? color_provider->GetColor(kColorAshShieldAndBaseOpaque)
                     : gfx::kPlaceholderColor;
  return color_utils::BlendForMinContrast(
             fg_color, bg_color,
             /*high_contrast_foreground=*/std::nullopt, minContrastRatio)
      .color;
}

void ConfigureLabelStyle(views::Label* label,
                         int size,
                         bool is_color_primary,
                         gfx::Font::Weight font_weight) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(
      gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, size, font_weight));
  auto layer_type =
      is_color_primary
          ? ash::AshColorProvider::ContentLayerType::kTextColorPrimary
          : ash::AshColorProvider::ContentLayerType::kTextColorSecondary;
  label->SetEnabledColor(
      ash::AshColorProvider::Get()->GetContentLayerColor(layer_type));
}

ui::ColorProvider* GetColorProviderForNativeTheme() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
}

std::unique_ptr<views::Background> CreateNotificationBackground(
    int top_radius,
    int bottom_radius,
    bool is_popup_notification,
    bool is_grouped_child_notification) {
  ui::ColorId background_color_id =
      is_popup_notification ? static_cast<ui::ColorId>(kColorAshShieldAndBase80)
                            : cros_tokens::kCrosSysSystemOnBase;

  const gfx::RoundedCornersF background_radii(top_radius, top_radius,
                                              bottom_radius, bottom_radius);
  if (is_grouped_child_notification) {
    // Grouped children are always transparent. Handle them separately.
    return views::CreateRoundedRectBackground(SK_ColorTRANSPARENT,
                                              background_radii);
  }

  return views::CreateThemedRoundedRectBackground(background_color_id,
                                                  background_radii);
}

void StyleNotificationPopup(message_center::MessageView* notification_view) {
  notification_view->SetPaintToLayer();
  auto* layer = notification_view->layer();
  layer->SetFillsBoundsOpaquely(false);
  layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kMessagePopupCornerRadius});
  layer->SetIsFastRoundedCorner(true);

  notification_view->SetBackground(CreateNotificationBackground(
      kMessagePopupCornerRadius, kMessagePopupCornerRadius,
      /*is_popup_notification=*/true, /*is_grouped_child_notification=*/false));
  notification_view->SetBorder(std::make_unique<views::HighlightBorder>(
      kMessagePopupCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  notification_view->UpdateCornerRadius(kMessagePopupCornerRadius,
                                        kMessagePopupCornerRadius);
}

std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
    views::Button::PressedCallback callback,
    const std::u16string& label) {
  std::unique_ptr<PillButton> actions_button = std::make_unique<PillButton>(
      std::move(callback), label, PillButton::Type::kFloatingWithoutIcon,
      /*icon=*/nullptr, kNotificationPillButtonHorizontalSpacing);
  actions_button->SetButtonTextColorId(cros_tokens::kCrosSysOnSurface);

  return actions_button;
}

std::unique_ptr<views::FlexLayoutView> CreateInlineSettingsViewForMessageView(
    message_center::MessageView* message_view) {
  auto inline_settings_view = std::make_unique<views::FlexLayoutView>();
  inline_settings_view->SetOrientation(views::LayoutOrientation::kHorizontal);

  // base::Unretained(message_view) is safe here because `inline_settings_view`
  // and it's children must be owned by the provided `message_view`
  auto turn_off_notifications_button = GenerateNotificationLabelButton(
      base::BindRepeating(&message_center::MessageView::DisableNotification,
                          base::Unretained(message_view)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_INLINE_SETTINGS_TURN_OFF_BUTTON_TEXT));
  turn_off_notifications_button->SetID(kNotificationTurnOffNotificationsButton);
  inline_settings_view->AddChildView(std::move(turn_off_notifications_button));
  auto cancel_button =
      notification_style_utils::GenerateNotificationLabelButton(
          base::BindRepeating(
              &message_center::MessageView::ToggleInlineSettings,
              base::Unretained(message_view)),
          l10n_util::GetStringUTF16(
              IDS_ASH_NOTIFICATION_INLINE_SETTINGS_CANCEL_BUTTON_TEXT));
  cancel_button->SetID(kNotificationInlineSettingsCancelButton);
  inline_settings_view->AddChildView(std::move(cancel_button));
  return inline_settings_view;
}

}  // namespace ash::notification_style_utils
