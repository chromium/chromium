// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_engagement_metrics_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"

namespace crostini {

constexpr char kUmaPrefix[] = "Crostini";

CrostiniEngagementMetricsService*
CrostiniEngagementMetricsService::Factory::GetForProfile(Profile* profile) {
  return static_cast<CrostiniEngagementMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CrostiniEngagementMetricsService::Factory*
CrostiniEngagementMetricsService::Factory::GetInstance() {
  static base::NoDestructor<CrostiniEngagementMetricsService::Factory> factory;
  return factory.get();
}

CrostiniEngagementMetricsService::Factory::Factory()
    : ProfileKeyedServiceFactory("CrostiniEngagementMetricsService") {}

CrostiniEngagementMetricsService::Factory::~Factory() = default;

KeyedService*
CrostiniEngagementMetricsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniEngagementMetricsService(profile);
}

bool CrostiniEngagementMetricsService::Factory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CrostiniEngagementMetricsService::Factory::ServiceIsNULLWhileTesting()
    const {
  // Checking whether Crostini is allowed requires more setup than is present
  // in most unit tests.
  return true;
}

CrostiniEngagementMetricsService::CrostiniEngagementMetricsService(
    Profile* profile) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile))
    return;
  guest_os_engagement_metrics_ =
      std::make_unique<guest_os::GuestOsEngagementMetrics>(
          profile->GetPrefs(), base::BindRepeating(IsCrostiniWindow),
          prefs::kEngagementPrefsPrefix, kUmaPrefix);
}

CrostiniEngagementMetricsService::~CrostiniEngagementMetricsService() = default;

void CrostiniEngagementMetricsService::SetBackgroundActive(
    bool background_active) {
  // If policy changes to enable Crostini, we won't have created the helper
  // object. This should be relatively rare so for now we don't track this
  // case.
  if (!guest_os_engagement_metrics_)
    return;
  guest_os_engagement_metrics_->SetBackgroundActive(background_active);
}

}  // namespace crostini
