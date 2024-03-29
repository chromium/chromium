// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/credential_storage_initializer.h"

#include "chrome/browser/ash/nearby/presence/credential_storage/metrics/credential_storage_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/cross_device/logging/logging.h"
#include "content/public/browser/storage_partition.h"

namespace ash::nearby::presence {

CredentialStorageInitializer::CredentialStorageInitializer(
    mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
        pending_receiver,
    Profile* profile) {
  CHECK(profile);
  auto* proto_db_provider = profile->GetOriginalProfile()
                                ->GetDefaultStoragePartition()
                                ->GetProtoDatabaseProvider();
  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  nearby_presence_credential_storage_ =
      std::make_unique<NearbyPresenceCredentialStorage>(
          std::move(pending_receiver), proto_db_provider, profile_path);
}

// Test only constructor used to inject a NearbyPresenceCredentialStorageBase.
CredentialStorageInitializer::CredentialStorageInitializer(
    std::unique_ptr<NearbyPresenceCredentialStorageBase>
        nearby_presence_credential_storage)
    : nearby_presence_credential_storage_(
          std::move(nearby_presence_credential_storage)) {
  CHECK(nearby_presence_credential_storage_);
}

CredentialStorageInitializer::~CredentialStorageInitializer() = default;

void CredentialStorageInitializer::Initialize() {
  CHECK(!is_initialized_);

  nearby_presence_credential_storage_->Initialize(
      base::BindOnce(&CredentialStorageInitializer::OnInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CredentialStorageInitializer::OnInitialized(bool initialization_success) {
  metrics::RecordCredentialStorageInitializationResult(
      /*success=*/initialization_success);

  // We don't expect initialization to fail very often -- comparable usages
  // have a 99.9% success rate. Simply fail for now. Later, we'll analyze
  // metrics post-MVP to understand the failure rate and if we need to
  // handle the failure case.
  if (!initialization_success) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": failed to initialize credential storage.";
  }
}

}  // namespace ash::nearby::presence
