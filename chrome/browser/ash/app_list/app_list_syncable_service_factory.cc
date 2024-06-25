// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"

#include <set>

#include "build/build_config.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace app_list {

namespace {
bool use_in_testing = false;
}

// static
AppListSyncableService* AppListSyncableServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppListSyncableService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppListSyncableServiceFactory* AppListSyncableServiceFactory::GetInstance() {
  static base::NoDestructor<AppListSyncableServiceFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService> AppListSyncableServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
  // This condition still needs to be explicitly stated here despite having
  // ProfileKeyedService logic implemented because `IsGuestSession()` and
  // `IsRegularProfile()` are not yet mutually exclusive in ASH and Lacros.
  // TODO(rsult): remove this condition when `IsGuestSession() is fixed.
  if (profile->IsGuestSession() && !profile->IsOffTheRecord())
    return nullptr;

  VLOG(1) << "BuildInstanceFor: " << profile->GetDebugName() << " (" << profile
          << ")";
  return std::make_unique<AppListSyncableService>(profile);
}

// static
void AppListSyncableServiceFactory::SetUseInTesting(bool use) {
  use_in_testing = use;
}

AppListSyncableServiceFactory::AppListSyncableServiceFactory()
    : ProfileKeyedServiceFactory(
          "AppListSyncableService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // No service for sign in profile.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  VLOG(1) << "AppListSyncableServiceFactory()";
  typedef std::set<BrowserContextKeyedServiceFactory*> FactorySet;
  FactorySet dependent_factories;
  dependent_factories.insert(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  dependent_factories.insert(ArcAppListPrefsFactory::GetInstance());
  dependent_factories.insert(apps::AppPreloadServiceFactory::GetInstance());
  dependent_factories.insert(apps::AppServiceProxyFactory::GetInstance());
  dependent_factories.insert(
      ash::FileSuggestKeyedServiceFactory::GetInstance());
  for (auto* dependent_factory : dependent_factories)
    DependsOn(dependent_factory);
}

AppListSyncableServiceFactory::~AppListSyncableServiceFactory() = default;

std::unique_ptr<KeyedService>
AppListSyncableServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return BuildInstanceFor(static_cast<Profile*>(browser_context));
}

void AppListSyncableServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

bool AppListSyncableServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Start AppListSyncableService early so that the app list positions are
  // available before the app list is opened.
  return true;
}

// static
bool AppListSyncableServiceFactory::IsUsedInTesting() {
  return use_in_testing;
}

bool AppListSyncableServiceFactory::ServiceIsNULLWhileTesting() const {
  return !IsUsedInTesting();
}

}  // namespace app_list
