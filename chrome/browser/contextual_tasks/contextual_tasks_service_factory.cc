// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "content/public/browser/browser_context.h"

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
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

ContextualTasksServiceFactory::~ContextualTasksServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextualTasksServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ContextualTasksServiceImpl>();
}

}  // namespace contextual_tasks
