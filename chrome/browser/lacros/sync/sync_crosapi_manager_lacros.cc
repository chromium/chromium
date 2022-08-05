// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_crosapi_manager_lacros.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/lacros/sync/sync_explicit_passphrase_client_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync/base/features.h"

namespace {

// Creates SyncExplicitPassphraseClientLacros for `profile` if preconditions
// are met, returns nullptr otherwise. Preconditions are:
// 1. Sync passphrase sharing feature is enabled.
// 2. SyncService crosapi is available.
// 3. Lacros SyncService exists (can be not created due to command line config).
// `profile` must be main profile.
std::unique_ptr<SyncExplicitPassphraseClientLacros>
MaybeCreateSyncExplicitPassphraseClient(Profile* profile) {
  DCHECK(profile->IsMainProfile());
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncChromeOSExplicitPassphraseSharing)) {
    return nullptr;
  }

  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::SyncService>()) {
    return nullptr;
  }

  auto* sync_service = SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return nullptr;
  }

  // TODO(crbug.com/1327602): move high-level Crosapi initialization logic to
  // SyncCrosapiManagerLacros and make SyncExplicitPassphraseClientLacros
  // working with crosapi::mojom::SyncExplicitPassphraseClient directly.
  return std::make_unique<SyncExplicitPassphraseClientLacros>(
      sync_service, &lacros_service->GetRemote<crosapi::mojom::SyncService>());
}

}  // namespace

SyncCrosapiManagerLacros::SyncCrosapiManagerLacros() = default;

SyncCrosapiManagerLacros::~SyncCrosapiManagerLacros() = default;

void SyncCrosapiManagerLacros::PostProfileInit(Profile* profile) {
  if (!profile->IsMainProfile()) {
    return;
  }

  DCHECK(!sync_explicit_passphrase_client_);
  sync_explicit_passphrase_client_ =
      MaybeCreateSyncExplicitPassphraseClient(profile);
}
