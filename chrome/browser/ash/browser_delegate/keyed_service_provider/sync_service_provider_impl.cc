// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/keyed_service_provider/sync_service_provider_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/browser_context.h"

namespace ash {

SyncServiceProviderImpl::SyncServiceProviderImpl() = default;

SyncServiceProviderImpl::~SyncServiceProviderImpl() = default;

syncer::SyncService* SyncServiceProviderImpl::Find(
    const AccountId& account_id) {
  content::BrowserContext* context =
      BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id);
  // The //ash IsWallpaperSyncEnabled caller (unlike the //chrome one) queries
  // sync for the active account directly, so Find() can be reached for accounts
  // without a loaded profile (e.g. kiosk or device-local accounts, or during
  // teardown). Treat a missing context as "sync unavailable".
  if (!context) {
    return nullptr;
  }
  return SyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
