// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map_factory.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

// static
ArcDocumentsProviderRootMap*
ArcDocumentsProviderRootMapFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcDocumentsProviderRootMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

ArcDocumentsProviderRootMapFactory::ArcDocumentsProviderRootMapFactory()
    : ProfileKeyedServiceFactory(
          "ArcDocumentsProviderRootMap",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ArcFileSystemOperationRunner::GetFactory());
}

ArcDocumentsProviderRootMapFactory::~ArcDocumentsProviderRootMapFactory() =
    default;

// static
ArcDocumentsProviderRootMapFactory*
ArcDocumentsProviderRootMapFactory::GetInstance() {
  static base::NoDestructor<ArcDocumentsProviderRootMapFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
ArcDocumentsProviderRootMapFactory::BuildServiceInstanceForBrowserContext(
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

  return std::make_unique<ArcDocumentsProviderRootMap>(
      Profile::FromBrowserContext(context));
}

}  // namespace arc
