// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace privacy_sandbox {
class TrackingProtectionSettings;
}

class TrackingProtectionSettingsFactory : public ProfileKeyedServiceFactory {
 public:
  static TrackingProtectionSettingsFactory* GetInstance();
  static privacy_sandbox::TrackingProtectionSettings* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<TrackingProtectionSettingsFactory>;
  TrackingProtectionSettingsFactory();
  ~TrackingProtectionSettingsFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_FACTORY_H_
