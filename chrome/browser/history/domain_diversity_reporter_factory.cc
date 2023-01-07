// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/domain_diversity_reporter_factory.h"

#include <string>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/metrics/domain_diversity_reporter.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

// static
DomainDiversityReporter* DomainDiversityReporterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DomainDiversityReporter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DomainDiversityReporterFactory* DomainDiversityReporterFactory::GetInstance() {
  static base::NoDestructor<DomainDiversityReporterFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService> DomainDiversityReporterFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  // Only build DomainDiversityReporter service with a valid |history_service|.
  if (!history_service)
    return nullptr;

  return std::make_unique<DomainDiversityReporter>(
      history_service, profile->GetPrefs(), base::DefaultClock::GetInstance());
}

DomainDiversityReporterFactory::DomainDiversityReporterFactory()
    : ProfileKeyedServiceFactory(
          "DomainDiversityReporter",
          // Incognito profiles share the HistoryService of the original
          // profile, so no need for an instance for them. Guest and system
          // profiles are not representative (guest in particular is transient)
          // and not reported.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // are not representative and should not have the reporter created
              // for.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

DomainDiversityReporterFactory::~DomainDiversityReporterFactory() = default;

KeyedService* DomainDiversityReporterFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile)).release();
}

void DomainDiversityReporterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DomainDiversityReporter::RegisterProfilePrefs(registry);
}

bool DomainDiversityReporterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool DomainDiversityReporterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
