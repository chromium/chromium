// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace contextual_tasks {
DEFINE_USER_DATA(EntryPointEligibilityManager);

EntryPointEligibilityManager::EntryPointEligibilityManager(
    BrowserWindowInterface* browser_window_interface)
    : profile_(browser_window_interface->GetProfile()),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  ContextualTasksUiService* const contextual_tasks_ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_);
  if (contextual_tasks_ui_service) {
    ContextualTasksEligibilityManager* eligibility_manager =
        contextual_tasks_ui_service->GetEligibilityManager();
    if (eligibility_manager) {
      eligibility_subscription_ =
          eligibility_manager->RegisterEligibilityChangedCallback(
              base::BindRepeating(&EntryPointEligibilityManager::
                                      MaybeNotifyEntryPointEligibilityChanged,
                                  base::Unretained(this)));
    }
  }
  entry_points_are_eligible_ = AreEntryPointsEligible();
}

EntryPointEligibilityManager::~EntryPointEligibilityManager() = default;

EntryPointEligibilityManager* EntryPointEligibilityManager::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

base::CallbackListSubscription
EntryPointEligibilityManager::RegisterOnEntryPointEligibilityChanged(
    EntryPointEligibilityChangeCallbackList::CallbackType callback) {
  return entry_point_eligibility_change_callback_list_.Add(std::move(callback));
}

bool EntryPointEligibilityManager::AreEntryPointsEligible() {
  return IsEligible(profile_);
}

// static
bool EntryPointEligibilityManager::IsEligible(Profile* profile) {
  // TODO(crbug.com/473082702): Find a robust way to mock the entrypoint
  // manager to allow tests to pass without needing a feature flag.
  if (base::FeatureList::IsEnabled(
          kContextualTasksForceEntryPointEligibility)) {
    return true;
  }

  ContextualTasksUiService* const contextual_tasks_ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile);
  if (!contextual_tasks_ui_service) {
    return false;
  }

  ContextualTasksEligibilityManager* eligibility_manager =
      contextual_tasks_ui_service->GetEligibilityManager();
  if (!eligibility_manager) {
    return false;
  }

  return eligibility_manager->IsEligible();
}

void EntryPointEligibilityManager::MaybeNotifyEntryPointEligibilityChanged(
    bool eligible) {
  const bool updated_eligibility = AreEntryPointsEligible();
  if (entry_points_are_eligible_ != updated_eligibility) {
    entry_points_are_eligible_ = updated_eligibility;
    entry_point_eligibility_change_callback_list_.Notify(
        entry_points_are_eligible_);
  }
}
}  // namespace contextual_tasks
