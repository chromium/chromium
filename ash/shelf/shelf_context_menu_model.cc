// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_context_menu_model.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/personalization_entry_point.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/safe_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/image_model.h"

namespace ash {

namespace {

// Returns true if the user can modify the shelf's auto-hide behavior pref.
bool CanUserModifyShelfAutoHide(PrefService* prefs) {
  return prefs && prefs->FindPreference(prefs::kShelfAutoHideBehaviorLocal)
                      ->IsUserModifiable();
}

// Returns true if the display is showing a fullscreen window.
// NOTE: This duplicates the functionality of Chrome's IsFullScreenMode.
bool IsFullScreenMode(int64_t display_id) {
  auto* controller = Shell::GetRootWindowControllerWithDisplayId(display_id);
  return controller && controller->GetWindowForFullscreenMode();
}

}  // namespace

ShelfContextMenuModel::ShelfContextMenuModel(ShelfItemDelegate* delegate,
                                             int64_t display_id,
                                             bool menu_in_shelf)
    : ui::SimpleMenuModel(this),
      delegate_(delegate),
      display_id_(display_id),
      menu_in_shelf_(menu_in_shelf) {
  // Add shelf and wallpaper items if ShelfView or HomeButton are selected.
  if (!delegate)
    AddShelfAndWallpaperItems();
}

ShelfContextMenuModel::~ShelfContextMenuModel() = default;

bool ShelfContextMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == MENU_ALIGNMENT_LEFT ||
      command_id == MENU_ALIGNMENT_BOTTOM ||
      command_id == MENU_ALIGNMENT_RIGHT) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    const ShelfAlignment alignment = GetShelfAlignmentPref(prefs, display_id_);
    if (command_id == MENU_ALIGNMENT_LEFT)
      return alignment == ShelfAlignment::kLeft;
    if (command_id == MENU_ALIGNMENT_BOTTOM) {
      return alignment == ShelfAlignment::kBottom ||
             alignment == ShelfAlignment::kBottomLocked;
    }
    if (command_id == MENU_ALIGNMENT_RIGHT)
      return alignment == ShelfAlignment::kRight;
  }

  return SimpleMenuModel::Delegate::IsCommandIdChecked(command_id);
}

void ShelfContextMenuModel::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  Shell* shell = Shell::Get();
  PrefService* prefs =
      shell->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  // Clamshell mode only options should not activate in tablet mode.
  const bool is_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  switch (command_id) {
    case MENU_AUTO_HIDE:
      SetShelfAutoHideBehaviorPref(
          prefs, display_id_,
          GetShelfAutoHideBehaviorPref(prefs, display_id_) ==
                  ShelfAutoHideBehavior::kAlways
              ? ShelfAutoHideBehavior::kNever
              : ShelfAutoHideBehavior::kAlways);
      break;
    case MENU_ALIGNMENT_LEFT:
      DCHECK(!is_tablet_mode);
      base::RecordAction(base::UserMetricsAction("Shelf_AlignmentSetLeft"));
      SetShelfAlignmentPref(prefs, display_id_, ShelfAlignment::kLeft);
      break;
    case MENU_ALIGNMENT_RIGHT:
      DCHECK(!is_tablet_mode);
      base::RecordAction(base::UserMetricsAction("Shelf_AlignmentSetRight"));
      SetShelfAlignmentPref(prefs, display_id_, ShelfAlignment::kRight);
      break;
    case MENU_ALIGNMENT_BOTTOM:
      DCHECK(!is_tablet_mode);
      base::RecordAction(base::UserMetricsAction("Shelf_AlignmentSetBottom"));
      SetShelfAlignmentPref(prefs, display_id_, ShelfAlignment::kBottom);
      break;
    case MENU_PERSONALIZATION_HUB:
      // Record entry point metric to Personalization Hub through Home Screen.
      base::UmaHistogramEnumeration(kPersonalizationEntryPointHistogramName,
                                    PersonalizationEntryPoint::kHomeScreen);
      NewWindowDelegate::GetPrimary()->OpenPersonalizationHub();
      break;
    case MENU_HIDE_CONTINUE_SECTION:
      DCHECK(is_tablet_mode);
      shell->app_list_controller()->SetHideContinueSection(true);
      break;
    case MENU_SHOW_CONTINUE_SECTION:
      DCHECK(is_tablet_mode);
      shell->app_list_controller()->SetHideContinueSection(false);
      break;
    case MENU_HIDE_DESK_NAME:
      base::UmaHistogramBoolean(kDeskButtonHiddenHistogramName, true);
      SetShowDeskButtonInShelfPref(prefs, false);
      break;
    case MENU_SHOW_DESK_NAME:
      SetShowDeskButtonInShelfPref(prefs, true);
      break;
    // Using reorder CommandId in ash/public/cpp/app_menu_constants.h
    case REORDER_BY_NAME_ALPHABETICAL:
      AppListModelProvider::Get()->model()->delegate()->RequestAppListSort(
          AppListSortOrder::kNameAlphabetical);
      break;
    case REORDER_BY_COLOR:
      AppListModelProvider::Get()->model()->delegate()->RequestAppListSort(
          AppListSortOrder::kColor);
      break;
    default:
      if (delegate_) {
        if (IsCommandIdAnAppLaunch(command_id)) {
          shell->app_list_controller()->RecordShelfAppLaunched();
        }

        delegate_->ExecuteCommand(true, command_id, event_flags, display_id_);
      }
      break;
  }
}

void ShelfContextMenuModel::AddShelfAndWallpaperItems() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)  // Null during startup.
    return;

  // In fullscreen, the shelf is either hidden or auto-hidden, depending on the
  // type of fullscreen. Do not show the auto-hide menu item while in fullscreen
  // because it is confusing when the preference appears not to apply.
  const bool is_fullscreen = IsFullScreenMode(display_id_);
  if (CanUserModifyShelfAutoHide(prefs) && !is_fullscreen) {
    const bool is_autohide_set =
        GetShelfAutoHideBehaviorPref(prefs, display_id_) ==
        ShelfAutoHideBehavior::kAlways;
    auto string_id = is_autohide_set
                         ? IDS_ASH_SHELF_CONTEXT_MENU_ALWAYS_SHOW_SHELF
                         : IDS_ASH_SHELF_CONTEXT_MENU_AUTO_HIDE;
    AddItemWithStringIdAndIcon(
        MENU_AUTO_HIDE, string_id,
        ui::ImageModel::FromVectorIcon(
            is_autohide_set ? kAlwaysShowShelfIcon : kAutoHideIcon,
            ui::kColorAshSystemUIMenuIcon));
  }

  // Only allow shelf alignment modifications by the logged in Gaia users
  // (regular or Family Link user). In tablet mode, the shelf alignment option
  // is not shown.
  LoginStatus status = Shell::Get()->session_controller()->login_status();
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  if ((status == LoginStatus::USER || status == LoginStatus::CHILD) &&
      !in_tablet_mode &&
      prefs->FindPreference(prefs::kShelfAlignmentLocal)->IsUserModifiable()) {
    alignment_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);

    constexpr int group = 0;
    alignment_submenu_->AddRadioItemWithStringId(
        MENU_ALIGNMENT_LEFT, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_LEFT, group);
    alignment_submenu_->AddRadioItemWithStringId(
        MENU_ALIGNMENT_BOTTOM, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_BOTTOM, group);
    alignment_submenu_->AddRadioItemWithStringId(
        MENU_ALIGNMENT_RIGHT, IDS_ASH_SHELF_CONTEXT_MENU_ALIGN_RIGHT, group);

    AddSubMenuWithStringIdAndIcon(
        MENU_ALIGNMENT_MENU, IDS_ASH_SHELF_CONTEXT_MENU_POSITION,
        alignment_submenu_.get(),
        ui::ImageModel::FromVectorIcon(kShelfPositionIcon,
                                       ui::kColorAshSystemUIMenuIcon));
  }

  AddItemWithStringIdAndIcon(
      MENU_PERSONALIZATION_HUB, IDS_AURA_OPEN_PERSONALIZATION_HUB,
      ui::ImageModel::FromVectorIcon(kPaintBrushIcon,
                                     ui::kColorAshSystemUIMenuIcon));

  // Only add the desk button items if the context menu was spawned on the
  // shelf, tablet mode is not enabled, and full screen is not enabled.
  if (features::IsDeskButtonEnabled() && !in_tablet_mode && menu_in_shelf_ &&
      !is_fullscreen) {
    // If the button is visible for any reason, show the option to hide it
    // manually. If it isn't visible show the option to show it.
    if (GetDeskButtonVisibility(prefs)) {
      AddItemWithStringIdAndIcon(
          MENU_HIDE_DESK_NAME, IDS_ASH_SHELF_CONTEXT_MENU_HIDE_DESK_NAME,
          ui::ImageModel::FromVectorIcon(kDeskButtonVisibilityOffIcon,
                                         ui::kColorAshSystemUIMenuIcon));
    } else {
      AddItemWithStringIdAndIcon(
          MENU_SHOW_DESK_NAME, IDS_ASH_SHELF_CONTEXT_MENU_SHOW_DESK_NAME,
          ui::ImageModel::FromVectorIcon(kDeskButtonVisibilityOnIcon,
                                         ui::kColorAshSystemUIMenuIcon));
    }
  }
}

}  // namespace ash
