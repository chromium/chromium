// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PrivacySandboxService;
class Profile;

class PrivacySandboxServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrivacySandboxServiceFactory* GetInstance();
  static PrivacySandboxService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<PrivacySandboxServiceFactory>;
  PrivacySandboxServiceFactory();
  ~PrivacySandboxServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_FACTORY_H_
