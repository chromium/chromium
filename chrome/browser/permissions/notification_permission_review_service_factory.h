// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/permissions/notification_permission_review_service.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

class NotificationPermissionsReviewServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NotificationPermissionsReviewServiceFactory* GetInstance();

  static permissions::NotificationPermissionsReviewService* GetForProfile(
      Profile* profile);

  // Non-copyable, non-moveable.
  NotificationPermissionsReviewServiceFactory(
      const NotificationPermissionsReviewServiceFactory&) = delete;
  NotificationPermissionsReviewServiceFactory& operator=(
      const NotificationPermissionsReviewServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      NotificationPermissionsReviewServiceFactory>;

  NotificationPermissionsReviewServiceFactory();
  ~NotificationPermissionsReviewServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_FACTORY_H_
