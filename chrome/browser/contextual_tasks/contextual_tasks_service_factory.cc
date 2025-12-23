// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
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
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/tab_strip_context_decorator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#endif

namespace contextual_tasks {

namespace {

size_t GetNumberOfActiveTasks(Profile* profile) {
  size_t number_of_active_tasks = 0;
#if !BUILDFLAG(IS_ANDROID)
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile, &number_of_active_tasks](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile) {
          return true;
        }

        // Check how many tasks are cached in the side panel.
        if (auto* coordinator =
                ContextualTasksSidePanelCoordinator::From(browser)) {
          number_of_active_tasks += coordinator->GetNumberOfActiveTasks();
        }

        // Check how many tasks are open in a full tab.
        TabStripModel* tab_strip_model = browser->GetTabStripModel();
        if (tab_strip_model) {
          for (int i = 0; i < tab_strip_model->count(); ++i) {
            if (auto* web_contents = tab_strip_model->GetWebContentsAt(i)) {
              if (web_contents->GetLastCommittedURL().scheme() ==
                      content::kChromeUIScheme &&
                  web_contents->GetLastCommittedURL().host() ==
                      chrome::kChromeUIContextualTasksHost) {
                number_of_active_tasks++;
              }
            }
          }
        }

        return true;
      });
#endif  // !BUILDFLAG(IS_ANDROID)
  return number_of_active_tasks;
}

}  // namespace

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

  // When sync is enabled, we should only set this to true for incognito and
  // guest sessions.
  bool supports_ephemeral_only = true;

  return std::make_unique<ContextualTasksServiceImpl>(
      chrome::GetChannel(),
      DataTypeStoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context))
          ->GetStoreFactory(),
      CreateCompositeContextDecorator(favicon_service, history_service,
                                      std::move(additional_decorators)),
      aim_eligibility_service, identity_manager, profile->GetPrefs(),
      supports_ephemeral_only,
      base::BindRepeating(&GetNumberOfActiveTasks, profile));
}

}  // namespace contextual_tasks
