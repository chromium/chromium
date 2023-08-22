// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

class InstantService;
class Profile;

// Singleton that owns all InstantServices and associates them with Profiles.
class InstantServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the InstantService for |profile|.
  static InstantService* GetForProfile(Profile* profile);

  static InstantServiceFactory* GetInstance();

  InstantServiceFactory(const InstantServiceFactory&) = delete;
  InstantServiceFactory& operator=(const InstantServiceFactory&) = delete;

 private:
  friend base::NoDestructor<InstantServiceFactory>;

  InstantServiceFactory();
  ~InstantServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void BrowserContextDestroyed(
      content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_FACTORY_H_
