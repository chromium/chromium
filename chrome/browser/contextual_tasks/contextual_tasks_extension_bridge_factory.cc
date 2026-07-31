// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge_factory.h"

#include <memory>

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"

namespace contextual_tasks {

// static
ContextualTasksExtensionBridge*
ContextualTasksExtensionBridgeFactory::GetForProfile(Profile* profile) {
  return static_cast<ContextualTasksExtensionBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextualTasksExtensionBridgeFactory*
ContextualTasksExtensionBridgeFactory::GetInstance() {
  static base::NoDestructor<ContextualTasksExtensionBridgeFactory> instance;
  return instance.get();
}

ContextualTasksExtensionBridgeFactory::ContextualTasksExtensionBridgeFactory()
    : ProfileKeyedServiceFactory(
          "ContextualTasksExtensionBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::ExtensionMojoBinderRegistryFactory::GetInstance());
}

ContextualTasksExtensionBridgeFactory::
    ~ContextualTasksExtensionBridgeFactory() = default;

std::unique_ptr<KeyedService>
ContextualTasksExtensionBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ContextualTasksExtensionBridge>(
      Profile::FromBrowserContext(context));
}

bool ContextualTasksExtensionBridgeFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace contextual_tasks
