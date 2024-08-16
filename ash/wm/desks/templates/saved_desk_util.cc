// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_util.h"
#include "base/containers/adapters.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

// The next activation index to assign to an admin template or floating
// workspace template window.
int32_t g_template_next_activation_index = kTemplateStartingActivationIndex;

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

bool IsGuestSession() {
  // User 0 is the current user.
  const UserIndex user_index = 0;
  const UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);
  return user_session->user_info.type == user_manager::UserType::kGuest;
}

// Returns true if all windows have bounds.
bool DoesAllWindowsHaveActivationIndices(const DeskTemplate& admin_template) {
  const auto& app_id_to_launch_list =
      admin_template.desk_restore_data()->app_id_to_launch_list();
  for (auto& [app_id, launch_list] : app_id_to_launch_list) {
    for (auto& [window_id, app_restore_data] : launch_list) {
      if (!app_restore_data->window_info.activation_index.has_value()) {
        return false;
      }
    }
  }
  return true;
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

bool ShouldShowSavedDesksOptions() {
  return !IsGuestSession() && !window_util::IsInFasterSplitScreenSetupSession();
}

bool ShouldShowSavedDesksOptionsForDesk(Desk* desk, DeskBarViewBase* bar_view) {
  if (!features::IsSavedDeskUiRevampEnabled()) {
    return false;
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  // TODO(hewer): Consult with UX if we should hide the save desk options when
  // we are in the saved desk library.
  return desk->is_active() &&
         (desk->ContainsAppWindows() ||
          !DesksController::Get()->visible_on_all_desks_windows().empty()) &&
         bar_view->type() == DeskBarViewBase::Type::kOverview &&
         saved_desk_util::ShouldShowSavedDesksOptions();
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

std::string GetAppId(aura::Window* window) {
  // Check with full restore first.
  if (std::string app_id = full_restore::GetAppId(window); !app_id.empty()) {
    return app_id;
  }

  // Otherwise, check if there's a window property.
  // TODO(b/288153585): Figure out if the kAppIDKey property is always going to
  // be set for window types that we care about. If that is the case, we should
  // be able to drop the call to full restore.
  if (const std::string* app_id_property = window->GetProperty(kAppIDKey)) {
    return *app_id_property;
  }

  return "";
}

bool IsWindowOnTopForTemplate(aura::Window* window) {
  const int32_t* activation_index =
      window->GetProperty(app_restore::kActivationIndexKey);
  return activation_index &&
         *activation_index <= kTemplateStartingActivationIndex;
}

void UpdateTemplateActivationIndices(DeskTemplate& saved_desk) {
  auto& app_id_to_launch_list =
      saved_desk.mutable_desk_restore_data()->mutable_app_id_to_launch_list();
  // Go through the windows as defined in the saved desk in reverse order so
  // that the window with the lowest id gets the lowest activation index. NB:
  // for now, we expect admin templates to only contain a single app.
  for (auto& [app_id, launch_list] : app_id_to_launch_list) {
    for (auto& [window_id, app_restore_data] : base::Reversed(launch_list)) {
      app_restore_data->window_info.activation_index =
          g_template_next_activation_index--;
    }
  }
}

void UpdateTemplateActivationIndicesRelativeOrder(DeskTemplate& saved_desk) {
  // Use relative ordering iff every window has an activation index.
  if (!DoesAllWindowsHaveActivationIndices(saved_desk)) {
    UpdateTemplateActivationIndices(saved_desk);
    return;
  }

  auto& app_id_to_launch_list =
      saved_desk.mutable_desk_restore_data()->mutable_app_id_to_launch_list();
  std::vector<app_restore::AppRestoreData*> relative_window_stack_order;
  for (auto& [app_id, launch_list] : app_id_to_launch_list) {
    for (auto& [window_id, app_restore_data] : launch_list) {
      relative_window_stack_order.push_back(app_restore_data.get());
    }
  }
  // Sort in descending order so that we maintain the relative window
  // stacking order.
  base::ranges::sort(relative_window_stack_order,
                     [](app_restore::AppRestoreData* data1,
                        app_restore::AppRestoreData* data2) {
                       return data1->window_info.activation_index.value_or(0) >
                              data2->window_info.activation_index.value_or(0);
                     });
  for (auto* app_restore_data : relative_window_stack_order) {
    app_restore_data->window_info.activation_index =
        g_template_next_activation_index--;
  }
}

}  // namespace saved_desk_util
}  // namespace ash
