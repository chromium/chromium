// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BackgroundContentsService;
class Profile;

// Singleton that owns all BackgroundContentsServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated BackgroundContentsService.
class BackgroundContentsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BackgroundContentsService* GetForProfile(Profile* profile);

  static BackgroundContentsServiceFactory* GetInstance();

  BackgroundContentsServiceFactory(const BackgroundContentsServiceFactory&) =
      delete;
  BackgroundContentsServiceFactory& operator=(
      const BackgroundContentsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BackgroundContentsServiceFactory>;

  BackgroundContentsServiceFactory();
  ~BackgroundContentsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_FACTORY_H_
