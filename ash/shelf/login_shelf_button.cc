// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_button.h"

#include <memory>
#include <string>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "base/functional/callback.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

constexpr char kLoginShelfButtonClassName[] = "LoginShelfButton";

// The highlight radius of the button.
constexpr int kButtonHighlightRadiusDp = 16;

}  // namespace

LoginShelfButton::LoginShelfButton(PressedCallback callback,
                                   int text_resource_id,
                                   const gfx::VectorIcon& icon)
    : PillButton(std::move(callback),
                 l10n_util::GetStringUTF16(text_resource_id),
                 PillButton::Type::kDefaultLargeWithIconLeading,
                 &icon,
                 PillButton::kPillButtonHorizontalSpacing),
      icon_(icon),
      text_resource_id_(text_resource_id) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  SetFocusPainter(nullptr);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  UpdateColors(ShelfBackgroundType::kDefaultBg);
}

LoginShelfButton::~LoginShelfButton() = default;

int LoginShelfButton::text_resource_id() const {
  return text_resource_id_;
}

const char* LoginShelfButton::GetClassName() const {
  return kLoginShelfButtonClassName;
}

std::u16string LoginShelfButton::GetTooltipText(const gfx::Point& p) const {
  if (label()->IsDisplayTextTruncated()) {
    return label()->GetText();
  }
  return std::u16string();
}

void LoginShelfButton::OnFocus() {
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->set_keyboard_locked(false /*lock*/);
  keyboard_controller->HideKeyboardImplicitlyByUser();
}

void LoginShelfButton::AddedToWidget() {
  PillButton::AddedToWidget();
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  shelf_observer_.Observe(shelf);
}

void LoginShelfButton::OnBackgroundTypeChanged(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  UpdateColors(background_type);
}

void LoginShelfButton::UpdateColors(ShelfBackgroundType background_type) {
  ui::ColorId icon_color = kColorAshButtonIconColor;
  if (!chromeos::features::IsJellyrollEnabled()) {
    ui::ColorId text_color = kColorAshButtonLabelColor;
    if (background_type == ShelfBackgroundType::kOobe) {
      text_color = kColorAshButtonLabelColorLight;
      icon_color = kColorAshButtonIconColorLight;
    }
    SetEnabledTextColorIds(text_color);
  } else {
    if (background_type == ShelfBackgroundType::kLoginNonBlurredWallpaper) {
      SetPillButtonType(PillButton::kDefaultLargeWithIconLeading);
    } else {
      SetPillButtonType(PillButton::kDefaultElevatedLargeWithIconLeading);
    }
    SetBorder(std::make_unique<views::HighlightBorder>(
        kButtonHighlightRadiusDp,
        views::HighlightBorder::Type::kHighlightBorderNoShadow));
    icon_color = cros_tokens::kCrosSysOnSurface;
  }
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon_, icon_color));
}

BEGIN_METADATA(LoginShelfButton)
END_METADATA

}  // namespace ash
