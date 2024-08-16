// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_extensions_metrics_delegate_impl.h"
#endif

// static
supervised_user::SupervisedUserMetricsService*
SupervisedUserMetricsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<supervised_user::SupervisedUserMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
SupervisedUserMetricsServiceFactory*
SupervisedUserMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserMetricsServiceFactory> factory;
  return factory.get();
}

SupervisedUserMetricsServiceFactory::SupervisedUserMetricsServiceFactory()
    : ProfileKeyedServiceFactory(
          "SupervisedUserMetricsServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  // Used for tracking web filter metrics.
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

SupervisedUserMetricsServiceFactory::~SupervisedUserMetricsServiceFactory() =
    default;

void SupervisedUserMetricsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  supervised_user::SupervisedUserMetricsService::RegisterProfilePrefs(registry);
}

KeyedService* SupervisedUserMetricsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<supervised_user::SupervisedUserMetricsService ::
                      SupervisedUserMetricsServiceExtensionDelegate>
      extensions_metrics_delegate = nullptr;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  extensions_metrics_delegate =
      std::make_unique<SupervisedUserExtensionsMetricsDelegateImpl>(
          extensions::ExtensionRegistry::Get(profile), profile);
  CHECK(extensions_metrics_delegate);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return new supervised_user::SupervisedUserMetricsService(
      profile->GetPrefs(),
      SupervisedUserServiceFactory::GetForProfile(profile)->GetURLFilter(),
      std::move(extensions_metrics_delegate));
}

bool SupervisedUserMetricsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SupervisedUserMetricsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
