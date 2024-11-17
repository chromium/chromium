// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/policy_cert_service_factory.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/networking/device_network_configuration_updater_ash.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/incognito_helpers.h"
#endif

namespace policy {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the PolicyCertificateProvider that should be used for |profile|.
// May return nullptr, which should be treated as no policy-provided
// certificates set.
ash::PolicyCertificateProvider* GetPolicyCertificateProvider(Profile* profile) {
  if (ash::ProfileHelper::Get()->IsSigninProfile(profile)) {
    return g_browser_process->platform_part()
        ->browser_policy_connector_ash()
        ->GetDeviceNetworkConfigurationUpdater();
  }

  // The lock screen profile has access to the policy provided CA certs from the
  // primary profile.
  if (ash::ProfileHelper::Get()->IsLockScreenProfile(profile)) {
    return UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(
        ProfileManager::GetPrimaryUserProfile());
  }

  return UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);
}

std::unique_ptr<KeyedService> BuildServiceInstanceAsh(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  ash::PolicyCertificateProvider* policy_certificate_provider =
      GetPolicyCertificateProvider(profile);
  if (!policy_certificate_provider)
    return nullptr;

  if (ash::ProfileHelper::Get()->IsSigninProfile(profile)) {
    return std::make_unique<PolicyCertService>(
        profile, policy_certificate_provider,
        /*may_use_profile_wide_trust_anchors=*/false);
  }

  if (ash::ProfileHelper::Get()->IsLockScreenProfile(profile)) {
    return std::make_unique<PolicyCertService>(
        profile, policy_certificate_provider,
        /*may_use_profile_wide_trust_anchors=*/true);
  }

  // Don't allow policy-provided certificates for "special" Profiles except the
  // one listed above.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user = ash::ProfileHelper::Get()->GetUserByProfile(
      profile->GetOriginalProfile());
  if (!user)
    return nullptr;

  // Only allow trusted policy-provided certificates for non-guest primary
  // users. Guest users don't have user policy, but set
  // `may_use_profile_wide_trust_anchors`=false for them out of caution against
  // future changes.
  bool may_use_profile_wide_trust_anchors =
      user == user_manager->GetPrimaryUser() &&
      user->GetType() != user_manager::UserType::kGuest;

  return std::make_unique<PolicyCertService>(
      profile, policy_certificate_provider, may_use_profile_wide_trust_anchors);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::unique_ptr<KeyedService> BuildServiceInstanceLacros(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  ash::PolicyCertificateProvider* policy_certificate_provider =
      UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);
  if (!policy_certificate_provider)
    return nullptr;

  Profile* original_profile = Profile::FromBrowserContext(
      GetBrowserContextRedirectedInIncognito(profile));
  // Only allow trusted policy-provided certificates for non-guest primary
  // users. Guest users don't have user policy, but set
  // `may_use_profile_wide_trust_anchors`=false for them out of caution against
  // future changes.
  bool may_use_profile_wide_trust_anchors =
      original_profile->IsMainProfile() && !original_profile->IsGuestSession();

  return std::make_unique<PolicyCertService>(
      profile, policy_certificate_provider, may_use_profile_wide_trust_anchors);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
  static base::NoDestructor<PolicyCertServiceFactory> instance;
  return instance.get();
}

PolicyCertServiceFactory::PolicyCertServiceFactory()
    : ProfileKeyedServiceFactory(
          "PolicyCertService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(UserNetworkConfigurationUpdaterFactory::GetInstance());
}

PolicyCertServiceFactory::~PolicyCertServiceFactory() = default;

std::unique_ptr<KeyedService>
PolicyCertServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return BuildServiceInstanceAsh(context);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return BuildServiceInstanceLacros(context);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

bool PolicyCertServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy
