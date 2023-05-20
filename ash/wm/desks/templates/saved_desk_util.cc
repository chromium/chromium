// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "components/app_restore/window_properties.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

bool IsGuestSession() {
  // User 0 is the current user.
  const UserIndex user_index = 0;
  const UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  return user_session->user_info.type == user_manager::USER_TYPE_GUEST;
}

}  // namespace

namespace saved_desk_util {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDeskTemplatesEnabled, false);
  registry->RegisterListPref(prefs::kAppLaunchAutomation);
}

bool AreDesksTemplatesEnabled() {
  if (IsGuestSession())
    return false;

  if (PrefService* pref_service = GetPrimaryUserPrefService()) {
    const PrefService::Preference* desk_templates_pref =
        pref_service->FindPreference(prefs::kDeskTemplatesEnabled);
    DCHECK(desk_templates_pref);

    if (desk_templates_pref->IsManaged()) {
      // Let policy settings override flags configuration.
      return pref_service->GetBoolean(prefs::kDeskTemplatesEnabled);
    }
  }

  // Allow the feature to be enabled by user when there is not explicit policy.
  return features::AreDesksTemplatesEnabled();
}

bool IsSavedDesksEnabled() {
  return !IsGuestSession();
}
SavedDeskDialogController* GetSavedDeskDialogController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return nullptr;

  SavedDeskDialogController* dialog_controller =
      overview_controller->overview_session()->saved_desk_dialog_controller();
  DCHECK(dialog_controller);
  return dialog_controller;
}

SavedDeskPresenter* GetSavedDeskPresenter() {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return nullptr;

  SavedDeskPresenter* presenter =
      overview_controller->overview_session()->saved_desk_presenter();
  DCHECK(presenter);
  return presenter;
}

bool IsAdminTemplateWindow(aura::Window* window) {
  const int32_t* activation_index =
      window->GetProperty(app_restore::kActivationIndexKey);
  return activation_index &&
         *activation_index <= kAdminTemplateStartingActivationIndex;
}

}  // namespace saved_desk_util
}  // namespace ash
