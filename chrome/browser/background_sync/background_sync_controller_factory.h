// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BackgroundSyncControllerImpl;
class Profile;

class BackgroundSyncControllerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BackgroundSyncControllerImpl* GetForProfile(Profile* profile);
  static BackgroundSyncControllerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<BackgroundSyncControllerFactory>;

  BackgroundSyncControllerFactory();
  ~BackgroundSyncControllerFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncControllerFactory);
};

#endif  // CHROME_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_
