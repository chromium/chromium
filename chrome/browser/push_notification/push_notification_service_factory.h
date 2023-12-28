// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace push_notification {

class PushNotificationService;

class PushNotificationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  PushNotificationServiceFactory(const PushNotificationServiceFactory&) =
      delete;
  PushNotificationServiceFactory& operator=(
      const PushNotificationServiceFactory&) = delete;

  static PushNotificationServiceFactory* GetInstance();

  static PushNotificationService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<PushNotificationServiceFactory>;

  PushNotificationServiceFactory();
  ~PushNotificationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace push_notification

#endif  // CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_FACTORY_H_
