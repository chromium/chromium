// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/sync_explicit_passphrase_client_lacros.h"

#include "base/callback.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/nigori/nigori.h"

namespace {

// TODO(crbug.com/1233545): Move common parts of Ash and Lacros implementation
// somewhere under components/ and reuse them (mojo utils, test helpers, maybe
// observer of the sync service).
crosapi::mojom::AccountKeyPtr GetMojoAccountKey(
    const syncer::SyncService& sync_service) {
  return account_manager::ToMojoAccountKey(account_manager::AccountKey(
      sync_service.GetAccountInfo().gaia, account_manager::AccountType::kGaia));
}

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

bool IsPassphraseAvailable(const syncer::SyncService& sync_service) {
  return sync_service.GetUserSettings()->IsUsingExplicitPassphrase() &&
         !sync_service.GetUserSettings()->IsPassphraseRequired();
}

}  // namespace

crosapi::mojom::NigoriKeyPtr NigoriToMojoForTesting(  // IN-TEST
    const syncer::Nigori& nigori) {
  return NigoriToMojo(nigori);
}

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

void SyncExplicitPassphraseClientLacros::LacrosSyncServiceObserver::
    OnSyncShutdown(syncer::SyncService* sync_service) {
  explicit_passphrase_client_->OnLacrosSyncShutdown();
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
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
    syncer::SyncService* sync_service,
    mojo::Remote<crosapi::mojom::SyncService>* sync_service_remote)
    : sync_service_(sync_service),
      sync_service_observer_(sync_service,
                             /*explicit_passphrase_client=*/this) {
  DCHECK(sync_service_remote);
  DCHECK(sync_service_remote->is_bound());

  (*sync_service_remote)
      ->BindExplicitPassphraseClient(remote_.BindNewPipeAndPassReceiver());
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

void SyncExplicitPassphraseClientLacros::OnLacrosSyncShutdown() {
  // Disconnect mojo to ensure no methods will access the SyncService anymore.
  remote_.reset();
  ash_explicit_passphrase_client_observer_.reset();
  sync_service_ = nullptr;
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
      sync_service_->GetUserSettings()->GetDecryptionNigoriKey();
  if (!decryption_key) {
    return;
  }
  remote_->SetDecryptionNigoriKey(GetMojoAccountKey(*sync_service_),
                                  NigoriToMojo(*decryption_key));
}

void SyncExplicitPassphraseClientLacros::OnQueryDecryptionKeyFromAshCompleted(
    crosapi::mojom::NigoriKeyPtr mojo_nigori_key) {
  DCHECK(sync_service_);
  if (!mojo_nigori_key) {
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
