// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"

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

// TODO(452127302): Support guest profiles when our dependencies do.
ContextualTasksUiServiceFactory::ContextualTasksUiServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextualTasksUiService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ContextualTasksContextControllerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ContextualTasksUiServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ContextualTasksUiService>();
}

bool ContextualTasksUiServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ContextualTasksUiServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
}  // namespace contextual_tasks
