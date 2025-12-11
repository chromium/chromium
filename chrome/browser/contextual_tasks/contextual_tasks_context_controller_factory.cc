// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"

namespace contextual_tasks {

// static
ContextualTasksContextController*
ContextualTasksContextControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<ContextualTasksContextController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextualTasksContextControllerFactory*
ContextualTasksContextControllerFactory::GetInstance() {
  static base::NoDestructor<ContextualTasksContextControllerFactory> instance;
  return instance.get();
}

ContextualTasksContextControllerFactory::
    ContextualTasksContextControllerFactory()
    : ProfileKeyedServiceFactory(
          "ContextualTasksContextController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ContextualTasksServiceFactory::GetInstance());
}

ContextualTasksContextControllerFactory::
    ~ContextualTasksContextControllerFactory() = default;

std::unique_ptr<KeyedService>
ContextualTasksContextControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kContextualTasks)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(profile);
  if (!contextual_tasks_service) {
    return nullptr;
  }
  return std::make_unique<ContextualTasksContextControllerImpl>(
      profile, contextual_tasks_service);
}

}  // namespace contextual_tasks
