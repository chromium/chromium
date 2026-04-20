// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/android/contextual_tasks_ui_service_delegate_android.h"
#else
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_desktop.h"
#endif

namespace contextual_tasks {

// static
ContextualTasksUiServiceFactory*
ContextualTasksUiServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualTasksUiServiceFactory> instance;
  return instance.get();
}

// static
ContextualTasksUiService* ContextualTasksUiServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ContextualTasksUiService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ContextualTasksUiService*
ContextualTasksUiServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<ContextualTasksUiService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

ContextualTasksUiServiceFactory::ContextualTasksUiServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextualTasksUiService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ContextualTasksServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(AimEligibilityServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ContextualTasksUiServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kContextualTasks)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_ANDROID)
  auto delegate =
      std::make_unique<ContextualTasksUiServiceDelegateAndroid>(profile);
#else
  auto delegate =
      std::make_unique<ContextualTasksUiServiceDelegateDesktop>(profile);
#endif
  std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer;
  if (contextual_tasks::ShouldEnableCookieSync()) {
    cookie_synchronizer = std::make_unique<ContextualTasksCookieSynchronizer>(
        profile, IdentityManagerFactory::GetForProfile(profile));
  }

  return std::make_unique<ContextualTasksUiService>(
      profile, std::move(delegate),
      ContextualTasksServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      AimEligibilityServiceFactory::GetForProfile(profile),
      std::move(cookie_synchronizer));
}

void ContextualTasksUiServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(kContextualTasksOnboardingTooltipDismissedCount,
                                0);
  registry->RegisterBooleanPref(kContextualTasksShareOpenTabsEveryThread,
                                false);
  registry->RegisterDictionaryPref(kContextualTasksSiteExclusions);
}

bool ContextualTasksUiServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ContextualTasksUiServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
}  // namespace contextual_tasks
