// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/permissions/bluetooth_delegate_impl.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE) && BUILDFLAG(ENABLE_GUEST_VIEW)

ChromeBluetoothDelegate::ChromeBluetoothDelegate(std::unique_ptr<Client> client)
    : permissions::BluetoothDelegateImpl(std::move(client)) {}

bool ChromeBluetoothDelegate::MayUseBluetooth(content::RenderFrameHost* rfh) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && BUILDFLAG(ENABLE_GUEST_VIEW)
  // Because permission is scoped to profile, <webview> and <controlledframe>,
  // despite having isolated StoragePartition, will share bluetooth permission
  // with the rest of the profile. Therefore bluetooth is not allowed in these
  // contexts.
  if (extensions::WebViewGuest::FromRenderFrameHost(rfh)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE) && BUILDFLAG(ENABLE_GUEST_VIEW)

  // Disable any other non-default StoragePartition contexts, unless it has a
  // non-http/https scheme.
  if (rfh->GetStoragePartition() !=
      rfh->GetBrowserContext()->GetDefaultStoragePartition()) {
    return !rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

ChromeBluetoothDelegate::AllowWebBluetoothResult
ChromeBluetoothDelegate::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  // TODO(crbug.com/40462828): Don't disable if
  // base::CommandLine::ForCurrentProcess()->
  // HasSwitch(switches::kEnableWebBluetooth) is true.
  if (base::GetFieldTrialParamValue(
          permissions::ContentSettingPermissionContextBase::
              kPermissionsKillSwitchFieldStudy,
          "Bluetooth") == permissions::ContentSettingPermissionContextBase::
                              kPermissionsKillSwitchBlockedValue) {
    // The kill switch is enabled for this permission. Block requests.
    return AllowWebBluetoothResult::kBlockGloballyDisabled;
  }

  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_GUARD) == CONTENT_SETTING_BLOCK) {
    return AllowWebBluetoothResult::kBlockPolicy;
  }
  return AllowWebBluetoothResult::kAllow;
}

std::string ChromeBluetoothDelegate::GetWebBluetoothBlocklist() {
  return base::GetFieldTrialParamValue("WebBluetoothBlocklist",
                                       "blocklist_additions");
}

bool ChromeBluetoothDelegate::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_SCANNING) == CONTENT_SETTING_BLOCK) {
    return true;
  }
  return false;
}

void ChromeBluetoothDelegate::BlockBluetoothScanning(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  content_settings->SetContentSettingDefaultScope(
      requesting_origin.GetURL(), embedding_origin.GetURL(),
      ContentSettingsType::BLUETOOTH_SCANNING, CONTENT_SETTING_BLOCK);
}
