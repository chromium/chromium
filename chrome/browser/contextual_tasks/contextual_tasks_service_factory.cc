// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/contextual_tasks/internal/composite_context_decorator.h"
#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/browser/browser_context.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/tab_strip_context_decorator.h"
#endif

namespace contextual_tasks {

// static
ContextualTasksServiceFactory* ContextualTasksServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualTasksServiceFactory> instance;
  return instance.get();
}

// static
ContextualTasksService* ContextualTasksServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<ContextualTasksService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

ContextualTasksServiceFactory::ContextualTasksServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextualTasksService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(AimEligibilityServiceFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

ContextualTasksServiceFactory::~ContextualTasksServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextualTasksServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kContextualTasks)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      additional_decorators;

#if !BUILDFLAG(IS_ANDROID)
  additional_decorators.emplace(
      ContextualTaskContextSource::kTabStrip,
      std::make_unique<TabStripContextDecorator>(profile));
#endif

  bool supports_ephemeral_only =
      profile->IsOffTheRecord() || profile->IsGuestSession();

  return std::make_unique<ContextualTasksServiceImpl>(
      chrome::GetChannel(),
      DataTypeStoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context))
          ->GetStoreFactory(),
      CreateCompositeContextDecorator(favicon_service, history_service,
                                      std::move(additional_decorators)),
      aim_eligibility_service, identity_manager, profile->GetPrefs(),
      supports_ephemeral_only);
}

}  // namespace contextual_tasks
