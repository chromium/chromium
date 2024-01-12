// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/file_change_service_bridge_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/lacros/file_change_service_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

FileChangeServiceBridgeFactory::FileChangeServiceBridgeFactory()
    : ProfileKeyedServiceFactory(
          "FileChangeServiceBridge",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(FileSystemAccessPermissionContextFactory::GetInstance());
}

FileChangeServiceBridgeFactory::~FileChangeServiceBridgeFactory() = default;

// static
FileChangeServiceBridgeFactory* FileChangeServiceBridgeFactory::GetInstance() {
  static base::NoDestructor<FileChangeServiceBridgeFactory> kInstance;
  return kInstance.get();
}

std::unique_ptr<KeyedService>
FileChangeServiceBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FileChangeServiceBridge>(
      Profile::FromBrowserContext(context));
}

bool FileChangeServiceBridgeFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
