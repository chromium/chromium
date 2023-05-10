// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager_factory.h"

#include "ash/components/arc/usb/usb_host_bridge.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager.h"
#include "content/public/browser/browser_context.h"

namespace arc {

// static
ArcUsbHostPermissionManager*
ArcUsbHostPermissionManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcUsbHostPermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ArcUsbHostPermissionManagerFactory*
ArcUsbHostPermissionManagerFactory::GetInstance() {
  return base::Singleton<ArcUsbHostPermissionManagerFactory>::get();
}

ArcUsbHostPermissionManagerFactory::ArcUsbHostPermissionManagerFactory()
    : ProfileKeyedServiceFactory(
          "ArcUsbHostPermissionManager",
          // This matches the logic in ExtensionSyncServiceFactory, which uses
          // the original browser context.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(ArcUsbHostBridge::GetFactory());
}

ArcUsbHostPermissionManagerFactory::~ArcUsbHostPermissionManagerFactory() {}

KeyedService* ArcUsbHostPermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return ArcUsbHostPermissionManager::Create(context);
}

}  // namespace arc
