// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "services/network/cert_verifier_with_trust_anchors.h"

namespace policy {
namespace {

// Returns the PolicyCertificateProvider that should be used for |profile|.
// May return nullptr, which should be treated as no policy-provided
// certificates set.
chromeos::PolicyCertificateProvider* GetPolicyCertificateProvider(
    Profile* profile) {
  if (chromeos::ProfileHelper::Get()->IsSigninProfile(profile)) {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetDeviceNetworkConfigurationUpdater();
  }

  return UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);
}

}  // namespace

// static
PolicyCertService* PolicyCertServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PolicyCertService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
bool PolicyCertServiceFactory::CreateAndStartObservingForProfile(
    Profile* profile) {
  // Note that this can be called multiple times if the network process crashes.
  return GetInstance()->GetServiceForBrowserContext(profile, true) != nullptr;
}

// static
PolicyCertServiceFactory* PolicyCertServiceFactory::GetInstance() {
  return base::Singleton<PolicyCertServiceFactory>::get();
}

// static
void PolicyCertServiceFactory::SetUsedPolicyCertificates(
    const std::string& user_id) {
  if (UsedPolicyCertificates(user_id))
    return;
  ListPrefUpdate update(g_browser_process->local_state(),
                        prefs::kUsedPolicyCertificates);
  update->AppendString(user_id);
}

// static
void PolicyCertServiceFactory::ClearUsedPolicyCertificates(
    const std::string& user_id) {
  ListPrefUpdate update(g_browser_process->local_state(),
                        prefs::kUsedPolicyCertificates);
  update->Remove(base::Value(user_id), nullptr);
}

// static
bool PolicyCertServiceFactory::UsedPolicyCertificates(
    const std::string& user_id) {
  base::Value value(user_id);
  const base::ListValue* list =
      g_browser_process->local_state()->GetList(prefs::kUsedPolicyCertificates);
  if (!list) {
    NOTREACHED();
    return false;
  }
  return list->Find(value) != list->end();
}

// static
void PolicyCertServiceFactory::RegisterPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterListPref(prefs::kUsedPolicyCertificates);
}

PolicyCertServiceFactory::PolicyCertServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyCertService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(UserNetworkConfigurationUpdaterFactory::GetInstance());
}

PolicyCertServiceFactory::~PolicyCertServiceFactory() {}

KeyedService* PolicyCertServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  chromeos::PolicyCertificateProvider* policy_certificate_provider =
      GetPolicyCertificateProvider(profile);
  if (!policy_certificate_provider)
    return nullptr;

  if (chromeos::ProfileHelper::Get()->IsSigninProfile(profile)) {
    return new PolicyCertService(profile, policy_certificate_provider,
                                 /*may_use_profile_wide_trust_anchors=*/false,
                                 /*user_id=*/std::string(),
                                 /*user_manager=*/nullptr);
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          profile->GetOriginalProfile());
  if (!user)
    return nullptr;

  bool may_use_profile_wide_trust_anchors =
      user == user_manager->GetPrimaryUser() &&
      user->GetType() != user_manager::USER_TYPE_GUEST;

  return new PolicyCertService(
      profile, policy_certificate_provider, may_use_profile_wide_trust_anchors,
      user->GetAccountId().GetUserEmail(), user_manager);
}

content::BrowserContext* PolicyCertServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool PolicyCertServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy
