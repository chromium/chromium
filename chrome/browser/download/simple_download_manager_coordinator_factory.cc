// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"

namespace {
void DownloadUrl(std::unique_ptr<download::DownloadUrlParameters> parameters,
                 Profile* profile) {
  content::DownloadManager* manager = profile->GetDownloadManager();
  DCHECK(manager);
  manager->DownloadUrl(std::move(parameters));
}

void DownloadUrlWithDownloadManager(
    SimpleFactoryKey* key,
    std::unique_ptr<download::DownloadUrlParameters> parameters) {
  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      key, base::BindOnce(&DownloadUrl, std::move(parameters)));
}
}  // namespace

// static
SimpleDownloadManagerCoordinatorFactory*
SimpleDownloadManagerCoordinatorFactory::GetInstance() {
  static base::NoDestructor<SimpleDownloadManagerCoordinatorFactory> instance;
  return instance.get();
}

// static
download::SimpleDownloadManagerCoordinator*
SimpleDownloadManagerCoordinatorFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<download::SimpleDownloadManagerCoordinator*>(
      GetInstance()->GetServiceForKey(key, true));
}

SimpleDownloadManagerCoordinatorFactory::
    SimpleDownloadManagerCoordinatorFactory()
    : SimpleKeyedServiceFactory("SimpleDownloadManagerCoordinator",
                                SimpleDependencyManager::GetInstance()) {}

SimpleDownloadManagerCoordinatorFactory::
    ~SimpleDownloadManagerCoordinatorFactory() = default;

std::unique_ptr<KeyedService>
SimpleDownloadManagerCoordinatorFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  // Use unretained is safe as the key is associated with the callback.
  return std::make_unique<download::SimpleDownloadManagerCoordinator>(
      base::BindRepeating(&DownloadUrlWithDownloadManager,
                          base::Unretained(key)));
}

SimpleFactoryKey* SimpleDownloadManagerCoordinatorFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}
