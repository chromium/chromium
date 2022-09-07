// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

gfx::ImageSkia GetIconImage() {
  return gfx::CreateVectorIcon(
      kShelfKeyboardNewuiIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
}

}  // namespace

VirtualKeyboardTray::VirtualKeyboardTray(Shelf* shelf)
    : TrayBackgroundView(shelf), icon_(new views::ImageView), shelf_(shelf) {
  const gfx::ImageSkia image = GetIconImage();
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  tray_container()->AddChildView(icon_);

  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    Shell::Get()->accessibility_controller()->AddObserver(this);
    Shell::Get()->AddShellObserver(this);
    keyboard::KeyboardUIController::Get()->AddObserver(this);
  }
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    keyboard::KeyboardUIController::Get()->RemoveObserver(this);
    Shell::Get()->RemoveShellObserver(this);
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
  }
}

void VirtualKeyboardTray::Initialize() {
  TrayBackgroundView::Initialize();
  SetVisiblePreferred(
      Shell::Get()->accessibility_controller()->virtual_keyboard().enabled());
}

std::u16string VirtualKeyboardTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_VIRTUAL_KEYBOARD_TRAY_ACCESSIBLE_NAME);
}

void VirtualKeyboardTray::HandleLocaleChange() {
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
}

void VirtualKeyboardTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {}

void VirtualKeyboardTray::ClickedOutsideBubble() {}

bool VirtualKeyboardTray::PerformAction(const ui::Event& event) {
  UserMetricsRecorder::RecordUserClickOnTray(
      LoginMetricsRecorder::TrayClickTarget::kVirtualKeyboardTray);

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();

  // Keyboard may not always be enabled. https://crbug.com/749989
  if (!keyboard_controller->IsEnabled())
    return true;

  // Normally, active status is set when virtual keyboard is shown/hidden,
  // however, showing virtual keyboard happens asynchronously and, especially
  // the first time, takes some time. We need to set active status here to
  // prevent bad things happening if user clicked the button before keyboard is
  // shown.
  if (is_active()) {
    keyboard_controller->HideKeyboardByUser();
    SetIsActive(false);
  } else {
    keyboard_controller->ShowKeyboardInDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            shelf_->GetWindow()));
    SetIsActive(true);
  }

  return true;
}

void VirtualKeyboardTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  icon_->SetImage(GetIconImage());
}

void VirtualKeyboardTray::OnAccessibilityStatusChanged() {
  bool new_enabled =
      Shell::Get()->accessibility_controller()->virtual_keyboard().enabled();
  SetVisiblePreferred(new_enabled);
}

void VirtualKeyboardTray::OnKeyboardVisibilityChanged(const bool is_visible) {
  SetIsActive(is_visible);
}

void VirtualKeyboardTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  icon_->SetImage(GetIconImage());
}

const char* VirtualKeyboardTray::GetClassName() const {
  return "VirtualKeyboardTray";
}

}  // namespace ash
