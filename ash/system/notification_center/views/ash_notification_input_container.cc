// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/ash_notification_input_container.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

// Padding between children, currently only between the textfield and the
// ImageButton.
constexpr int kBetweenChildSpacing = 12;

// Insets for inside the border.
constexpr auto kInsideBorderInsets = gfx::Insets::TLBR(6, 16, 14, 6);

// The icon size of inline reply input field.
constexpr int kInputReplyButtonSize = 20;
// Padding on the input reply button.
constexpr auto kInputReplyButtonPadding = gfx::Insets::TLBR(0, 6, 0, 6);
// Radius of the circular input reply button highlight.
constexpr int kInputReplyHighlightRadius =
    (kInputReplyButtonPadding.width() + kInputReplyButtonSize) / 2;

// Padding of the textfield, inside the rounded background.
constexpr auto kInputTextfieldPaddingCrOS = gfx::Insets::TLBR(6, 12, 6, 12);
// Corner radius of the grey background of the textfield.
constexpr int kTextfieldBackgroundCornerRadius = 24;

}  // namespace

AshNotificationInputContainer::AshNotificationInputContainer(
    message_center::NotificationInputDelegate* delegate)
    : message_center::NotificationInputContainer(delegate) {}

AshNotificationInputContainer::~AshNotificationInputContainer() {}

views::BoxLayout* AshNotificationInputContainer::InstallLayoutManager() {
  return SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets,
      kBetweenChildSpacing));
}

views::InkDropContainerView* AshNotificationInputContainer::InstallInkDrop() {
  // Do not install an inkdrop.
  return nullptr;
}

gfx::Insets AshNotificationInputContainer::GetTextfieldPadding() const {
  return kInputTextfieldPaddingCrOS;
}

int AshNotificationInputContainer::GetDefaultPlaceholderStringId() const {
  return IDS_ASH_NOTIFICATION_INLINE_REPLY_PLACEHOLDER;
}

void AshNotificationInputContainer::StyleTextfield() {
  views::FocusRing::Install(textfield());
  views::InstallRoundRectHighlightPathGenerator(
      textfield(), gfx::Insets(), kTextfieldBackgroundCornerRadius);
  views::FocusRing::Get(textfield())->SetColorId(ui::kColorAshFocusRing);
}

gfx::Insets AshNotificationInputContainer::GetSendButtonPadding() const {
  return kInputReplyButtonPadding;
}

void AshNotificationInputContainer::SetSendButtonHighlightPath() {
  views::FocusRing::Install(textfield());
  views::InstallRoundRectHighlightPathGenerator(button(), gfx::Insets(),
                                                kInputReplyHighlightRadius);
  views::FocusRing::Get(button())->SetColorId(ui::kColorAshFocusRing);
}

void AshNotificationInputContainer::UpdateButtonImage() {
  if (!GetWidget())
    return;
  UpdateButtonState();
  button()->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kSendIcon,
                                     cros_tokens::kColorProminent,
                                     kInputReplyButtonSize));
  button()->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(vector_icons::kSendIcon,
                                     cros_tokens::kColorDisabled,
                                     kInputReplyButtonSize));
}

void AshNotificationInputContainer::UpdateButtonState() {
  button()->SetEnabled(!IsInputEmpty());
}

bool AshNotificationInputContainer::IsInputEmpty() {
  return textfield()->GetText().empty();
}

void AshNotificationInputContainer::OnThemeChanged() {
  message_center::NotificationInputContainer::OnThemeChanged();
  UpdateButtonImage();
  SetSendButtonHighlightPath();
  StyleTextfield();

  if (chromeos::features::IsJellyEnabled()) {
    textfield()->SetTextColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface));
    textfield()->SetFontList(
        ash::TypographyProvider::Get()->ResolveTypographyToken(
            ash::TypographyToken::kCrosBody2));
    textfield()->set_placeholder_text_color(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurfaceVariant));
    textfield()->set_placeholder_font_list(
        ash::TypographyProvider::Get()->ResolveTypographyToken(
            ash::TypographyToken::kCrosBody2));

    textfield()->SetBackground(views::CreateRoundedRectBackground(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSurface),
        kTextfieldBackgroundCornerRadius));
  }
}

}  // namespace ash
