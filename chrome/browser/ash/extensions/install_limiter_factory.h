// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace extensions {

class InstallLimiter;

// Singleton that owns all InstallLimiter and associates them with profiles.
class InstallLimiterFactory : public ProfileKeyedServiceFactory {
 public:
  static InstallLimiter* GetForProfile(Profile* profile);

  static InstallLimiterFactory* GetInstance();

  InstallLimiterFactory(const InstallLimiterFactory&) = delete;
  InstallLimiterFactory& operator=(const InstallLimiterFactory&) = delete;

 private:
  friend base::NoDestructor<InstallLimiterFactory>;

  InstallLimiterFactory();
  ~InstallLimiterFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_INSTALL_LIMITER_FACTORY_H_
