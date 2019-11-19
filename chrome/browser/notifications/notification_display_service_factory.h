// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class NotificationDisplayService;
class Profile;

class NotificationDisplayServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NotificationDisplayService* GetForProfile(Profile* profile);
  static NotificationDisplayServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NotificationDisplayServiceFactory>;

  NotificationDisplayServiceFactory();

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayServiceFactory);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_
