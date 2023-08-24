// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class ChromePasswordProtectionService;

// Singleton that owns ChromePasswordProtectionService objects, one for each
// active Profile. It listens to profile destroy events and destroy its
// associated service. It returns a separate instance if the profile is in the
// Incognito mode.
class ChromePasswordProtectionServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static ChromePasswordProtectionService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static ChromePasswordProtectionServiceFactory* GetInstance();

  ChromePasswordProtectionServiceFactory(
      const ChromePasswordProtectionServiceFactory&) = delete;
  ChromePasswordProtectionServiceFactory& operator=(
      const ChromePasswordProtectionServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ChromePasswordProtectionServiceFactory>;

  ChromePasswordProtectionServiceFactory();
  ~ChromePasswordProtectionServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
