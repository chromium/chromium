// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace notifications {
class NotificationScheduleService;
}  // namespace notifications

class NotificationScheduleServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static NotificationScheduleServiceFactory* GetInstance();
  static notifications::NotificationScheduleService* GetForKey(
      SimpleFactoryKey* key);

 private:
  friend base::NoDestructor<NotificationScheduleServiceFactory>;

  NotificationScheduleServiceFactory();
  NotificationScheduleServiceFactory(
      const NotificationScheduleServiceFactory&) = delete;
  NotificationScheduleServiceFactory& operator=(
      const NotificationScheduleServiceFactory&) = delete;
  ~NotificationScheduleServiceFactory() override;

  // SimpleKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_FACTORY_H_
