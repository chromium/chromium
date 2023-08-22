// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace privacy_sandbox {
class PrivacySandboxSettings;
}

class PrivacySandboxSettingsFactory : public ProfileKeyedServiceFactory {
 public:
  static PrivacySandboxSettingsFactory* GetInstance();
  static privacy_sandbox::PrivacySandboxSettings* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PrivacySandboxSettingsFactory>;
  PrivacySandboxSettingsFactory();
  ~PrivacySandboxSettingsFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_FACTORY_H_
