// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_engagement_metrics_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"

namespace plugin_vm {

constexpr char kUmaPrefix[] = "PluginVm";

PluginVmEngagementMetricsService*
PluginVmEngagementMetricsService::Factory::GetForProfile(Profile* profile) {
  return static_cast<PluginVmEngagementMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PluginVmEngagementMetricsService::Factory*
PluginVmEngagementMetricsService::Factory::GetInstance() {
  static base::NoDestructor<PluginVmEngagementMetricsService::Factory> factory;
  return factory.get();
}

PluginVmEngagementMetricsService::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "PluginVmEngagementMetricsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

PluginVmEngagementMetricsService::Factory::~Factory() = default;

std::unique_ptr<KeyedService> PluginVmEngagementMetricsService::Factory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PluginVmEngagementMetricsService>(profile);
}

bool PluginVmEngagementMetricsService::Factory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PluginVmEngagementMetricsService::Factory::ServiceIsNULLWhileTesting()
    const {
  // Checking whether Plugin VM is allowed requires more setup than is present
  // in most unit tests.
  return true;
}

PluginVmEngagementMetricsService::PluginVmEngagementMetricsService(
    Profile* profile) {
  if (!PluginVmFeatures::Get()->IsAllowed(profile))
    return;
  guest_os_engagement_metrics_ =
      std::make_unique<guest_os::GuestOsEngagementMetrics>(
          profile->GetPrefs(), base::BindRepeating(IsPluginVmAppWindow),
          prefs::kEngagementPrefsPrefix, kUmaPrefix);
}

PluginVmEngagementMetricsService::~PluginVmEngagementMetricsService() = default;

void PluginVmEngagementMetricsService::SetBackgroundActive(
    bool background_active) {
  // If policy changes to enable Plugin VM, we won't have created the helper
  // object. This should be relatively rare so for now we don't track this
  // case.
  if (!guest_os_engagement_metrics_)
    return;
  guest_os_engagement_metrics_->SetBackgroundActive(background_active);
}

}  // namespace plugin_vm
