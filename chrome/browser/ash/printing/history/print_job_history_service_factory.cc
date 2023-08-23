// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_database_impl.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"
#include "chrome/browser/ash/printing/history/print_job_reporting_service.h"
#include "chrome/browser/ash/printing/history/print_job_reporting_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/storage_partition.h"

namespace ash {

// static
PrintJobHistoryService* PrintJobHistoryServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrintJobHistoryService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrintJobHistoryServiceFactory* PrintJobHistoryServiceFactory::GetInstance() {
  static base::NoDestructor<PrintJobHistoryServiceFactory> instance;
  return instance.get();
}

PrintJobHistoryServiceFactory::PrintJobHistoryServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrintJobHistoryService",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOriginalOnly)
              // We do not want an instance of PrintJobHistory on the lock
              // screen.  The result is multiple print job notifications.
              // https://crbug.com/1011532
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(CupsPrintJobManagerFactory::GetInstance());
  DependsOn(PrintJobReportingServiceFactory::GetInstance());
}

PrintJobHistoryServiceFactory::~PrintJobHistoryServiceFactory() = default;

std::unique_ptr<KeyedService>
PrintJobHistoryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  leveldb_proto::ProtoDatabaseProvider* database_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  auto print_job_database = std::make_unique<PrintJobDatabaseImpl>(
      database_provider, profile->GetPath());
  CupsPrintJobManager* print_job_manager =
      CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  PrintJobReportingService* print_job_reporting_service =
      PrintJobReportingServiceFactory::GetForBrowserContext(profile);

  std::unique_ptr<PrintJobHistoryServiceImpl> history_service = 
    std::make_unique<PrintJobHistoryServiceImpl>(
      std::move(print_job_database), print_job_manager, profile->GetPrefs());
  // Service is null in tests.
  if (print_job_reporting_service) {
    history_service->AddObserver(print_job_reporting_service);
  }
  return history_service;
}

void PrintJobHistoryServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  PrintJobHistoryService::RegisterProfilePrefs(user_prefs);
}

}  // namespace ash
