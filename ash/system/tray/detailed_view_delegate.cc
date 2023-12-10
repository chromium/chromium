// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/detailed_view_delegate.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

// The scroll view's top is flush against the header.
constexpr auto kQsScrollViewMargin = gfx::Insets::TLBR(0, 16, 16, 16);

}  // namespace

DetailedViewDelegate::DetailedViewDelegate(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

DetailedViewDelegate::~DetailedViewDelegate() = default;

void DetailedViewDelegate::TransitionToMainView(bool restore_focus) {
  if (!tray_controller_) {
    return;
  }
  tray_controller_->TransitionToMainView(restore_focus);
}

void DetailedViewDelegate::CloseBubble() {
  if (!tray_controller_) {
    return;
  }
  tray_controller_->CloseBubble();
}

gfx::Insets DetailedViewDelegate::GetScrollViewMargin() const {
  return kQsScrollViewMargin;
}

// TODO(b/253091169): Refactor the following creating buttons methods to return
// unique pointers.
views::Button* DetailedViewDelegate::CreateBackButton(
    views::Button::PressedCallback callback) {
  return new IconButton(std::move(callback), IconButton::Type::kMedium,
                        &kQuickSettingsLeftArrowIcon,
                        IDS_ASH_STATUS_TRAY_PREVIOUS_MENU);
}

views::Button* DetailedViewDelegate::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  return new IconButton(std::move(callback), IconButton::Type::kMedium,
                        &kUnifiedMenuInfoIcon, info_accessible_name_id);
}

views::Button* DetailedViewDelegate::CreateSettingsButton(
    views::Button::PressedCallback callback,
    int setting_accessible_name_id) {
  auto* button = new IconButton(std::move(callback), IconButton::Type::kMedium,
                                &vector_icons::kSettingsOutlineIcon,
                                setting_accessible_name_id);
  if (!TrayPopupUtils::CanOpenWebUISettings()) {
    button->SetEnabled(false);
  }
  return button;
}

views::Button* DetailedViewDelegate::CreateHelpButton(
    views::Button::PressedCallback callback) {
  auto* button =
      new IconButton(std::move(callback), IconButton::Type::kMedium,
                     &vector_icons::kHelpOutlineIcon, IDS_ASH_STATUS_TRAY_HELP);
  // Help opens a web page, so treat it like Web UI settings.
  if (!TrayPopupUtils::CanOpenWebUISettings()) {
    button->SetEnabled(false);
  }
  return button;
}

}  // namespace ash
