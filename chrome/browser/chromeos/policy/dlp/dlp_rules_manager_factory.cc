// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"

#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace policy {

namespace {

bool CanBuildServiceForProfile(const Profile* profile) {
  if (!profile)
    return false;

  // UserManager might be not be available in tests.
  bool is_main_profile = user_manager::UserManager::IsInitialized() &&
                         ash::ProfileHelper::IsPrimaryProfile(profile);
  return is_main_profile && profile->GetProfilePolicyConnector()->IsManaged();
}

}  // namespace

// static
DlpRulesManagerFactory* DlpRulesManagerFactory::GetInstance() {
  static base::NoDestructor<DlpRulesManagerFactory> factory;
  return factory.get();
}

// static
DlpRulesManager* DlpRulesManagerFactory::GetForPrimaryProfile() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile)
    return nullptr;
  return static_cast<DlpRulesManager*>(
      DlpRulesManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

DlpRulesManagerFactory::DlpRulesManagerFactory()
    : ProfileKeyedServiceFactory(
          "DlpRulesManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

bool DlpRulesManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // We have to create the instance immediately because it's responsible for
  // instantiation of DataTransferDlpController. Otherwise even if the policy is
  // present, DataTransferDlpController won't be instantiated and therefore no
  // policy will be applied.
  return true;
}

std::unique_ptr<KeyedService>
DlpRulesManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!CanBuildServiceForProfile(profile))
    return nullptr;

  PrefService* local_state = g_browser_process->local_state();
  // Might be not available in tests.
  if (!local_state)
    return nullptr;

  return std::make_unique<DlpRulesManagerImpl>(local_state, profile);
}
}  // namespace policy
