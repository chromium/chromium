// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_engagement_metrics_service.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "PluginVmEngagementMetricsService",
          BrowserContextDependencyManager::GetInstance()) {}

PluginVmEngagementMetricsService::Factory::~Factory() = default;

KeyedService*
PluginVmEngagementMetricsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PluginVmEngagementMetricsService(profile);
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
  if (!IsPluginVmAllowedForProfile(profile))
    return;
  guest_os_engagement_metrics_ =
      std::make_unique<guest_os::GuestOsEngagementMetrics>(
          profile->GetPrefs(), base::BindRepeating(IsPluginVmWindow),
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
