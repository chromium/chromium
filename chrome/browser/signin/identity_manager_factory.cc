// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_manager_builder.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "content/public/browser/network_service_instance.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/account_manager/account_manager_factory.h"
#include "chrome/browser/account_manager_facade_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/account_manager_facade_factory.h"
#endif

#if defined(OS_WIN)
#include "base/bind.h"
#include "chrome/browser/signin/signin_util_win.h"
#endif

void IdentityManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  signin::IdentityManager::RegisterProfilePrefs(registry);
}

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "IdentityManager",
          BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  DependsOn(WebDataServiceFactory::GetInstance());
#endif
  DependsOn(ChromeSigninClientFactory::GetInstance());

  signin::SetIdentityManagerProvider(
      base::BindRepeating([](content::BrowserContext* context) {
        return GetForProfile(Profile::FromBrowserContext(context));
      }));
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

#if !defined(OS_ANDROID)
  params.delete_signin_cookies_on_exit =
      signin::SettingsDeleteSigninCookiesOnExit(
          CookieSettingsFactory::GetForProfile(profile).get());
  params.token_web_data = WebDataServiceFactory::GetTokenWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  DCHECK(factory);
  params.account_manager =
      factory->GetAccountManager(profile->GetPath().value());
  params.account_manager_facade =
      GetAccountManagerFacade(profile->GetPath().value());
  params.is_regular_profile =
      chromeos::ProfileHelper::IsRegularProfile(profile);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  params.account_manager_facade =
      GetAccountManagerFacade(profile->GetPath().value());
  // Lacros runs inside a user session and is not used to render Chrome OS's
  // Login Screen, or its Lock Screen. Hence, all Profiles in Lacros are regular
  // Profiles.
  params.is_regular_profile = true;
#endif

#if defined(OS_WIN)
  params.reauth_callback =
      base::BindRepeating(&signin_util::ReauthWithCredentialProviderIfPossible,
                          base::Unretained(profile));
#endif

  std::unique_ptr<signin::IdentityManager> identity_manager =
      signin::BuildIdentityManager(&params);

  for (Observer& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager.release();
}
