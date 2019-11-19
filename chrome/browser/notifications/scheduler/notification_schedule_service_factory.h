// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace notifications {
class NotificationScheduleService;
}  // namespace notifications

class NotificationScheduleServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NotificationScheduleServiceFactory* GetInstance();
  static notifications::NotificationScheduleService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<NotificationScheduleServiceFactory>;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  NotificationScheduleServiceFactory();
  ~NotificationScheduleServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleServiceFactory);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_
