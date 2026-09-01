// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/wifi_configuration_sync_service_provider_impl.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_context.h"

namespace ash {

WifiConfigurationSyncServiceProviderImpl::
    WifiConfigurationSyncServiceProviderImpl() = default;

WifiConfigurationSyncServiceProviderImpl::
    ~WifiConfigurationSyncServiceProviderImpl() = default;

sync_wifi::WifiConfigurationSyncService*
WifiConfigurationSyncServiceProviderImpl::Find(const AccountId& account_id) {
  content::BrowserContext* context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  // Callers resolve a live profile for `account_id` before calling Find(), so a
  // browser context always exists here.
  CHECK(context);
  return WifiConfigurationSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context), /*create=*/false);
}

}  // namespace ash
