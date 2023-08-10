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
