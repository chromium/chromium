// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_explicit_passphrase_client_lacros.h"

#include "base/functional/callback.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/chromeos/explicit_passphrase_mojo_utils.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace {

crosapi::mojom::AccountKeyPtr GetMojoAccountKey(
    const syncer::SyncService& sync_service) {
  return account_manager::ToMojoAccountKey(account_manager::AccountKey(
      sync_service.GetAccountInfo().gaia, account_manager::AccountType::kGaia));
}

bool IsPassphraseAvailable(const syncer::SyncService& sync_service) {
  return sync_service.GetUserSettings()->IsUsingExplicitPassphrase() &&
         !sync_service.GetUserSettings()->IsPassphraseRequired();
}

}  // namespace

// TODO(crbug.com/40191593): Consider sharing sync service observer logic
// between Ash and Lacros.
SyncExplicitPassphraseClientLacros::LacrosSyncServiceObserver::
    LacrosSyncServiceObserver(
        syncer::SyncService* sync_service,
        SyncExplicitPassphraseClientLacros* explicit_passphrase_client)
    : sync_service_(sync_service),
      explicit_passphrase_client_(explicit_passphrase_client),
      is_passphrase_required_(
          sync_service->GetUserSettings()->IsPassphraseRequired()),
      is_passphrase_available_(IsPassphraseAvailable(*sync_service)) {
  sync_service_->AddObserver(this);
}

SyncExplicitPassphraseClientLacros::LacrosSyncServiceObserver::
    ~LacrosSyncServiceObserver() {
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void SyncExplicitPassphraseClientLacros::LacrosSyncServiceObserver::
    OnStateChanged(syncer::SyncService* sync_service) {
  const bool new_is_passphrase_required =
      sync_service_->GetUserSettings()->IsPassphraseRequired();
  if (new_is_passphrase_required && !is_passphrase_required_) {
    explicit_passphrase_client_->OnLacrosPassphraseRequired();
  }
  is_passphrase_required_ = new_is_passphrase_required;

  const bool new_is_passphrase_available =
      IsPassphraseAvailable(*sync_service_);
  if (new_is_passphrase_available && !is_passphrase_available_) {
    explicit_passphrase_client_->OnLacrosPassphraseAvailable();
  }
  is_passphrase_available_ = new_is_passphrase_available;
}

SyncExplicitPassphraseClientLacros::AshSyncExplicitPassphraseClientObserver::
    AshSyncExplicitPassphraseClientObserver(
        SyncExplicitPassphraseClientLacros* explicit_passphrase_client,
        mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>*
            explicit_passphrase_client_remote)
    : explicit_passphrase_client_(explicit_passphrase_client) {
  (*explicit_passphrase_client_remote)
      ->AddObserver(receiver_.BindNewPipeAndPassRemote());
}

SyncExplicitPassphraseClientLacros::AshSyncExplicitPassphraseClientObserver::
    ~AshSyncExplicitPassphraseClientObserver() = default;

void SyncExplicitPassphraseClientLacros::
    AshSyncExplicitPassphraseClientObserver::OnPassphraseRequired() {
  // Note: |is_passphrase_available_| and |is_passphrase_required_| are mutually
  // exclusive, although both can be set to false;
  is_passphrase_required_ = true;
  is_passphrase_available_ = false;
  explicit_passphrase_client_->OnAshPassphraseRequired();
}

void SyncExplicitPassphraseClientLacros::
    AshSyncExplicitPassphraseClientObserver::OnPassphraseAvailable() {
  // Note: |is_passphrase_available_| and |is_passphrase_required_| are mutually
  // exclusive, although both can be set to false;
  is_passphrase_required_ = false;
  is_passphrase_available_ = true;
  explicit_passphrase_client_->OnAshPassphraseAvailable();
}

SyncExplicitPassphraseClientLacros::SyncExplicitPassphraseClientLacros(
    mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient> remote,
    syncer::SyncService* sync_service)
    : sync_service_(sync_service),
      sync_service_observer_(sync_service,
                             /*explicit_passphrase_client=*/this),
      remote_(std::move(remote)) {
  DCHECK(remote_.is_bound());

  ash_explicit_passphrase_client_observer_ =
      std::make_unique<AshSyncExplicitPassphraseClientObserver>(
          /*explicit_passphrase_client=*/this, &remote_);
  // Ash will notify |ash_explicit_passphrase_client_observer_| upon connection
  // if passphrase is required or available, nothing actionable till then.
}

SyncExplicitPassphraseClientLacros::~SyncExplicitPassphraseClientLacros() =
    default;

void SyncExplicitPassphraseClientLacros::FlushMojoForTesting() {
  remote_.FlushForTesting();  // IN-TEST
}

void SyncExplicitPassphraseClientLacros::OnLacrosPassphraseRequired() {
  DCHECK(ash_explicit_passphrase_client_observer_);
  if (ash_explicit_passphrase_client_observer_->is_passphrase_available()) {
    QueryDecryptionKeyFromAsh();
  }
}

void SyncExplicitPassphraseClientLacros::OnLacrosPassphraseAvailable() {
  DCHECK(ash_explicit_passphrase_client_observer_);
  if (ash_explicit_passphrase_client_observer_->is_passphrase_required()) {
    SendDecryptionKeyToAsh();
  }
}

void SyncExplicitPassphraseClientLacros::OnAshPassphraseRequired() {
  if (sync_service_observer_.is_passphrase_available()) {
    SendDecryptionKeyToAsh();
  }
}

void SyncExplicitPassphraseClientLacros::OnAshPassphraseAvailable() {
  if (sync_service_observer_.is_passphrase_required()) {
    QueryDecryptionKeyFromAsh();
  }
}

void SyncExplicitPassphraseClientLacros::QueryDecryptionKeyFromAsh() {
  DCHECK(sync_service_);
  remote_->GetDecryptionNigoriKey(
      GetMojoAccountKey(*sync_service_),
      base::BindOnce(&SyncExplicitPassphraseClientLacros::
                         OnQueryDecryptionKeyFromAshCompleted,
                     base::Unretained(this)));
}

void SyncExplicitPassphraseClientLacros::SendDecryptionKeyToAsh() {
  DCHECK(sync_service_);
  std::unique_ptr<syncer::Nigori> decryption_key =
      sync_service_->GetUserSettings()
          ->GetExplicitPassphraseDecryptionNigoriKey();
  if (!decryption_key) {
    return;
  }
  remote_->SetDecryptionNigoriKey(GetMojoAccountKey(*sync_service_),
                                  syncer::NigoriToMojo(*decryption_key));
}

void SyncExplicitPassphraseClientLacros::OnQueryDecryptionKeyFromAshCompleted(
    crosapi::mojom::NigoriKeyPtr mojo_nigori_key) {
  DCHECK(sync_service_);
  if (!mojo_nigori_key) {
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
