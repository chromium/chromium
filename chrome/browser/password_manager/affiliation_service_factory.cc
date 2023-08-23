// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/affiliation_service_factory.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service_impl.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

AffiliationServiceFactory::AffiliationServiceFactory()
    : ProfileKeyedServiceFactory(
          "AffiliationService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AffiliationServiceFactory::~AffiliationServiceFactory() = default;

AffiliationServiceFactory* AffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<AffiliationServiceFactory> instance;
  return instance.get();
}

password_manager::AffiliationService* AffiliationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<password_manager::AffiliationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
AffiliationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  auto backend_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  auto affiliation_service =
          std::make_unique<password_manager::AffiliationServiceImpl>(
              url_loader_factory, backend_task_runner);

  base::FilePath database_path =
      profile->GetPath().Append(password_manager::kAffiliationDatabaseFileName);
  affiliation_service->Init(content::GetNetworkConnectionTracker(),
                            database_path);

  return affiliation_service;
}
