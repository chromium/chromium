// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_SYNC_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_SYNC_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/sync/sync_service_provider.h"

class AccountId;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

// //chrome-side implementation of SyncServiceProvider. Wraps
// //chrome/browser/sync's Profile-keyed SyncServiceFactory so ChromeOS callers
// can reach the service through the chromeos-side interface.
class SyncServiceProviderImpl : public SyncServiceProvider {
 public:
  SyncServiceProviderImpl();
  SyncServiceProviderImpl(const SyncServiceProviderImpl&) = delete;
  SyncServiceProviderImpl& operator=(const SyncServiceProviderImpl&) = delete;
  ~SyncServiceProviderImpl() override;

  // SyncServiceProvider:
  syncer::SyncService* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_SYNC_SERVICE_PROVIDER_IMPL_H_
