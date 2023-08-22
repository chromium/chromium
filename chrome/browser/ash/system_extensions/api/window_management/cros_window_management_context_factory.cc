// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
CrosWindowManagementContext*
CrosWindowManagementContextFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<CrosWindowManagementContext*>(
      CrosWindowManagementContextFactory::GetInstance()
          .GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
CrosWindowManagementContextFactory&
CrosWindowManagementContextFactory::GetInstance() {
  static base::NoDestructor<CrosWindowManagementContextFactory> instance;
  return *instance;
}

CrosWindowManagementContextFactory::CrosWindowManagementContextFactory()
    : BrowserContextKeyedServiceFactory(
          "CrosWindowManagementContextFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(&SystemExtensionsProviderFactory::GetInstance());
}

CrosWindowManagementContextFactory::~CrosWindowManagementContextFactory() =
    default;

std::unique_ptr<KeyedService>
CrosWindowManagementContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<CrosWindowManagementContext>(
      Profile::FromBrowserContext(context));
}

bool CrosWindowManagementContextFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

content::BrowserContext*
CrosWindowManagementContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetProfileForSystemExtensions(Profile::FromBrowserContext(context));
}

}  // namespace ash
