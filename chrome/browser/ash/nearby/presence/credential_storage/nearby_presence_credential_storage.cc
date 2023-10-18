// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// File paths must remain in sync with
// //components/leveldb_proto/public/shared_proto_database_client_list.cc
const base::FilePath::CharType kLocalPublicCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresenceLocalPublicCredentialDatabase");
const base::FilePath::CharType kRemotePublicCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresenceRemotePublicCredentialDatabase");
const base::FilePath::CharType kPrivateCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresencePrivateCredentialDatabase");

// When saving credentials, |delete_key_filter| is always set to true, since
// |entries_to_save| are not deleted if they match |delete_key_filter|. This
// avoids duplicating new keys in memory to be saved.
const leveldb_proto::KeyFilter& DeleteKeyFilter() {
  static const base::NoDestructor<leveldb_proto::KeyFilter> filter(
      base::BindRepeating([](const std::string& key) { return true; }));
  return *filter;
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceCredentialStorage::NearbyPresenceCredentialStorage(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_filepath) {
  CHECK(db_provider);

  base::FilePath private_database_path =
      profile_filepath.Append(kPrivateCredentialDatabaseName);
  base::FilePath local_public_database_path =
      profile_filepath.Append(kLocalPublicCredentialDatabaseName);
  base::FilePath remote_public_database_path =
      profile_filepath.Append(kRemotePublicCredentialDatabaseName);

  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  auto private_db = db_provider->GetDB<::nearby::internal::LocalCredential>(
      leveldb_proto::ProtoDbType::NEARBY_PRESENCE_PRIVATE_CREDENTIAL_DATABASE,
      private_database_path, database_task_runner);
  auto local_public_db =
      db_provider->GetDB<::nearby::internal::SharedCredential>(
          leveldb_proto::ProtoDbType::
              NEARBY_PRESENCE_LOCAL_PUBLIC_CREDENTIAL_DATABASE,
          local_public_database_path, database_task_runner);
  auto remote_public_db =
      db_provider->GetDB<::nearby::internal::SharedCredential>(
          leveldb_proto::ProtoDbType::
              NEARBY_PRESENCE_REMOTE_PUBLIC_CREDENTIAL_DATABASE,
          remote_public_database_path, database_task_runner);
  NearbyPresenceCredentialStorage(std::move(private_db),
                                  std::move(local_public_db),
                                  std::move(remote_public_db));
}

NearbyPresenceCredentialStorage::NearbyPresenceCredentialStorage(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        ::nearby::internal::LocalCredential>> private_db,
    std::unique_ptr<
        leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
        local_public_db,
    std::unique_ptr<
        leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
        remote_public_db)
    : private_db_(std::move(private_db)),
      local_public_db_(std::move(local_public_db)),
      remote_public_db_(std::move(remote_public_db)) {
  CHECK(private_db_);
  CHECK(local_public_db_);
  CHECK(remote_public_db_);
}

NearbyPresenceCredentialStorage::~NearbyPresenceCredentialStorage() = default;

void NearbyPresenceCredentialStorage::Initialize(
    base::OnceCallback<void(bool)> on_fully_initialized) {
  // First attempt to initialize the private database. If successful,
  // the local public database, followed by the remote public database,
  // will attempt initialization.
  private_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPrivateDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized)));
}

void NearbyPresenceCredentialStorage::SaveCredentials(
    std::vector<mojom::LocalCredentialPtr> local_credentials,
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    mojom::PublicCredentialType public_credential_type,
    SaveCredentialsCallback on_credentials_fully_saved_callback) {
  CHECK(on_credentials_fully_saved_callback);

  auto credential_pairs_to_save = std::make_unique<std::vector<
      std::pair<std::string, ::nearby::internal::SharedCredential>>>();
  for (const auto& shared_credential : shared_credentials) {
    auto shared_credential_proto =
        proto::SharedCredentialFromMojom(shared_credential.get());

    credential_pairs_to_save->emplace_back(std::make_pair(
        shared_credential_proto.secret_id(), shared_credential_proto));
  }

  switch (public_credential_type) {
    case (mojom::PublicCredentialType::kLocalPublicCredential):
      // In the chain of callbacks, attempt to first save public credentials.
      // If successful, then attempt to save private credentials in a
      // follow-up callback. Iff both operations are successful,
      // 'on_credentials_fully_saved_callback' will return kOk.
      local_public_db_->UpdateEntriesWithRemoveFilter(
          /*entries_to_save=*/std::move(credential_pairs_to_save),
          /*delete_key_filter=*/DeleteKeyFilter(),
          base::BindOnce(
              &NearbyPresenceCredentialStorage::OnLocalPublicCredentialsSaved,
              weak_ptr_factory_.GetWeakPtr(), std::move(local_credentials),
              std::move(on_credentials_fully_saved_callback)));
      break;
    case (mojom::PublicCredentialType::kRemotePublicCredential):
      // When remote public credentials are updated, the private credentials
      // provided are empty. To preserve the existing private credentials, do
      // not update the private credential database.
      remote_public_db_->UpdateEntriesWithRemoveFilter(
          /*entries_to_save=*/std::move(credential_pairs_to_save),
          /*delete_key_filter=*/DeleteKeyFilter(),
          base::BindOnce(
              &NearbyPresenceCredentialStorage::OnRemotePublicCredentialsSaved,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(on_credentials_fully_saved_callback)));
      break;
  }
}

void NearbyPresenceCredentialStorage::GetPublicCredentials(
    mojom::PublicCredentialType public_credential_type,
    GetPublicCredentialsCallback callback) {
  CHECK(callback);

  switch (public_credential_type) {
    case (mojom::PublicCredentialType::kLocalPublicCredential):
      local_public_db_->LoadEntries(base::BindOnce(
          &NearbyPresenceCredentialStorage::OnPublicCredentialsRetrieved,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
    case (mojom::PublicCredentialType::kRemotePublicCredential):
      remote_public_db_->LoadEntries(base::BindOnce(
          &NearbyPresenceCredentialStorage::OnPublicCredentialsRetrieved,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
  }
}

void NearbyPresenceCredentialStorage::GetPrivateCredentials(
    GetPrivateCredentialsCallback callback) {
  CHECK(callback);
  private_db_->LoadEntries(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPrivateCredentialsRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NearbyPresenceCredentialStorage::OnPrivateCredentialsRetrieved(
    GetPrivateCredentialsCallback callback,
    bool success,
    std::unique_ptr<std::vector<::nearby::internal::LocalCredential>> entries) {
  CHECK(callback);

  if (!success) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__ << ": failed to retrieve private credentials";
    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kAborted,
                            absl::nullopt);
    return;
  }

  CHECK(entries);

  std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
      local_credentials_mojom;
  for (const auto& entry : *entries) {
    local_credentials_mojom.emplace_back(
        ash::nearby::presence::proto::LocalCredentialToMojom(entry));
  }

  std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk,
                          std::move(local_credentials_mojom));
}

void NearbyPresenceCredentialStorage::OnPublicCredentialsRetrieved(
    GetPublicCredentialsCallback callback,
    bool success,
    std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
        entries) {
  CHECK(callback);

  if (!success) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__ << ": failed to retrieve public credentials";
    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kAborted,
                            absl::nullopt);
    return;
  }

  CHECK(entries);

  std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
      shared_credentials_mojom;
  for (const auto& entry : *entries) {
    auto credential_mojo =
        ash::nearby::presence::proto::SharedCredentialToMojom(entry);
    shared_credentials_mojom.push_back(std::move(credential_mojo));
  }

  std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk,
                          std::move(shared_credentials_mojom));
}

void NearbyPresenceCredentialStorage::OnLocalPublicCredentialsSaved(
    std::vector<mojom::LocalCredentialPtr> local_credentials,
    SaveCredentialsCallback on_credentials_fully_saved_callback,
    bool success) {
  CHECK(on_credentials_fully_saved_callback);

  // If local public credentials failed to save, skip saving the
  // private credentials.
  if (!success) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__ << ": failed to save local public credentials";
    std::move(on_credentials_fully_saved_callback)
        .Run(mojo_base::mojom::AbslStatusCode::kAborted);
    return;
  }

  std::vector<::nearby::internal::LocalCredential> proto_local_credentials;
  for (const auto& local_credential : local_credentials) {
    proto_local_credentials.push_back(
        proto::LocalCredentialFromMojom(local_credential.get()));
  }

  auto credential_pairs_to_save = std::make_unique<std::vector<
      std::pair<std::string, ::nearby::internal::LocalCredential>>>();
  for (const auto& local_credential : proto_local_credentials) {
    credential_pairs_to_save->emplace_back(
        std::make_pair(local_credential.secret_id(), local_credential));
  }

  private_db_->UpdateEntriesWithRemoveFilter(
      /*entries_to_save=*/std::move(credential_pairs_to_save),
      /*delete_key_filter=*/DeleteKeyFilter(),
      base::BindOnce(
          &NearbyPresenceCredentialStorage::OnPrivateCredentialsSaved,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(on_credentials_fully_saved_callback)));
}

void NearbyPresenceCredentialStorage::OnRemotePublicCredentialsSaved(
    SaveCredentialsCallback on_credentials_fully_saved_callback,
    bool success) {
  mojo_base::mojom::AbslStatusCode save_status;
  if (success) {
    save_status = mojo_base::mojom::AbslStatusCode::kOk;
  } else {
    // TODO(b/287334363): Emit failure metric for remote public credential save.
    LOG(ERROR) << __func__ << ": failed to save remote public credentials";
    save_status = mojo_base::mojom::AbslStatusCode::kAborted;
  }

  CHECK(on_credentials_fully_saved_callback);
  std::move(on_credentials_fully_saved_callback).Run(save_status);
}

void NearbyPresenceCredentialStorage::OnPrivateCredentialsSaved(
    SaveCredentialsCallback on_credentials_fully_saved_callback,
    bool success) {
  mojo_base::mojom::AbslStatusCode save_status;
  if (success) {
    save_status = mojo_base::mojom::AbslStatusCode::kOk;
  } else {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__ << ": failed to save private credentials";
    save_status = mojo_base::mojom::AbslStatusCode::kAborted;
  }

  CHECK(on_credentials_fully_saved_callback);
  std::move(on_credentials_fully_saved_callback).Run(save_status);
}

void NearbyPresenceCredentialStorage::OnPrivateDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    leveldb_proto::Enums::InitStatus private_db_initialization_status) {
  // If the private initialization failed, do not attempt to initialize the
  // public databases.
  if (private_db_initialization_status !=
      leveldb_proto::Enums::InitStatus::kOK) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__
               << ": failed to initialize private credential database with "
                  "initialization status: "
               << private_db_initialization_status;
    std::move(on_fully_initialized).Run(/*success=*/false);
    return;
  }

  // Attempt to initialize the local public credential database. Iff successful,
  // then attempt to initialize the remote public credential database.
  local_public_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnLocalPublicDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized)));
}

void NearbyPresenceCredentialStorage::OnLocalPublicDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    leveldb_proto::Enums::InitStatus local_public_db_initialization_status) {
  // If the local public initialization failed, do not attempt to initialize the
  // remote public database.
  if (local_public_db_initialization_status !=
      leveldb_proto::Enums::InitStatus::kOK) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__
               << ": failed to initialize local public credential database "
                  "with initialization status: "
               << local_public_db_initialization_status;
    std::move(on_fully_initialized).Run(/*success=*/false);
    return;
  }

  remote_public_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnRemotePublicDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized)));
}

void NearbyPresenceCredentialStorage::OnRemotePublicDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    leveldb_proto::Enums::InitStatus remote_public_db_initialization_status) {
  if (remote_public_db_initialization_status !=
      leveldb_proto::Enums::InitStatus::kOK) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__
               << ": failed to initialize remote public credential database "
                  "with initialization status: "
               << remote_public_db_initialization_status;
    std::move(on_fully_initialized).Run(/*success=*/false);
    return;
  }

  std::move(on_fully_initialized).Run(/*success=*/true);
}

}  // namespace ash::nearby::presence
