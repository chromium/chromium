// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"

#include <string>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/enterprise/enterprise_site_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck crbug.com/1125897
#endif

// static
TemplateURLService* TemplateURLServiceFactory::GetForProfile(Profile* profile) {
  TRACE_EVENT0("loading", "TemplateURLServiceFactory::GetForProfile");

  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  static base::NoDestructor<TemplateURLServiceFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService> TemplateURLServiceFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  base::RepeatingClosure dsp_change_callback;
#if BUILDFLAG(ENABLE_RLZ)
  dsp_change_callback = base::BindRepeating(
      base::IgnoreResult(&rlz::RLZTracker::RecordProductEvent), rlz_lib::CHROME,
      rlz::RLZTracker::ChromeOmnibox(), rlz_lib::SET_TO_GOOGLE);
#endif
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TemplateURLService>(
      CHECK_DEREF(profile->GetPrefs()),
      CHECK_DEREF(
          search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
              profile)),
      std::make_unique<UIThreadSearchTermsData>(),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::unique_ptr<TemplateURLServiceClient>(
          new ChromeTemplateURLServiceClient(
              HistoryServiceFactory::GetForProfile(
                  profile, ServiceAccessType::EXPLICIT_ACCESS))),
      dsp_change_callback
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      ,
      profile->IsMainProfile()
#endif  // BUIDFLAG(IS_CHROMEOS_LACROS)
  );
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : ProfileKeyedServiceFactory(
          "TemplateURLServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Needed for Guest sessions because they have an omnibox and
              // thus need template URLs (search providers).
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // It's not possible for the user to search in a system profile.
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(search_engines::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() = default;

std::unique_ptr<KeyedService>
TemplateURLServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS creates various unusual profiles (login, lock screen...) that do
  // not need a template URL service and cannot search.  The only non-regular
  // profile that needs a template URL is the signin profile.  The
  // signin profile sometimes can detect a "captive portal" (i.e., a network
  // connection that requires a login before it is usable).  The captive portal
  // sign-in flow creates a window with a URL bar.  The URL bar code currently
  // assumes a template URL service exists.  (This is true even though the user
  // cannot search from the captive portal sign-in window.)
  if (!ash::ProfileHelper::IsUserProfile(profile) &&
      !ash::ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }
#endif

  return BuildInstanceFor(profile);
}

void TemplateURLServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DefaultSearchManager::RegisterProfilePrefs(registry);
  EnterpriseSiteSearchManager::RegisterProfilePrefs(registry);
  TemplateURLService::RegisterProfilePrefs(registry);
}

bool TemplateURLServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
