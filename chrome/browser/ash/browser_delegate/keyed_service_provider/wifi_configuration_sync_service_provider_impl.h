// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_WIFI_CONFIGURATION_SYNC_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_WIFI_CONFIGURATION_SYNC_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service_provider.h"

class AccountId;

namespace ash {

namespace sync_wifi {
class WifiConfigurationSyncService;
}  // namespace sync_wifi

// //chrome-side implementation of WifiConfigurationSyncServiceProvider. Wraps
// //chrome/browser/sync's Profile-keyed WifiConfigurationSyncServiceFactory so
// ChromeOS callers can reach the service through the chromeos-side interface.
class WifiConfigurationSyncServiceProviderImpl
    : public WifiConfigurationSyncServiceProvider {
 public:
  WifiConfigurationSyncServiceProviderImpl();
  WifiConfigurationSyncServiceProviderImpl(
      const WifiConfigurationSyncServiceProviderImpl&) = delete;
  WifiConfigurationSyncServiceProviderImpl& operator=(
      const WifiConfigurationSyncServiceProviderImpl&) = delete;
  ~WifiConfigurationSyncServiceProviderImpl() override;

  // WifiConfigurationSyncServiceProvider:
  sync_wifi::WifiConfigurationSyncService* Find(
      const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_WIFI_CONFIGURATION_SYNC_SERVICE_PROVIDER_IMPL_H_
