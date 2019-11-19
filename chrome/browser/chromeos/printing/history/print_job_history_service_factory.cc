// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_database_impl.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

namespace chromeos {

// static
PrintJobHistoryService* PrintJobHistoryServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintJobHistoryService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintJobHistoryServiceFactory* PrintJobHistoryServiceFactory::GetInstance() {
  return base::Singleton<PrintJobHistoryServiceFactory>::get();
}

PrintJobHistoryServiceFactory::PrintJobHistoryServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrintJobHistoryService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::CupsPrintJobManagerFactory::GetInstance());
}

PrintJobHistoryServiceFactory::~PrintJobHistoryServiceFactory() {}

KeyedService* PrintJobHistoryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // We do not want an instance of PrintJobHistory on the lock screen.  The
  // result is multiple print job notifications. https://crbug.com/1011532
  if (ProfileHelper::IsLockScreenAppProfile(profile) ||
      ProfileHelper::IsSigninProfile(profile)) {
    return nullptr;
  }

  leveldb_proto::ProtoDatabaseProvider* database_provider =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetProtoDatabaseProvider();

  std::unique_ptr<PrintJobDatabase> print_job_database =
      std::make_unique<PrintJobDatabaseImpl>(database_provider,
                                             profile->GetPath());
  CupsPrintJobManager* print_job_manager =
      chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);

  return new PrintJobHistoryServiceImpl(std::move(print_job_database),
                                        print_job_manager, profile->GetPrefs());
}

}  // namespace chromeos
