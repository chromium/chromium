// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/storage/storage_notification_service_impl.h"

class StorageNotificationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static StorageNotificationServiceImpl* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static StorageNotificationServiceFactory* GetInstance();

  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* browser_context);

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<StorageNotificationServiceFactory>;

  StorageNotificationServiceFactory();
  ~StorageNotificationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

#endif  // CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_FACTORY_H_
