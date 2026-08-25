// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/isolated_mode/isolated_mode_settings_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/isolated_mode/isolated_mode_settings_service.h"

namespace enterprise_isolated_mode {

// static
IsolatedModeSettingsServiceFactory*
IsolatedModeSettingsServiceFactory::GetInstance() {
  static base::NoDestructor<IsolatedModeSettingsServiceFactory> instance;
  return instance.get();
}

// static
IsolatedModeSettingsService* IsolatedModeSettingsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IsolatedModeSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

IsolatedModeSettingsServiceFactory::IsolatedModeSettingsServiceFactory()
    : ProfileKeyedServiceFactory(
          "IsolatedModeSettingsService",
          ProfileSelections::BuildRedirectedInIncognito()) {}

IsolatedModeSettingsServiceFactory::~IsolatedModeSettingsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IsolatedModeSettingsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<IsolatedModeSettingsService>(profile->GetPrefs(),
                                                       chrome::GetChannel());
}

bool IsolatedModeReplacesIncognito(const Profile* profile) {
  if (!profile) {
    return false;
  }
  auto* service = IsolatedModeSettingsServiceFactory::GetForProfile(
      const_cast<Profile*>(profile));
  return service && service->ReplacesIncognito();
}

}  // namespace enterprise_isolated_mode
