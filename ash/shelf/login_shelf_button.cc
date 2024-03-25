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

// The highlight radius of the button.
// The large pill buttons height is 36 and the radius should be half of that.
constexpr int kButtonHighlightRadiusDp = 18;
constexpr int kButtonHighlightWidthDp = 1;

}  // namespace

LoginShelfButton::LoginShelfButton(PressedCallback callback,
                                   int text_resource_id,
                                   const gfx::VectorIcon& icon)
    : PillButton(std::move(callback),
                 l10n_util::GetStringUTF16(text_resource_id),
                 PillButton::Type::kDefaultElevatedLargeWithIconLeading,
                 &icon,
                 PillButton::kPillButtonHorizontalSpacing),
      icon_(icon),
      text_resource_id_(text_resource_id) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  SetFocusPainter(nullptr);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  SetBorder(views::CreateThemedRoundedRectBorder(
      kButtonHighlightWidthDp, kButtonHighlightRadiusDp,
      ui::kColorCrosSystemHighlight));
}

LoginShelfButton::~LoginShelfButton() = default;

int LoginShelfButton::text_resource_id() const {
  return text_resource_id_;
}

std::u16string LoginShelfButton::GetTooltipText(const gfx::Point& p) const {
  if (label()->IsDisplayTextTruncated()) {
    return GetText();
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
  if (background_type_ == background_type) {
    return;
  }
  background_type_ = background_type;

  if (background_type_ == ShelfBackgroundType::kLoginNonBlurredWallpaper) {
    SetPillButtonType(PillButton::kDefaultLargeWithIconLeading);
  } else {
    SetPillButtonType(PillButton::kDefaultElevatedLargeWithIconLeading);
  }
}

void LoginShelfButton::OnActiveChanged() {
  if (is_active_) {
    SetBackgroundColorId(cros_tokens::kCrosSysSystemPrimaryContainer);
    SetEnabledTextColorIds(cros_tokens::kCrosSysSystemOnPrimaryContainer);
    SetIconColorId(cros_tokens::kCrosSysSystemOnPrimaryContainer);
  } else {
    // Switch the pillbutton to the default background color.
    SetBackgroundColor(gfx::kPlaceholderColor);
    SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurface);
    SetIconColor(gfx::kPlaceholderColor);
  }
}

void LoginShelfButton::SetIsActive(bool is_active) {
  if (is_active_ == is_active) {
    return;
  }
  is_active_ = is_active;
  OnActiveChanged();
}

bool LoginShelfButton::GetIsActive() const {
  return is_active_;
}

BEGIN_METADATA(LoginShelfButton)
END_METADATA

}  // namespace ash
