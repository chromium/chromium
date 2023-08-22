// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace {

// File paths must remain in sync with
// //components/leveldb_proto/public/shared_proto_database_client_list.cc
const base::FilePath::CharType kPublicCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresencePublicCredentialDatabase");
const base::FilePath::CharType kPrivateCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresencePrivateCredentialDatabase");

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceCredentialStorage::NearbyPresenceCredentialStorage(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_filepath) {
  CHECK(db_provider);

  base::FilePath private_database_path =
      profile_filepath.Append(kPrivateCredentialDatabaseName);
  base::FilePath public_database_path =
      profile_filepath.Append(kPublicCredentialDatabaseName);

  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  auto private_db = db_provider->GetDB<::nearby::internal::LocalCredential>(
      leveldb_proto::ProtoDbType::NEARBY_PRESENCE_PRIVATE_CREDENTIAL_DATABASE,
      private_database_path, database_task_runner);
  auto public_db = db_provider->GetDB<::nearby::internal::SharedCredential>(
      leveldb_proto::ProtoDbType::NEARBY_PRESENCE_PUBLIC_CREDENTIAL_DATABASE,
      public_database_path, database_task_runner);

  NearbyPresenceCredentialStorage(std::move(private_db), std::move(public_db));
}

NearbyPresenceCredentialStorage::NearbyPresenceCredentialStorage(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        ::nearby::internal::LocalCredential>> private_db,
    std::unique_ptr<
        leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
        public_db)
    : private_db_(std::move(private_db)), public_db_(std::move(public_db)) {
  CHECK(private_db_);
  CHECK(public_db_);
}

NearbyPresenceCredentialStorage::~NearbyPresenceCredentialStorage() = default;

void NearbyPresenceCredentialStorage::Initialize(
    base::OnceCallback<void(bool)> on_fully_initialized) {
  // First attempt to initialize the private database. If successful,
  // the public database will then be initialized.
  private_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPrivateDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized)));
}

void NearbyPresenceCredentialStorage::SaveCredentials(
    std::vector<mojom::LocalCredentialPtr> local_credentials,
    SaveCredentialsCallback on_save_credential_callback) {
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

  // |delete_key_filter| is always set to true, since |entries_to_save| are not
  // deleted if they match |delete_key_filter|. This avoids duplicating new keys
  // in memory to be saved.
  const leveldb_proto::KeyFilter clearAllFilter =
      base::BindRepeating([](const std::string& key) { return true; });

  private_db_->UpdateEntriesWithRemoveFilter(
      std::move(credential_pairs_to_save), clearAllFilter,
      base::BindOnce(
          &NearbyPresenceCredentialStorage::OnPrivateCredentialsSaved,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(on_save_credential_callback)));
}

void NearbyPresenceCredentialStorage::OnPrivateCredentialsSaved(
    SaveCredentialsCallback on_save_credential_callback,
    bool success) {
  mojom::StatusCode save_status;
  if (success) {
    save_status = mojom::StatusCode::kOk;
  } else {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__ << ": failed to save private credentials";
    save_status = mojom::StatusCode::kFailure;
  }

  // TODO(b/287334195): Attempt to save public credentials if private
  // credentials were successfully saved.
  std::move(on_save_credential_callback).Run(save_status);
}

void NearbyPresenceCredentialStorage::OnPrivateDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    leveldb_proto::Enums::InitStatus private_db_initialization_status) {
  // If the private initialization failed, do not attempt to initialize the
  // public database.
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

  public_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPublicDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized)));
}

void NearbyPresenceCredentialStorage::OnPublicDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    leveldb_proto::Enums::InitStatus public_db_initialization_status) {
  if (public_db_initialization_status !=
      leveldb_proto::Enums::InitStatus::kOK) {
    // TODO(b/287334363): Emit a failure metric.
    LOG(ERROR) << __func__
               << ": failed to initialize public credential database with "
                  "initialization status: "
               << public_db_initialization_status;
    std::move(on_fully_initialized).Run(/*success=*/false);
    return;
  }

  std::move(on_fully_initialized).Run(/*success=*/true);
}

}  // namespace ash::nearby::presence
