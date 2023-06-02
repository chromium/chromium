// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_NOTIFICATIONS_ENGAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_NOTIFICATIONS_ENGAGEMENT_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/permissions/notifications_engagement_service.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class NotificationsEngagementServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static NotificationsEngagementServiceFactory* GetInstance();

  static permissions::NotificationsEngagementService* GetForProfile(
      Profile* profile);

  // Non-copyable, non-moveable.
  NotificationsEngagementServiceFactory(
      const NotificationsEngagementServiceFactory&) = delete;
  NotificationsEngagementServiceFactory& operator=(
      const NotificationsEngagementServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NotificationsEngagementServiceFactory>;

  NotificationsEngagementServiceFactory();
  ~NotificationsEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_NOTIFICATIONS_ENGAGEMENT_SERVICE_FACTORY_H_
