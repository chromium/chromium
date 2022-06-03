// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_remote_permission_service_factory.h"

#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
FlocRemotePermissionServiceFactory*
FlocRemotePermissionServiceFactory::GetInstance() {
  return base::Singleton<FlocRemotePermissionServiceFactory>::get();
}

// static
federated_learning::FlocRemotePermissionService*
FlocRemotePermissionServiceFactory::GetForProfile(Profile* profile) {

  return static_cast<federated_learning::FlocRemotePermissionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* FlocRemotePermissionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  return new federated_learning::FlocRemotePermissionService(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

FlocRemotePermissionServiceFactory::FlocRemotePermissionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FlocRemotePermissionServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

FlocRemotePermissionServiceFactory::~FlocRemotePermissionServiceFactory() =
    default;
