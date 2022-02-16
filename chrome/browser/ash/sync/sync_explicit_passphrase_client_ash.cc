// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_explicit_passphrase_client_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/nigori/nigori.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

crosapi::mojom::NigoriKeyPtr NigoriToMojo(const syncer::Nigori& nigori) {
  std::string deprecated_user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori.ExportKeys(&deprecated_user_key, &encryption_key, &mac_key);

  crosapi::mojom::NigoriKeyPtr mojo_result = crosapi::mojom::NigoriKey::New();
  mojo_result->encryption_key =
      std::vector<uint8_t>(encryption_key.begin(), encryption_key.end());
  mojo_result->mac_key = std::vector<uint8_t>(mac_key.begin(), mac_key.end());
  return mojo_result;
}

std::unique_ptr<syncer::Nigori> NigoriFromMojo(
    const crosapi::mojom::NigoriKey& mojo_key) {
  const std::string encryption_key(mojo_key.encryption_key.begin(),
                                   mojo_key.encryption_key.end());
  const std::string mac_key(mojo_key.mac_key.begin(), mojo_key.mac_key.end());
  // |user_key| is deprecated, it's safe to pass empty string.
  return syncer::Nigori::CreateByImport(
      /*user_key=*/std::string(), encryption_key, mac_key);
}

}  // namespace

crosapi::mojom::NigoriKeyPtr NigoriToMojoForTesting(  // IN-TEST
    const syncer::Nigori& nigori) {
  return NigoriToMojo(nigori);
}

SyncExplicitPassphraseClientAsh::SyncExplicitPassphraseClientAsh(
    syncer::SyncService* sync_service)
    : sync_service_(sync_service),
      previous_passphrase_required_state_(
          sync_service_->GetUserSettings()->IsPassphraseRequired()) {
  sync_service_->AddObserver(this);
}

SyncExplicitPassphraseClientAsh::~SyncExplicitPassphraseClientAsh() {
  sync_service_->RemoveObserver(this);
}

void SyncExplicitPassphraseClientAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::SyncExplicitPassphraseClient>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SyncExplicitPassphraseClientAsh::AddObserver(
    mojo::PendingRemote<crosapi::mojom::SyncExplicitPassphraseClientObserver>
        observer) {
  auto observer_id = observers_.Add(std::move(observer));
  // Immediately notify observer if passphrase is required or available.
  if (previous_passphrase_required_state_) {
    observers_.Get(observer_id)->OnPassphraseRequired();
  } else if (sync_service_->GetUserSettings()->GetDecryptionNigoriKey()) {
    observers_.Get(observer_id)->OnPassphraseAvailable();
  }
}

void SyncExplicitPassphraseClientAsh::GetDecryptionNigoriKey(
    crosapi::mojom::AccountKeyPtr mojo_account_key,
    GetDecryptionNigoriKeyCallback callback) {
  if (!ValidateAccountKey(mojo_account_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<syncer::Nigori> decryption_key =
      sync_service_->GetUserSettings()->GetDecryptionNigoriKey();
  if (!decryption_key) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(NigoriToMojo(*decryption_key));
}

void SyncExplicitPassphraseClientAsh::SetDecryptionNigoriKey(
    crosapi::mojom::AccountKeyPtr mojo_account_key,
    crosapi::mojom::NigoriKeyPtr mojo_nigori_key) {
  if (!ValidateAccountKey(mojo_account_key) || !mojo_nigori_key) {
    return;
  }

  std::unique_ptr<syncer::Nigori> nigori_key = NigoriFromMojo(*mojo_nigori_key);
  if (!nigori_key) {
    // Deserialization failed, |mojo_nigori_key| doesn't represent an actual
    // Nigori key.
    return;
  }
  sync_service_->GetUserSettings()->SetDecryptionNigoriKey(
      std::move(nigori_key));
}

void SyncExplicitPassphraseClientAsh::OnStateChanged(
    syncer::SyncService* sync_service) {
  bool new_passphrase_required_state =
      sync_service->GetUserSettings()->IsPassphraseRequired();
  if (new_passphrase_required_state == previous_passphrase_required_state_) {
    // State change is not relevant for this class.
    return;
  }

  if (new_passphrase_required_state) {
    for (auto& observer : observers_) {
      observer->OnPassphraseRequired();
    }
  } else {
    // Passphrase required state was resolved, that means that new passphrase is
    // likely available (modulo some corner cases, like sync reset, but this
    // should be safe to issue redundant OnPassphraseAvailable() call).
    for (auto& observer : observers_) {
      observer->OnPassphraseAvailable();
    }
  }

  previous_passphrase_required_state_ = new_passphrase_required_state;
}

void SyncExplicitPassphraseClientAsh::FlushMojoForTesting() {
  observers_.FlushForTesting();  // IN-TEST
}

bool SyncExplicitPassphraseClientAsh::ValidateAccountKey(
    const crosapi::mojom::AccountKeyPtr& mojo_account_key) const {
  const absl::optional<account_manager::AccountKey> account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  if (!account_key.has_value()) {
    return false;
  }

  if (account_key->account_type() != account_manager::AccountType::kGaia) {
    // ActiveDirectory accounts are not supported.
    return false;
  }

  return !account_key->id().empty() &&
         account_key->id() == sync_service_->GetAccountInfo().gaia;
}

}  // namespace ash
