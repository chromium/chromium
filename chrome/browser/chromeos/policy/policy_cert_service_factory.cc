// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
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
#include "services/network/public/cpp/features.h"

namespace policy {

// static
PolicyCertService* PolicyCertServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PolicyCertService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
std::unique_ptr<network::CertVerifierWithTrustAnchors>
PolicyCertServiceFactory::CreateForProfile(Profile* profile) {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(!GetInstance()->GetServiceForBrowserContext(profile, false));
  PolicyCertService* service = static_cast<PolicyCertService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
  if (!service)
    return nullptr;
  return service->CreatePolicyCertVerifier();
}

// static
bool PolicyCertServiceFactory::CreateAndStartObservingForProfile(
    Profile* profile) {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  // This can be called multiple times if the network process crashes.
  if (GetInstance()->GetServiceForBrowserContext(profile, false))
    return true;
  PolicyCertService* service = static_cast<PolicyCertService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
  if (!service)
    return false;
  service->StartObservingPolicyCerts();
  return true;
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
  update->Remove(base::Value(user_id), NULL);
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
  Profile* profile = static_cast<Profile*>(context);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          profile->GetOriginalProfile());
  if (!user)
    return NULL;

  UserNetworkConfigurationUpdater* net_conf_updater =
      UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);
  if (!net_conf_updater)
    return NULL;

  return new PolicyCertService(profile, user->GetAccountId().GetUserEmail(),
                               net_conf_updater, user_manager);
}

content::BrowserContext* PolicyCertServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool PolicyCertServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy
