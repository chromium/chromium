// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map_factory.h"

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace arc {

// static
ArcDocumentsProviderRootMap*
ArcDocumentsProviderRootMapFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcDocumentsProviderRootMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

ArcDocumentsProviderRootMapFactory::ArcDocumentsProviderRootMapFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcDocumentsProviderRootMap",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcFileSystemOperationRunner::GetFactory());
}

ArcDocumentsProviderRootMapFactory::~ArcDocumentsProviderRootMapFactory() =
    default;

// static
ArcDocumentsProviderRootMapFactory*
ArcDocumentsProviderRootMapFactory::GetInstance() {
  return base::Singleton<ArcDocumentsProviderRootMapFactory>::get();
}

content::BrowserContext*
ArcDocumentsProviderRootMapFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Allow accessing ArcDocumentsProvider files in incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ArcDocumentsProviderRootMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* arc_service_manager = ArcServiceManager::Get();

  // Practically, this is in testing case.
  if (!arc_service_manager) {
    VLOG(2) << "ArcServiceManager is not available.";
    return nullptr;
  }

  if (arc_service_manager->browser_context() != context) {
    VLOG(2) << "Non ARC allowed browser context.";
    return nullptr;
  }

  return new ArcDocumentsProviderRootMap(Profile::FromBrowserContext(context));
}

}  // namespace arc
