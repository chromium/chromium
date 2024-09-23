// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_manager_builder.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/sync/base/features.h"
#include "content/public/browser/network_service_instance.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "components/unexportable_keys/unexportable_key_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/profile_account_manager.h"
#include "chrome/browser/lacros/account_manager/profile_account_manager_factory.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/functional/bind.h"
#include "chrome/browser/signin/signin_util_win.h"
#endif

void IdentityManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  signin::IdentityManager::RegisterProfilePrefs(registry);
}

IdentityManagerFactory::IdentityManagerFactory()
    : ProfileKeyedServiceFactory(
          "IdentityManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DependsOn(WebDataServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  DependsOn(UnexportableKeyServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  DependsOn(ProfileAccountManagerFactory::GetInstance());
#endif
  DependsOn(ChromeSigninClientFactory::GetInstance());
  signin::SetIdentityManagerProvider(
      base::BindRepeating([](content::BrowserContext* context) {
        return GetForProfile(Profile::FromBrowserContext(context));
      }));
  // TODO(crbug.com/40244790): This should declare a dependency to
  // CookieSettingsFactory but this causes a hang for some reason.
}

IdentityManagerFactory::~IdentityManagerFactory() {
  signin::SetIdentityManagerProvider({});
}

// static
signin::IdentityManager* IdentityManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<signin::IdentityManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
signin::IdentityManager* IdentityManagerFactory::GetForProfileIfExists(
    const Profile* profile) {
  return static_cast<signin::IdentityManager*>(
      GetInstance()->GetServiceForBrowserContext(const_cast<Profile*>(profile),
                                                 false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  return base::Singleton<IdentityManagerFactory>::get();
}

// static
void IdentityManagerFactory::EnsureFactoryAndDependeeFactoriesBuilt() {
  IdentityManagerFactory::GetInstance();
  ChromeSigninClientFactory::GetInstance();
}

void IdentityManagerFactory::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManagerFactory::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

KeyedService* IdentityManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  signin::IdentityManagerBuildParams params;
  params.account_consistency =
      AccountConsistencyModeManager::GetMethodForProfile(profile),
  params.image_decoder = std::make_unique<ImageDecoderImpl>();
  params.local_state = g_browser_process->local_state();
  params.network_connection_tracker = content::GetNetworkConnectionTracker();
  params.pref_service = profile->GetPrefs();
  params.profile_path = profile->GetPath();
  params.signin_client = ChromeSigninClientFactory::GetForProfile(profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  {
    scoped_refptr<content_settings::CookieSettings> cookie_settings =
        CookieSettingsFactory::GetForProfile(profile);
    params.delete_signin_cookies_on_exit =
        signin::SettingsDeleteSigninCookiesOnExit(cookie_settings.get());
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  params.token_web_data = WebDataServiceFactory::GetTokenWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  params.unexportable_key_service =
      UnexportableKeyServiceFactory::GetForProfile(profile);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#endif  // #if BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  params.account_manager_facade =
      GetAccountManagerFacade(profile->GetPath().value());
  params.is_regular_profile = ash::ProfileHelper::IsUserProfile(profile);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The system and (original profile of the) guest profiles are not regular.
  const bool is_regular_profile = profile->IsRegularProfile();
  const bool use_profile_account_manager =
      is_regular_profile &&
      // `ProfileManager` may be null in tests, and is required for account
      // consistency.
      g_browser_process->profile_manager();

  params.account_manager_facade =
      use_profile_account_manager
          ? ProfileAccountManagerFactory::GetForProfile(profile)
          : GetAccountManagerFacade(profile->GetPath().value());
  params.is_regular_profile = is_regular_profile;
#endif

#if BUILDFLAG(IS_WIN)
  params.reauth_callback =
      base::BindRepeating(&signin_util::ReauthWithCredentialProviderIfPossible,
                          base::Unretained(profile));
#endif

  params.require_sync_consent_for_scope_verification =
      !base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos);

  std::unique_ptr<signin::IdentityManager> identity_manager =
      signin::BuildIdentityManager(&params);

  for (Observer& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager.release();
}
