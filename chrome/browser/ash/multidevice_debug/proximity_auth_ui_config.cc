// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_debug/proximity_auth_ui_config.h"

#include "ash/webui/multidevice_debug/proximity_auth_ui.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace ash::multidevice {

namespace {

void BindMultiDeviceSetup(
    Profile* profile,
    mojo::PendingReceiver<multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  multidevice_setup::MultiDeviceSetupService* service =
      multidevice_setup::MultiDeviceSetupServiceFactory::GetForProfile(profile);
  if (service) {
    service->BindMultiDeviceSetup(std::move(receiver));
  }
}

}  // namespace

ProximityAuthUIConfig::ProximityAuthUIConfig()
    : WebUIConfig(content::kChromeUIScheme, kChromeUIProximityAuthHost) {}

bool ProximityAuthUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return false;
  }

  // Guest/incognito/signin profiles should not access chrome://proximity-auth.
  if (IsSigninBrowserContext(profile) || profile->IsOffTheRecord()) {
    return false;
  }

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  return device_sync::DeviceSyncClientFactory::GetForProfile(profile) !=
         nullptr;
}

std::unique_ptr<content::WebUIController>
ProximityAuthUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                             const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return std::make_unique<ProximityAuthUI>(
      web_ui, device_sync::DeviceSyncClientFactory::GetForProfile(profile),
      base::BindRepeating(&BindMultiDeviceSetup, profile));
}

}  // namespace ash::multidevice
