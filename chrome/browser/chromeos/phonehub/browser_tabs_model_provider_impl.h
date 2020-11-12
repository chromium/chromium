// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_IMPL_H_

#include <ostream>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/phonehub/browser_tabs_metadata_fetcher.h"
#include "chromeos/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace chromeos {
namespace phonehub {

// Responsible for providing the BrowserTabsModel to observers.
class BrowserTabsModelProviderImpl
    : public BrowserTabsModelProvider,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  BrowserTabsModelProviderImpl(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      sync_sessions::SessionSyncService* session_sync_service,
      std::unique_ptr<BrowserTabsMetadataFetcher>
          browser_tabs_metadata_fetcher);
  ~BrowserTabsModelProviderImpl() override;

 private:
  friend class BrowserTabsModelProviderImplTest;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;

  void AttemptBrowserTabsModelUpdate();
  void InvalidateWeakPtrsAndClearTabMetadata(bool is_tab_sync_enabled);
  void OnMetadataFetched(
      base::Optional<std::vector<BrowserTabsModel::BrowserTabMetadata>>
          metadata);
  base::Optional<std::string> GetSessionName() const;

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  sync_sessions::SessionSyncService* session_sync_service_;
  std::unique_ptr<BrowserTabsMetadataFetcher> browser_tabs_metadata_fetcher_;
  std::unique_ptr<base::CallbackList<void()>::Subscription>
      session_updated_subscription_;

  base::WeakPtrFactory<BrowserTabsModelProviderImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PHONEHUB_BROWSER_TABS_MODEL_PROVIDER_IMPL_H_
