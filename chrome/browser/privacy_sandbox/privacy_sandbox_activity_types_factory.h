// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_activity_types_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

class Profile;

class PrivacySandboxActivityTypesFactory : public ProfileKeyedServiceFactory {
 public:
  static PrivacySandboxActivityTypesFactory* GetInstance();
  static privacy_sandbox::PrivacySandboxActivityTypesService* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<PrivacySandboxActivityTypesFactory>;
  PrivacySandboxActivityTypesFactory();
  ~PrivacySandboxActivityTypesFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ACTIVITY_TYPES_FACTORY_H_
