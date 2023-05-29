// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BackgroundSyncControllerImpl;
class Profile;

class BackgroundSyncControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static BackgroundSyncControllerImpl* GetForProfile(Profile* profile);
  static BackgroundSyncControllerFactory* GetInstance();

  BackgroundSyncControllerFactory(const BackgroundSyncControllerFactory&) =
      delete;
  BackgroundSyncControllerFactory& operator=(
      const BackgroundSyncControllerFactory&) = delete;

 private:
  friend base::NoDestructor<BackgroundSyncControllerFactory>;

  BackgroundSyncControllerFactory();
  ~BackgroundSyncControllerFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_
