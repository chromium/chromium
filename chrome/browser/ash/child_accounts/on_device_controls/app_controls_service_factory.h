// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash::on_device_controls {

class AppControlsService;

class AppControlsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  AppControlsServiceFactory(const AppControlsServiceFactory&) = delete;
  AppControlsServiceFactory& operator=(const AppControlsServiceFactory&) =
      delete;

  // Returns singleton instance of `AppControlsServiceFactory`.
  static AppControlsServiceFactory* GetInstance();

  // Returns whether or not on-device apps parental controls is supported for
  // `context`.
  static bool IsOnDeviceAppControlsAvailable(content::BrowserContext* context);

  // Returns the `AppControlsService` associated with `context`.
  static AppControlsService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<AppControlsServiceFactory>;

  AppControlsServiceFactory();
  ~AppControlsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_SERVICE_FACTORY_H_
