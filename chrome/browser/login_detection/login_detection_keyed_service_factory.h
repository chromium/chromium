// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace login_detection {

class LoginDetectionKeyedService;

// LazyInstance that owns all LoginDetectionKeyedServices and associates them
// with Profiles.
class LoginDetectionKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the LoginDetectionService for the profile.
  //
  // Returns null if the LoginDetection feature flag is disabled.
  static LoginDetectionKeyedService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all LoginDetectionKeyedService(s).
  static LoginDetectionKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<LoginDetectionKeyedServiceFactory>;

  LoginDetectionKeyedServiceFactory();
  ~LoginDetectionKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace login_detection

#endif  //  CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_FACTORY_H_
