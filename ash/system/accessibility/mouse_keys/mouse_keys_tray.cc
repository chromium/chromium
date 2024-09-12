// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Arbitrary ID for the icon.
const int kMouseKeysTrayIconID = 11;

ui::ImageModel GetMouseKeysIcon() {
  return ui::ImageModel::FromVectorIcon(
      kSystemTrayMouseKeysIcon,
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface));
}

}  // namespace

MouseKeysTray::MouseKeysTray(Shelf* shelf,
                             TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf, catalog_name) {
  const ui::ImageModel image = GetMouseKeysIcon();
  const int vertical_padding = (kTrayItemSize - image.Size().height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.Size().width()) / 2;
  tray_container()->AddChildView(
      views::Builder<views::ImageView>()
          .SetID(kMouseKeysTrayIconID)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS))
          .SetImage(image)
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets::VH(vertical_padding, horizontal_padding)))
          .Build());

  // Observe the accessibility controller state changes to know when mouse keys
  // state is updated or when it is disabled/enabled.
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

MouseKeysTray::~MouseKeysTray() {
  // This may be called during shutdown in which case some of the
  // ash objects may already be destroyed.
  auto* shell = Shell::Get();
  if (!shell) {
    return;
  }
  auto* accessibility_controller = shell->accessibility_controller();
  if (accessibility_controller) {
    accessibility_controller->RemoveObserver(this);
  }
}

void MouseKeysTray::Initialize() {
  TrayBackgroundView::Initialize();
  OnAccessibilityStatusChanged();
  HandleLocaleChange();
}

std::u16string MouseKeysTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_MOUSE_KEYS_TRAY_ACCESSIBLE_NAME);
}

void MouseKeysTray::HandleLocaleChange() {
  GetIcon()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS));
}

void MouseKeysTray::UpdateTrayItemColor(bool is_active) {
  SetIsActive(is_active);
}

void MouseKeysTray::OnAccessibilityStatusChanged() {
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  SetVisiblePreferred(::features::IsAccessibilityMouseKeysEnabled() &&
                      accessibility_controller->mouse_keys().enabled());
}

void MouseKeysTray::OnSessionStateChanged(session_manager::SessionState state) {
  GetIcon()->SetImage(GetMouseKeysIcon());
}

views::ImageView* MouseKeysTray::GetIcon() {
  return static_cast<views::ImageView*>(
      tray_container()->GetViewByID(kMouseKeysTrayIconID));
}

BEGIN_METADATA(MouseKeysTray);
END_METADATA

}  // namespace ash
