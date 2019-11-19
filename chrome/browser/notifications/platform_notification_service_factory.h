// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PlatformNotificationServiceImpl;
class Profile;

class PlatformNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PlatformNotificationServiceImpl* GetForProfile(Profile* profile);
  static PlatformNotificationServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      PlatformNotificationServiceFactory>;

  PlatformNotificationServiceFactory();

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationServiceFactory);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_FACTORY_H_
