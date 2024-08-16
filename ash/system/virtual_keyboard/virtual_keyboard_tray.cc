// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

class TrayBubbleView;

VirtualKeyboardTray::VirtualKeyboardTray(
    Shelf* shelf,
    TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf, catalog_name), shelf_(shelf) {
  SetCallback(base::BindRepeating(&VirtualKeyboardTray::OnButtonPressed,
                                  base::Unretained(this)));

  auto icon = std::make_unique<views::ImageView>();
  const ui::ImageModel image = ui::ImageModel::FromVectorIcon(
      kShelfKeyboardNewuiIcon, kColorAshIconColorPrimary);
  icon->SetImage(image);
  icon->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
  const int vertical_padding = (kTrayItemSize - image.Size().height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.Size().width()) / 2;
  icon->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  icon_ = tray_container()->AddChildView(std::move(icon));
  // First sets the image with non-Jelly color to get the image dimension and
  // create the correct paddings, and then updates the color if Jelly is
  // enabled.
  UpdateTrayItemColor(is_active());

  // The Shell may not exist in some unit tests.
  if (Shell::HasInstance()) {
    Shell::Get()->accessibility_controller()->AddObserver(this);
    Shell::Get()->AddShellObserver(this);
    keyboard::KeyboardUIController::Get()->AddObserver(this);
  }
}

VirtualKeyboardTray::~VirtualKeyboardTray() {
  // The Shell may not exist in some unit tests.
  if (!Shell::HasInstance())
    return;

  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void VirtualKeyboardTray::OnButtonPressed(const ui::Event& event) {
  UserMetricsRecorder::RecordUserClickOnTray(
      LoginMetricsRecorder::TrayClickTarget::kVirtualKeyboardTray);

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();

  // Keyboard may not always be enabled. https://crbug.com/749989
  if (!keyboard_controller->IsEnabled())
    return;

  // Normally, active status is set when virtual keyboard is shown/hidden,
  // however, showing virtual keyboard happens asynchronously and, especially
  // the first time, takes some time. We need to set active status here to
  // prevent bad things happening if user clicked the button before keyboard is
  // shown.
  if (is_active()) {
    keyboard_controller->HideKeyboardByUser();
    SetIsActive(false);
    return;
  }
    keyboard_controller->ShowKeyboardInDisplay(
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            shelf_->GetWindow()));
    SetIsActive(true);
    return;
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

void VirtualKeyboardTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {}

void VirtualKeyboardTray::UpdateTrayItemColor(bool is_active) {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kShelfKeyboardNewuiIcon,
      is_active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                : cros_tokens::kCrosSysOnSurface));
}

void VirtualKeyboardTray::HideBubble(const TrayBubbleView* bubble_view) {}

void VirtualKeyboardTray::OnAccessibilityStatusChanged() {
  bool new_enabled =
      Shell::Get()->accessibility_controller()->virtual_keyboard().enabled();
  SetVisiblePreferred(new_enabled);
}

void VirtualKeyboardTray::OnKeyboardVisibilityChanged(const bool is_visible) {
  SetIsActive(is_visible);
}

BEGIN_METADATA(VirtualKeyboardTray);
END_METADATA

}  // namespace ash
