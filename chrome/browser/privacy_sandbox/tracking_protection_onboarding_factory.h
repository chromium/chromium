// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_FACTORY_H_

#include <memory>
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

class Profile;

class TrackingProtectionOnboardingFactory : public ProfileKeyedServiceFactory {
 public:
  static TrackingProtectionOnboardingFactory* GetInstance();
  static privacy_sandbox::TrackingProtectionOnboarding* GetForProfile(
      Profile* profile);

 private:
  friend base::NoDestructor<TrackingProtectionOnboardingFactory>;
  TrackingProtectionOnboardingFactory();
  ~TrackingProtectionOnboardingFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_FACTORY_H_
