// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_APPS_PARENTAL_CONTROLS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_APPS_PARENTAL_CONTROLS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class OnDeviceAppsParentalControlsService;

class OnDeviceAppsParentalControlsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  OnDeviceAppsParentalControlsServiceFactory(
      const OnDeviceAppsParentalControlsServiceFactory&) = delete;
  OnDeviceAppsParentalControlsServiceFactory& operator=(
      const OnDeviceAppsParentalControlsServiceFactory&) = delete;

  // Returns singleton instance of OnDeviceAppsParentalControlsServiceFactory.
  static OnDeviceAppsParentalControlsServiceFactory* GetInstance();

  // Returns whether or not on-device apps parental controls is supported for
  // |context|.
  static bool IsOnDeviceAppsParentalControlsAvailable(
      content::BrowserContext* context);

  // Returns the OnDeviceAppsParentalControlsService associated with |context|.
  static OnDeviceAppsParentalControlsService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<OnDeviceAppsParentalControlsServiceFactory>;

  OnDeviceAppsParentalControlsServiceFactory();
  ~OnDeviceAppsParentalControlsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};
}  // namespace ash

#endif
