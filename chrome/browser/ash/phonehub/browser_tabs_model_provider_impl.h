// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_IMPL_H_

#include <ostream>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {
namespace phonehub {

// Gets the browser tab model info by finding a SyncedSession (provided lazily
// by a SessionService) with a |session_name| that matches the |pii_free_name|
// of the phone provided by a MultiDeviceSetupClient. If sync is enabled, the
// class uses a BrowserTabsMetadataFetcher to actually fetch the browser tab
// metadata once it finds the correct SyncedSession.
//
// Uses a SyncService in TriggerRefresh() to manually request updates for the
// latest SyncedSessions. If updated SyncSessions exist on the server, all
// SessionSyncService subscriptions will be updated almost immediately, instead
// of being lazily updated and eventually consistent with the latest browser tab
// info on the server.
class BrowserTabsModelProviderImpl
    : public BrowserTabsModelProvider,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  BrowserTabsModelProviderImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      syncer::SyncService* sync_service,
      sync_sessions::SessionSyncService* session_sync_service,
      std::unique_ptr<BrowserTabsMetadataFetcher>
          browser_tabs_metadata_fetcher);
  ~BrowserTabsModelProviderImpl() override;

  // BrowserTabsModelProvider:
  void TriggerRefresh() override;
  bool IsBrowserTabSyncEnabled() override;

 private:
  friend class BrowserTabsModelProviderImplTest;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  void AttemptBrowserTabsModelUpdate();
  void InvalidateWeakPtrsAndClearTabMetadata(bool is_tab_sync_enabled);
  void OnMetadataFetched(
      std::optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>
          metadata);
  std::optional<std::string> GetHostDeviceName() const;

  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
  std::unique_ptr<BrowserTabsMetadataFetcher> browser_tabs_metadata_fetcher_;
  base::CallbackListSubscription session_updated_subscription_;
  base::WeakPtrFactory<BrowserTabsModelProviderImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_IMPL_H_
