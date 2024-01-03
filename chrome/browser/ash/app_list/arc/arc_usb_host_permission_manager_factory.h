// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace arc {

class ArcUsbHostPermissionManager;

class ArcUsbHostPermissionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  ArcUsbHostPermissionManagerFactory(
      const ArcUsbHostPermissionManagerFactory&) = delete;
  ArcUsbHostPermissionManagerFactory& operator=(
      const ArcUsbHostPermissionManagerFactory&) = delete;

  static ArcUsbHostPermissionManager* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcUsbHostPermissionManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<ArcUsbHostPermissionManagerFactory>;

  ArcUsbHostPermissionManagerFactory();
  ~ArcUsbHostPermissionManagerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_FACTORY_H_
