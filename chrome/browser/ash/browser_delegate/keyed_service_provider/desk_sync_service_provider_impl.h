// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_DESK_SYNC_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_DESK_SYNC_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/desks_storage/desk_sync_service_provider.h"

class AccountId;

namespace desks_storage {
class DeskSyncService;
}  // namespace desks_storage

namespace ash {

// //chrome-side implementation of DeskSyncServiceProvider. Wraps
// //chrome/browser/sync's Profile-keyed DeskSyncServiceFactory so ChromeOS
// callers can reach the service through the chromeos-side interface.
class DeskSyncServiceProviderImpl : public DeskSyncServiceProvider {
 public:
  DeskSyncServiceProviderImpl();
  DeskSyncServiceProviderImpl(const DeskSyncServiceProviderImpl&) = delete;
  DeskSyncServiceProviderImpl& operator=(const DeskSyncServiceProviderImpl&) =
      delete;
  ~DeskSyncServiceProviderImpl() override;

  // DeskSyncServiceProvider:
  desks_storage::DeskSyncService* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_DESK_SYNC_SERVICE_PROVIDER_IMPL_H_
