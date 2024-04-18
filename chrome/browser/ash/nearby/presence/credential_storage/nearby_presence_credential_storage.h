// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_
#define CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}

namespace ash::nearby::presence {

// This base class exists for test implementations of
// NearbyPresenceCredentialStorage.
class NearbyPresenceCredentialStorageBase
    : public mojom::NearbyPresenceCredentialStorage {
 public:
  ~NearbyPresenceCredentialStorageBase() override = default;

  virtual void Initialize(base::OnceCallback<void(bool)> on_initialized) = 0;
};

// Implementation of the Mojo NearbyPresenceCredentialStorage interface. It
// handles requests to read/write to the credential storage database for Nearby
// Presence.
class NearbyPresenceCredentialStorage
    : public NearbyPresenceCredentialStorageBase {
 public:
  NearbyPresenceCredentialStorage(
      mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
          pending_receiver,
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& profile_path);
  NearbyPresenceCredentialStorage(const NearbyPresenceCredentialStorage&) =
      delete;
  NearbyPresenceCredentialStorage& operator=(
      const NearbyPresenceCredentialStorage&) = delete;

  ~NearbyPresenceCredentialStorage() override;

  // Asynchronously initializes the private credential database, and then the
  // public credential database. The callback is invoked with true iff both
  // initializations are successful. Must be called in order to store
  // credentials.
  void Initialize(base::OnceCallback<void(bool)> on_initialized) override;

  // mojom::NearbyPresenceCredentialStorage:
  void SaveCredentials(
      std::vector<mojom::LocalCredentialPtr> local_credentials,
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      mojom::PublicCredentialType public_credential_type,
      SaveCredentialsCallback on_credentials_fully_saved_callback) override;
  void GetPublicCredentials(mojom::PublicCredentialType public_credential_type,
                            GetPublicCredentialsCallback callback) override;
  void GetPrivateCredentials(GetPrivateCredentialsCallback callback) override;
  void UpdateLocalCredential(mojom::LocalCredentialPtr local_credential,
                             UpdateLocalCredentialCallback callback) override;

 protected:
  // Test only constructor used to inject databases without using a profile.
  NearbyPresenceCredentialStorage(
      mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
          pending_receiver,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::LocalCredential>>
          private_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          local_public_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          remote_public_db);

 private:
  void OnLocalCredentialUpdated(UpdateLocalCredentialCallback callback,
                                bool success);
  void OnPrivateCredentialsRetrieved(
      GetPrivateCredentialsCallback callback,
      base::TimeTicks retrieval_start_time,
      bool success,
      std::unique_ptr<std::vector<::nearby::internal::LocalCredential>>
          entries);
  void OnPublicCredentialsRetrieved(
      GetPublicCredentialsCallback callback,
      base::TimeTicks retrieval_start_time,
      mojom::PublicCredentialType public_credential_type,
      bool success,
      std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
          entries);

  void OnLocalPublicCredentialsSaved(
      std::vector<mojom::LocalCredentialPtr> local_credentials,
      SaveCredentialsCallback on_credentials_fully_saved_callback,
      bool success);
  void OnRemotePublicCredentialsSaved(
      SaveCredentialsCallback on_credentials_fully_saved_callback,
      bool success);
  void OnPrivateCredentialsSaved(
      SaveCredentialsCallback on_credentials_fully_saved_callback,
      bool success);

  void OnPrivateDatabaseInitialized(
      base::OnceCallback<void(bool)> on_fully_initialized,
      base::TimeTicks initialization_start_time,
      leveldb_proto::Enums::InitStatus private_db_initialization_status);
  void OnLocalPublicDatabaseInitialized(
      base::OnceCallback<void(bool)> on_fully_initialized,
      base::TimeTicks initialization_start_time,
      leveldb_proto::Enums::InitStatus local_public_db_initialization_status);
  void OnRemotePublicDatabaseInitialized(
      base::OnceCallback<void(bool)> on_fully_initialized,
      base::TimeTicks initialization_start_time,
      leveldb_proto::Enums::InitStatus remote_public_db_initialization_status);

  void RecordCredentialsCountAndSize();
  void RecordLocalSharedCredentialsCountAndSize(
      bool success,
      std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
          entries);
  void RecordRemoteSharedCredentialsCountAndSize(
      bool success,
      std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
          entries);

  std::unique_ptr<
      leveldb_proto::ProtoDatabase<::nearby::internal::LocalCredential>>
      private_db_;
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
      local_public_db_;
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
      remote_public_db_;

  // `reciever_` is only bound after successful initialization.
  mojo::Receiver<mojom::NearbyPresenceCredentialStorage> receiver_{this};

  // `pending_receiver_` will only be valid prior to initialization, and is
  // bound to `receiver_` upon a successful call to `Initialize()`.
  mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
      pending_receiver_;

  base::WeakPtrFactory<NearbyPresenceCredentialStorage> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_
