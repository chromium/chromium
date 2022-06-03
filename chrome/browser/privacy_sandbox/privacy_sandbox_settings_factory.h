// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PrivacySandboxSettings;
class Profile;

class PrivacySandboxSettingsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrivacySandboxSettingsFactory* GetInstance();
  static PrivacySandboxSettings* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<PrivacySandboxSettingsFactory>;
  PrivacySandboxSettingsFactory();
  ~PrivacySandboxSettingsFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_FACTORY_H_
