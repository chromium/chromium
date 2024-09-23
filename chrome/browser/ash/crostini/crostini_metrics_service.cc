// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_metrics_service.h"

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/exo/wm_helper.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/wm/public/activation_client.h"

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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CrostiniMetricsService::Factory::~Factory() = default;

std::unique_ptr<KeyedService>
CrostiniMetricsService::Factory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<CrostiniMetricsService>(profile);
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

  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
  }
}

CrostiniMetricsService::~CrostiniMetricsService() {
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
  }
}

void CrostiniMetricsService::SetBackgroundActive(bool background_active) {
  // If policy changes to enable Crostini, we won't have created the helper
  // object. This should be relatively rare so for now we don't track this
  // case.
  if (!guest_os_engagement_metrics_) {
    return;
  }
  guest_os_engagement_metrics_->SetBackgroundActive(background_active);
}

void CrostiniMetricsService::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (lost_active && IsCrostiniWindow(lost_active)) {
    // Log the current input method when a Crostini window loses focus. This is
    // a simple way for us to record which input method are being used with
    // Crostini, and doesn't require us to hook into keyboard events. Enum
    // format matches InputMethod.ID2.

    auto* imm = ash::input_method::InputMethodManager::Get();
    if (imm && imm->GetActiveIMEState()) {
      base::UmaHistogramSparse(
          "Crostini.InputMethodOnBlur",
          static_cast<int32_t>(base::PersistentHash(
              imm->GetActiveIMEState()->GetCurrentInputMethod().id())));
    }
  }
}

}  // namespace crostini
