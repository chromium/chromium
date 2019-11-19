// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/arc/arc_notification_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Returns the Shelf instance for the display with the given |display_id|.
Shelf* GetShelfForDisplay(int64_t display_id) {
  // The controller may be null for invalid ids or for displays being removed.
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller ? root_window_controller->shelf() : nullptr;
}

// Set each Shelf's auto-hide behavior from the per-display pref.
void SetShelfAutoHideFromPrefs() {
  // TODO(jamescook): The session state check should not be necessary, but
  // otherwise this wrongly tries to set the alignment on a secondary display
  // during login before the ShelfLockingManager is created.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  PrefService* prefs = session_controller->GetLastActiveUserPrefService();
  if (!prefs || !session_controller->IsActiveUserSessionStarted())
    return;

  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    auto value = GetShelfAutoHideBehaviorPref(prefs, display.id());
    // Don't show the shelf in app mode.
    if (session_controller->IsRunningInAppMode())
      value = SHELF_AUTO_HIDE_ALWAYS_HIDDEN;
    if (Shelf* shelf = GetShelfForDisplay(display.id()))
      shelf->SetAutoHideBehavior(value);
  }
}

// Set each Shelf's alignment from the per-display pref.
void SetShelfAlignmentFromPrefs() {
  // TODO(jamescook): The session state check should not be necessary, but
  // otherwise this wrongly tries to set the alignment on a secondary display
  // during login before the ShelfLockingManager is created.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  PrefService* prefs = session_controller->GetLastActiveUserPrefService();
  if (!prefs || !session_controller->IsActiveUserSessionStarted())
    return;

  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (Shelf* shelf = GetShelfForDisplay(display.id()))
      shelf->SetAlignment(GetShelfAlignmentPref(prefs, display.id()));
  }
}

void UpdateShelfVisibility() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (Shelf* shelf = GetShelfForDisplay(display.id()))
      shelf->UpdateVisibilityState();
  }
}

// Set each Shelf's auto-hide behavior and alignment from the per-display prefs.
void SetShelfBehaviorsFromPrefs() {
  SetShelfAutoHideFromPrefs();

  // The shelf should always be bottom-aligned in tablet mode; alignment is
  // assigned from prefs when tablet mode is exited.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    return;
  }

  SetShelfAlignmentFromPrefs();
}

}  // namespace

ShelfController::ShelfController()
    : is_notification_indicator_enabled_(
          features::IsNotificationIndicatorEnabled()) {
  ShelfModel::SetInstance(&model_);

  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);

  if (is_notification_indicator_enabled_)
    message_center_observer_.Add(message_center::MessageCenter::Get());
}

ShelfController::~ShelfController() {
  model_.DestroyItemDelegates();
}

void ShelfController::Shutdown() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void ShelfController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // These prefs are public for ChromeLauncherController's OnIsSyncingChanged.
  // See the pref names definitions for explanations of the synced, local, and
  // per-display behaviors.
  registry->RegisterStringPref(
      prefs::kShelfAutoHideBehavior, kShelfAutoHideBehaviorNever,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(prefs::kShelfAutoHideBehaviorLocal,
                               std::string());
  registry->RegisterStringPref(
      prefs::kShelfAlignment, kShelfAlignmentBottom,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(prefs::kShelfAlignmentLocal, std::string());
  registry->RegisterDictionaryPref(prefs::kShelfPreferences);
}

void ShelfController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  SetShelfBehaviorsFromPrefs();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(prefs::kShelfAlignmentLocal,
                              base::BindRepeating(&SetShelfAlignmentFromPrefs));
  pref_change_registrar_->Add(prefs::kShelfAutoHideBehaviorLocal,
                              base::BindRepeating(&SetShelfAutoHideFromPrefs));
  pref_change_registrar_->Add(prefs::kShelfPreferences,
                              base::BindRepeating(&SetShelfBehaviorsFromPrefs));
}

void ShelfController::OnTabletModeStarted() {
  // Do nothing when running in app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  // Force the shelf to be visible and to be bottom aligned in tablet mode; the
  // prefs are restored on exit.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (Shelf* shelf = GetShelfForDisplay(display.id())) {
      // Only animate into tablet mode if the shelf alignment will not change.
      if (shelf->IsHorizontalAlignment())
        shelf->set_is_tablet_mode_animation_running(true);
      shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
      shelf->shelf_widget()->OnTabletModeChanged();
    }
  }
}

void ShelfController::OnTabletModeEnded() {
  // Do nothing when running in app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  SetShelfBehaviorsFromPrefs();
  // Only animate out of tablet mode if the shelf alignment will not change.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (Shelf* shelf = GetShelfForDisplay(display.id())) {
      if (shelf->IsHorizontalAlignment())
        shelf->set_is_tablet_mode_animation_running(true);
      shelf->shelf_widget()->OnTabletModeChanged();
    }
  }
}

void ShelfController::OnDisplayConfigurationChanged() {
  // Update the alignment and auto-hide state from prefs, because a display may
  // have been added, or the display ids for existing shelf instances may have
  // changed. See https://crbug.com/748291
  SetShelfBehaviorsFromPrefs();

  // Update shelf visibility to adapt to display changes. For instance shelf
  // should be hidden on secondary display during inactive session states.
  UpdateShelfVisibility();
}

void ShelfController::OnNotificationAdded(const std::string& notification_id) {
  if (!is_notification_indicator_enabled_)
    return;

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);

  if (!notification)
    return;

  // Skip this if the notification shouldn't badge an app.
  if (notification->notifier_id().type !=
          message_center::NotifierType::APPLICATION &&
      notification->notifier_id().type !=
          message_center::NotifierType::ARC_APPLICATION) {
    return;
  }

  // Skip this if the notification doesn't have a valid app id.
  if (notification->notifier_id().id == kDefaultArcNotifierId)
    return;

  model_.AddNotificationRecord(notification->notifier_id().id, notification_id);
}

void ShelfController::OnNotificationRemoved(const std::string& notification_id,
                                            bool by_user) {
  if (!is_notification_indicator_enabled_)
    return;

  model_.RemoveNotificationRecord(notification_id);
}

}  // namespace ash
