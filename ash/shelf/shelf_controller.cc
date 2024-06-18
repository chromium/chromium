// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/launcher_nudge_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
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
      value = ShelfAutoHideBehavior::kAlwaysHidden;
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

  // Tablet mode uses bottom aligned shelf, don't override it if the shelf
  // prefs change.
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (Shelf* shelf = GetShelfForDisplay(display.id()))
      shelf->SetAlignment(GetShelfAlignmentPref(prefs, display.id()));
  }
}

// Re-layouts the shelf on every display.
void LayoutShelves() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (Shelf* shelf = GetShelfForDisplay(display.id())) {
      shelf->shelf_layout_manager()->LayoutShelf(true);
    }
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
  if (display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  SetShelfAlignmentFromPrefs();
}

}  // namespace

ShelfController::ShelfController() {
  ShelfModel::SetInstance(&model_);

  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
  model_.AddObserver(this);
}

ShelfController::~ShelfController() {
  model_.DestroyItemDelegates();
}

void ShelfController::Init() {
  launcher_nudge_controller_ = std::make_unique<LauncherNudgeController>();
}

void ShelfController::Shutdown() {
  model_.RemoveObserver(this);
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void ShelfController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // These prefs are public for ChromeShelfController's OnIsSyncingChanged.
  // See the pref names definitions for explanations of the synced, local, and
  // per-display behaviors.
  registry->RegisterStringPref(
      prefs::kShelfAutoHideBehavior, kShelfAutoHideBehaviorNever,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(prefs::kShelfAutoHideBehaviorLocal,
                               std::string());
  if (base::FeatureList::IsEnabled(features::kShelfAutoHideSeparation)) {
    registry->RegisterStringPref(
        prefs::kShelfAutoHideTabletModeBehavior, kShelfAutoHideBehaviorNever,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterStringPref(prefs::kShelfAutoHideTabletModeBehaviorLocal,
                                 std::string());
  }
  if (base::FeatureList::IsEnabled(features::kDeskButton)) {
    registry->RegisterStringPref(
        prefs::kShowDeskButtonInShelf, std::string(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
    registry->RegisterBooleanPref(prefs::kDeviceUsesDesks, false);
  }
  registry->RegisterStringPref(
      prefs::kShelfAlignment, kShelfAlignmentBottom,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(prefs::kShelfAlignmentLocal, std::string());
  registry->RegisterDictionaryPref(prefs::kShelfPreferences);

  LauncherNudgeController::RegisterProfilePrefs(registry);
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
  if (base::FeatureList::IsEnabled(features::kShelfAutoHideSeparation)) {
    pref_change_registrar_->Add(
        prefs::kShelfAutoHideTabletModeBehaviorLocal,
        base::BindRepeating(&SetShelfAutoHideFromPrefs));
  }
  if (base::FeatureList::IsEnabled(features::kDeskButton)) {
    pref_change_registrar_->Add(prefs::kShowDeskButtonInShelf,
                                base::BindRepeating(&LayoutShelves));
    pref_change_registrar_->Add(prefs::kDeviceUsesDesks,
                                base::BindRepeating(&LayoutShelves));
  }
  pref_change_registrar_->Add(prefs::kShelfPreferences,
                              base::BindRepeating(&SetShelfBehaviorsFromPrefs));

  pref_change_registrar_->Add(
      prefs::kAppNotificationBadgingEnabled,
      base::BindRepeating(&ShelfController::UpdateAppNotificationBadging,
                          base::Unretained(this)));

  // Observe AppRegistryCache for the current active account to get
  // notification updates.
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  cache_ = apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);

  app_registry_cache_observer_.Reset();
  if (cache_) {
    app_registry_cache_observer_.Observe(cache_);
  }

  // Resetting the recorded pref forces the next call to
  // UpdateAppNotificationBadging() to update notification badging for every
  // app item.
  notification_badging_pref_enabled_.reset();

  // Update the notification badge indicator for all apps. This will also
  // ensure that apps have the correct notification badge value for the
  // multiprofile case when switching between users.
  UpdateAppNotificationBadging();
}

void ShelfController::OnDisplayTabletStateChanged(display::TabletState state) {
  // Do nothing when running in app mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode())
    return;

  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      // Do nothing when the tablet state is in the process of transition.
      break;
    case display::TabletState::kInTabletMode:
      if (base::FeatureList::IsEnabled(features::kShelfAutoHideSeparation)) {
        SetShelfAutoHideFromPrefs();
      }

      // Force the shelf to be bottom aligned in tablet mode; the prefs are
      // restored on exit.
      for (const auto& display :
           display::Screen::GetScreen()->GetAllDisplays()) {
        if (Shelf* shelf = GetShelfForDisplay(display.id())) {
          shelf->SetAlignment(ShelfAlignment::kBottom);
        }
      }
      break;
    case display::TabletState::kInClamshellMode:
      SetShelfBehaviorsFromPrefs();
      break;
  }
}

void ShelfController::OnDidApplyDisplayChanges() {
  // Update the alignment and auto-hide state from prefs, because a display may
  // have been added, or the display ids for existing shelf instances may have
  // changed. See https://crbug.com/748291
  SetShelfBehaviorsFromPrefs();

  // Update shelf visibility to adapt to display changes. For instance shelf
  // should be hidden on secondary display during inactive session states.
  UpdateShelfVisibility();
}

void ShelfController::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.HasBadgeChanged() &&
      notification_badging_pref_enabled_.value_or(false)) {
    bool has_badge = update.HasBadge().value_or(false);
    model_.UpdateItemNotification(update.AppId(), has_badge);
  }
}

void ShelfController::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void ShelfController::ShelfItemAdded(int index) {
  if (!cache_ || !notification_badging_pref_enabled_.value_or(false))
    return;

  auto app_id = model_.items()[index].id.app_id;

  // Update the notification badge indicator for the newly added shelf item.
  cache_->ForOneApp(app_id, [this](const apps::AppUpdate& update) {
    bool has_badge = update.HasBadge().value_or(false);
    model_.UpdateItemNotification(update.AppId(), has_badge);
  });
}

void ShelfController::UpdateAppNotificationBadging() {
  bool new_badging_enabled = pref_change_registrar_
                                 ? pref_change_registrar_->prefs()->GetBoolean(
                                       prefs::kAppNotificationBadgingEnabled)
                                 : false;

  if (notification_badging_pref_enabled_.has_value() &&
      notification_badging_pref_enabled_.value() == new_badging_enabled) {
    return;
  }
  notification_badging_pref_enabled_ = new_badging_enabled;

  if (cache_) {
    cache_->ForEachApp([this](const apps::AppUpdate& update) {
      // Set the app notification badge hidden when the pref is disabled.
      bool has_badge = notification_badging_pref_enabled_.value()
                           ? update.HasBadge().value_or(false)
                           : false;

      model_.UpdateItemNotification(update.AppId(), has_badge);
    });
  }
}

}  // namespace ash
