// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class TailoredSecurityService;

// Used for creating and fetching a per-profile instance of the
// TailoredSecurityService.
class TailoredSecurityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Get the singleton instance of the factory.
  static TailoredSecurityServiceFactory* GetInstance();

  // Get the TailoredSecurityService for |profile|, creating one if needed.
  static safe_browsing::TailoredSecurityService* GetForProfile(
      Profile* profile);

 protected:
  // Overridden from BrowserContextKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<TailoredSecurityServiceFactory>;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  TailoredSecurityServiceFactory();
  ~TailoredSecurityServiceFactory() override = default;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
