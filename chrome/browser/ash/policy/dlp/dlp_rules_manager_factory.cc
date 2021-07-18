// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"

#include "base/bind_post_task.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace policy {

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
    : BrowserContextKeyedServiceFactory(
          "DlpRulesManager",
          BrowserContextDependencyManager::GetInstance()) {}

bool DlpRulesManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // We have to create the instance immediately because it's responsible for
  // instantiation of DataTransferDlpController. Otherwise even if the policy is
  // present, DataTransferDlpController won't be instantiated and therefore no
  // policy will be applied.
  return true;
}

KeyedService* DlpRulesManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // UserManager might be not available in tests.
  if (!user_manager::UserManager::IsInitialized() || !profile ||
      !chromeos::ProfileHelper::IsPrimaryProfile(profile) ||
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }

  PrefService* local_state = g_browser_process->local_state();
  // Might be not available in tests.
  if (!local_state)
    return nullptr;

  auto dm_token = GetDMToken(profile, /*only_affiliated=*/false);
  if (!dm_token.is_valid()) {
    LOG(ERROR) << "DlpReporting has invalid DMToken. Reporting disabled.";
    return nullptr;
  }

  return new DlpRulesManagerImpl(local_state, dm_token.value());
}
}  // namespace policy
