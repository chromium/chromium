// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/arc_compat_mode_util.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace ash::compat_mode_util {

ResizeCompatMode PredictCurrentMode(const views::Widget* widget) {
  return PredictCurrentMode(widget->GetNativeWindow());
}

ResizeCompatMode PredictCurrentMode(const aura::Window* window) {
  const auto resize_lock_type = window->GetProperty(kArcResizeLockTypeKey);
  if (resize_lock_type == ArcResizeLockType::NONE ||
      resize_lock_type == ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE) {
    return ResizeCompatMode::kResizable;
  }

  const auto& bounds = window->bounds();
  // We don't use the exact size here to predict tablet or phone size because
  // the window size might be bigger than it due to the ARC app-side minimum
  // size constraints.
  if (bounds.width() <= bounds.height()) {
    return ResizeCompatMode::kPhone;
  }

  return ResizeCompatMode::kTablet;
}

const gfx::VectorIcon& GetIcon(ResizeCompatMode mode) {
  switch (mode) {
    case ResizeCompatMode::kPhone:
      return chromeos::features::IsJellyEnabled() ? kSystemMenuPhoneIcon
                                                  : kSystemMenuPhoneLegacyIcon;
    case ResizeCompatMode::kTablet:
      return chromeos::features::IsJellyEnabled() ? kSystemMenuTabletIcon
                                                  : kSystemMenuTabletLegacyIcon;
    case ResizeCompatMode::kResizable:
      return kAppCompatResizableIcon;
  }
}

std::u16string GetText(ResizeCompatMode mode) {
  switch (mode) {
    case ResizeCompatMode::kPhone:
      return l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PORTRAIT);
    case ResizeCompatMode::kTablet:
      return l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_LANDSCAPE);
    case ResizeCompatMode::kResizable:
      return l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE);
  }
}

}  // namespace ash::compat_mode_util
