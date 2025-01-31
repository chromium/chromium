// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
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
#include "ui/views/accessibility/view_accessibility.h"
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
  SetCallback(
      base::BindRepeating(&MouseKeysTray::OnMouseKeyIconPressed, GetWeakPtr()));
  const ui::ImageModel image = GetMouseKeysIcon();
  const int vertical_padding = (kTrayItemSize - image.Size().height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.Size().width()) / 2;

  tray_container()->AddChildView(
      views::Builder<views::ImageView>()
          .SetID(kMouseKeysTrayIconID)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_PAUSE))
          .SetImage(image)
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets::VH(vertical_padding, horizontal_padding)))
          .Build());

  // Observe the accessibility controller state changes to know when mouse keys
  // state is updated or when it is disabled/enabled.
  Shell::Get()->accessibility_controller()->AddObserver(this);

  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_PAUSE));
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

void MouseKeysTray::OnMouseKeyIconPressed(const ui::Event& event) {
  Shell::Get()->accessibility_controller()->ToggleMouseKeys();
}

void MouseKeysTray::Initialize() {
  TrayBackgroundView::Initialize();
  OnAccessibilityStatusChanged();
  HandleLocaleChange();
}

void MouseKeysTray::HandleLocaleChange() {
  UpdateStatus();
}

void MouseKeysTray::UpdateTrayItemColor(bool is_active) {
  SetIsActive(is_active);
}

void MouseKeysTray::OnAccessibilityStatusChanged() {
  UpdateStatus();
}

void MouseKeysTray::UpdateStatus() {
  auto* mouse_keys_controller = Shell::Get()->mouse_keys_controller();

  // Early exit if mouse_keys_controller is not available
  if (!mouse_keys_controller) {
    return;
  }

  bool is_mouse_keys_enabled = ::features::IsAccessibilityMouseKeysEnabled() &&
                               mouse_keys_controller->enabled();

  SetVisiblePreferred(is_mouse_keys_enabled);

  bool is_mouse_keys_active =
      is_mouse_keys_enabled && !mouse_keys_controller->paused();
  UpdateTrayItemColor(is_mouse_keys_active);
  SetMouseKeysStatusText(is_mouse_keys_active);
}

void MouseKeysTray::SetMouseKeysStatusText(bool is_active) {
  auto tooltip_string =
      is_active ? l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_PAUSE)
                : l10n_util::GetStringUTF16(
                      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_MOUSE_KEYS_RESUME);

  GetViewAccessibility().SetName(tooltip_string);
  GetIcon()->SetTooltipText(tooltip_string);
}

void MouseKeysTray::OnSessionStateChanged(session_manager::SessionState state) {
  GetIcon()->SetImage(GetMouseKeysIcon());
}

base::WeakPtr<MouseKeysTray> MouseKeysTray::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

views::ImageView* MouseKeysTray::GetIcon() {
  return static_cast<views::ImageView*>(
      tray_container()->GetViewByID(kMouseKeysTrayIconID));
}

BEGIN_METADATA(MouseKeysTray);
END_METADATA

}  // namespace ash
