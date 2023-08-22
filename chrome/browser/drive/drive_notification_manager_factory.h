// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_NOTIFICATION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_DRIVE_DRIVE_NOTIFICATION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace drive {

class DriveNotificationManager;

// Singleton that owns all DriveNotificationManager and associates them with
// browser contexts.
class DriveNotificationManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the |DriveNotificationManager| for |context| if one exists or NULL
  // otherwise.
  static DriveNotificationManager* FindForBrowserContext(
      content::BrowserContext* context);

  // Returns the |DriveNotificationManager| for |context|, creating it first if
  // required.
  static DriveNotificationManager* GetForBrowserContext(
      content::BrowserContext* context);

  static DriveNotificationManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<DriveNotificationManagerFactory>;

  DriveNotificationManagerFactory();
  ~DriveNotificationManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_NOTIFICATION_MANAGER_FACTORY_H_
