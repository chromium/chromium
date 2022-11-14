// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"

#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {
namespace printing {
namespace print_management {

// static
PrintingManager* PrintingManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PrintingManager*>(
      PrintingManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
PrintingManagerFactory* PrintingManagerFactory::GetInstance() {
  return base::Singleton<PrintingManagerFactory>::get();
}

PrintingManagerFactory::PrintingManagerFactory()
    : ProfileKeyedServiceFactory(
          "PrintingManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // We do not want an instance of PrintingManager on the lock
              // screen. The result is multiple print job notifications.
              // https://crbug.com/1011532
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(PrintJobHistoryServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(CupsPrintJobManagerFactory::GetInstance());
}

PrintingManagerFactory::~PrintingManagerFactory() = default;

// static
KeyedService* PrintingManagerFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PrintingManager(
      PrintJobHistoryServiceFactory::GetForBrowserContext(context),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      CupsPrintJobManagerFactory::GetForBrowserContext(context),
      profile->GetPrefs());
}

KeyedService* PrintingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

void PrintingManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kDeletePrintJobHistoryAllowed, true);
}

bool PrintingManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PrintingManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace print_management
}  // namespace printing
}  // namespace ash
