// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_metrics_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"

namespace crostini {

constexpr char kUmaPrefix[] = "Crostini";

CrostiniMetricsService* CrostiniMetricsService::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<CrostiniMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CrostiniMetricsService::Factory*
CrostiniMetricsService::Factory::GetInstance() {
  static base::NoDestructor<CrostiniMetricsService::Factory> factory;
  return factory.get();
}

CrostiniMetricsService::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "CrostiniMetricsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

CrostiniMetricsService::Factory::~Factory() = default;

KeyedService* CrostiniMetricsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniMetricsService(profile);
}

bool CrostiniMetricsService::Factory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool CrostiniMetricsService::Factory::ServiceIsNULLWhileTesting() const {
  // Checking whether Crostini is allowed requires more setup than is present
  // in most unit tests.
  return true;
}

CrostiniMetricsService::CrostiniMetricsService(Profile* profile) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile)) {
    return;
  }
  guest_os_engagement_metrics_ =
      std::make_unique<guest_os::GuestOsEngagementMetrics>(
          profile->GetPrefs(), base::BindRepeating(IsCrostiniWindow),
          prefs::kEngagementPrefsPrefix, kUmaPrefix);
}

CrostiniMetricsService::~CrostiniMetricsService() = default;

void CrostiniMetricsService::SetBackgroundActive(bool background_active) {
  // If policy changes to enable Crostini, we won't have created the helper
  // object. This should be relatively rare so for now we don't track this
  // case.
  if (!guest_os_engagement_metrics_) {
    return;
  }
  guest_os_engagement_metrics_->SetBackgroundActive(background_active);
}

}  // namespace crostini
