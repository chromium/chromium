// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/sync_explicit_passphrase_client_ash.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/service/sync_user_settings.h"

namespace ash {

namespace {

bool IsPassphraseAvailable(const syncer::SyncService& sync_service) {
  return sync_service.GetUserSettings()->IsUsingExplicitPassphrase() &&
         !sync_service.GetUserSettings()->IsPassphraseRequired();
}

}  // namespace

SyncExplicitPassphraseClientAsh::SyncExplicitPassphraseClientAsh(
    syncer::SyncService* sync_service)
    : sync_service_(sync_service),
      is_passphrase_required_(
          sync_service_->GetUserSettings()->IsPassphraseRequired()),
      is_passphrase_available_(IsPassphraseAvailable(*sync_service_)) {
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
  // |is_passphrase_required_| and |is_passphrase_available_| are mutually
  // exclusive.
  DCHECK(!is_passphrase_required_ || !is_passphrase_available_);
  if (is_passphrase_required_) {
    observers_.Get(observer_id)->OnPassphraseRequired();
  } else if (is_passphrase_available_) {
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
      sync_service_->GetUserSettings()
          ->GetExplicitPassphraseDecryptionNigoriKey();
  if (!decryption_key) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(syncer::NigoriToMojo(*decryption_key));
}

void SyncExplicitPassphraseClientAsh::SetDecryptionNigoriKey(
    crosapi::mojom::AccountKeyPtr mojo_account_key,
    crosapi::mojom::NigoriKeyPtr mojo_nigori_key) {
  if (!ValidateAccountKey(mojo_account_key) || !mojo_nigori_key) {
    return;
  }

  std::unique_ptr<syncer::Nigori> nigori_key =
      syncer::NigoriFromMojo(*mojo_nigori_key);
  if (!nigori_key) {
    // Deserialization failed, |mojo_nigori_key| doesn't represent an actual
    // Nigori key.
    return;
  }
  sync_service_->GetUserSettings()->SetExplicitPassphraseDecryptionNigoriKey(
      std::move(nigori_key));
}

void SyncExplicitPassphraseClientAsh::OnStateChanged(
    syncer::SyncService* sync_service) {
  bool new_is_passphrase_required =
      sync_service->GetUserSettings()->IsPassphraseRequired();
  if (new_is_passphrase_required && !is_passphrase_required_) {
    for (auto& observer : observers_) {
      observer->OnPassphraseRequired();
    }
  }
  is_passphrase_required_ = new_is_passphrase_required;

  bool new_is_passphrase_available = IsPassphraseAvailable(*sync_service);
  if (new_is_passphrase_available && !is_passphrase_available_) {
    for (auto& observer : observers_) {
      observer->OnPassphraseAvailable();
    }
  }
  is_passphrase_available_ = new_is_passphrase_available;
}

void SyncExplicitPassphraseClientAsh::FlushMojoForTesting() {
  observers_.FlushForTesting();  // IN-TEST
}

bool SyncExplicitPassphraseClientAsh::ValidateAccountKey(
    const crosapi::mojom::AccountKeyPtr& mojo_account_key) const {
  const std::optional<account_manager::AccountKey> account_key =
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
