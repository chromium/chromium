// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_SYNC_EXPLICIT_PASSPHRASE_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_SYNC_SYNC_EXPLICIT_PASSPHRASE_CLIENT_LACROS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/service/sync_service_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace syncer {
class SyncService;
}  // namespace syncer

// Once created, observes changes of passphrase state in both Lacros SyncService
// (via SyncServiceObserver) and Ash SyncService (via crosapi) and passes
// decryption nigori key from one to another when needed.
class SyncExplicitPassphraseClientLacros {
 public:
  // |remote| must be bound. |sync_service| must not be null and must outlive
  // |this| object.
  SyncExplicitPassphraseClientLacros(
      mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient> remote,
      syncer::SyncService* sync_service);
  SyncExplicitPassphraseClientLacros(
      const SyncExplicitPassphraseClientLacros& other) = delete;
  SyncExplicitPassphraseClientLacros& operator=(
      const SyncExplicitPassphraseClientLacros& other) = delete;
  ~SyncExplicitPassphraseClientLacros();

  void FlushMojoForTesting();

 private:
  class LacrosSyncServiceObserver : public syncer::SyncServiceObserver {
   public:
    LacrosSyncServiceObserver(
        syncer::SyncService* sync_service,
        SyncExplicitPassphraseClientLacros* explicit_passphrase_client);
    LacrosSyncServiceObserver(const LacrosSyncServiceObserver& other) = delete;
    LacrosSyncServiceObserver& operator=(
        const LacrosSyncServiceObserver& other) = delete;
    ~LacrosSyncServiceObserver() override;

    void OnStateChanged(syncer::SyncService* sync_service) override;

    bool is_passphrase_required() const { return is_passphrase_required_; }

    bool is_passphrase_available() const { return is_passphrase_available_; }

   private:
    raw_ptr<syncer::SyncService> sync_service_;
    raw_ptr<SyncExplicitPassphraseClientLacros> explicit_passphrase_client_;

    bool is_passphrase_required_;
    bool is_passphrase_available_;
  };

  class AshSyncExplicitPassphraseClientObserver
      : public crosapi::mojom::SyncExplicitPassphraseClientObserver {
   public:
    explicit AshSyncExplicitPassphraseClientObserver(
        SyncExplicitPassphraseClientLacros* explicit_passphrase_client,
        mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient>*
            explicit_passphrase_client_remote);
    AshSyncExplicitPassphraseClientObserver(
        const AshSyncExplicitPassphraseClientObserver& other) = delete;
    AshSyncExplicitPassphraseClientObserver& operator=(
        const AshSyncExplicitPassphraseClientObserver& other) = delete;
    ~AshSyncExplicitPassphraseClientObserver() override;

    void OnPassphraseRequired() override;
    void OnPassphraseAvailable() override;

    bool is_passphrase_required() const { return is_passphrase_required_; }

    bool is_passphrase_available() const { return is_passphrase_available_; }

   private:
    raw_ptr<SyncExplicitPassphraseClientLacros> explicit_passphrase_client_;
    mojo::Receiver<crosapi::mojom::SyncExplicitPassphraseClientObserver>
        receiver_{this};

    bool is_passphrase_required_ = false;
    bool is_passphrase_available_ = false;
  };

  void OnLacrosPassphraseRequired();
  void OnLacrosPassphraseAvailable();
  void OnAshPassphraseRequired();
  void OnAshPassphraseAvailable();

  void QueryDecryptionKeyFromAsh();
  void SendDecryptionKeyToAsh();

  void OnQueryDecryptionKeyFromAshCompleted(
      crosapi::mojom::NigoriKeyPtr mojo_nigori_key);

  raw_ptr<syncer::SyncService> sync_service_;
  LacrosSyncServiceObserver sync_service_observer_;
  std::unique_ptr<AshSyncExplicitPassphraseClientObserver>
      ash_explicit_passphrase_client_observer_;
  mojo::Remote<crosapi::mojom::SyncExplicitPassphraseClient> remote_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_SYNC_EXPLICIT_PASSPHRASE_CLIENT_LACROS_H_
