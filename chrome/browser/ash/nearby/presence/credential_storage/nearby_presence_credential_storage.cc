// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"

#include <optional>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/nearby/presence/credential_storage/metrics/credential_storage_metrics.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace {

// File paths must remain in sync with
// //components/leveldb_proto/public/shared_proto_database_client_list.cc
const base::FilePath::CharType kLocalPublicCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresenceLocalPublicCredentialDatabase");
const base::FilePath::CharType kRemotePublicCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresenceRemotePublicCredentialDatabase");
const base::FilePath::CharType kPrivateCredentialDatabaseName[] =
    FILE_PATH_LITERAL("NearbyPresencePrivateCredentialDatabase");

bool ShouldDeleteEntry(const base::flat_set<std::string>& keys_to_not_delete,
                       const std::string& key) {
  return !keys_to_not_delete.contains(key);
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceCredentialStorage::NearbyPresenceCredentialStorage(
    mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
        pending_receiver,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_filepath)
    : pending_receiver_(std::move(pending_receiver)) {
  CHECK(db_provider);
  CHECK(pending_receiver_);

  base::FilePath private_database_path =
      profile_filepath.Append(kPrivateCredentialDatabaseName);
  base::FilePath local_public_database_path =
      profile_filepath.Append(kLocalPublicCredentialDatabaseName);
  base::FilePath remote_public_database_path =
      profile_filepath.Append(kRemotePublicCredentialDatabaseName);

  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  private_db_ = db_provider->GetDB<::nearby::internal::LocalCredential>(
      leveldb_proto::ProtoDbType::NEARBY_PRESENCE_PRIVATE_CREDENTIAL_DATABASE,
      private_database_path, database_task_runner);
  local_public_db_ = db_provider->GetDB<::nearby::internal::SharedCredential>(
      leveldb_proto::ProtoDbType::
          NEARBY_PRESENCE_LOCAL_PUBLIC_CREDENTIAL_DATABASE,
      local_public_database_path, database_task_runner);
  remote_public_db_ = db_provider->GetDB<::nearby::internal::SharedCredential>(
      leveldb_proto::ProtoDbType::
          NEARBY_PRESENCE_REMOTE_PUBLIC_CREDENTIAL_DATABASE,
      remote_public_database_path, database_task_runner);
}

// Test only constructor used to inject databases without using a profile.
NearbyPresenceCredentialStorage::NearbyPresenceCredentialStorage(
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
        remote_public_db)
    : private_db_(std::move(private_db)),
      local_public_db_(std::move(local_public_db)),
      remote_public_db_(std::move(remote_public_db)),
      pending_receiver_(std::move(pending_receiver)) {
  CHECK(private_db_);
  CHECK(local_public_db_);
  CHECK(remote_public_db_);
  CHECK(pending_receiver_);
}

NearbyPresenceCredentialStorage::~NearbyPresenceCredentialStorage() = default;

void NearbyPresenceCredentialStorage::Initialize(
    base::OnceCallback<void(bool)> on_fully_initialized) {
  // First attempt to initialize the private database. If successful,
  // the local public database, followed by the remote public database,
  // will attempt initialization. If all databases successfully initialize,
  // `pending_receiver` will be bound and `on_fully_initialized` will return
  // true.
  private_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPrivateDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized),
      /*initialization_start_time=*/base::TimeTicks::Now()));
}

void NearbyPresenceCredentialStorage::SaveCredentials(
    std::vector<mojom::LocalCredentialPtr> local_credentials,
    std::vector<mojom::SharedCredentialPtr> shared_credentials,
    mojom::PublicCredentialType public_credential_type,
    SaveCredentialsCallback on_credentials_fully_saved_callback) {
  CHECK(on_credentials_fully_saved_callback);

  auto credential_pairs_to_save = std::make_unique<std::vector<
      std::pair<std::string, ::nearby::internal::SharedCredential>>>();
  base::flat_set<std::string> keys_to_not_delete;
  for (const auto& shared_credential : shared_credentials) {
    auto shared_credential_proto =
        proto::SharedCredentialFromMojom(shared_credential.get());

    std::string id = base::NumberToString(shared_credential_proto.id());
    credential_pairs_to_save->emplace_back(
        std::make_pair(id, shared_credential_proto));

    keys_to_not_delete.insert(id);
  }

  switch (public_credential_type) {
    case (mojom::PublicCredentialType::kLocalPublicCredential):
      // In the chain of callbacks, attempt to first save public credentials.
      // If successful, then attempt to save private credentials in a
      // follow-up callback. Iff both operations are successful,
      // 'on_credentials_fully_saved_callback' will return kOk.
      local_public_db_->UpdateEntriesWithRemoveFilter(
          /*entries_to_save=*/std::move(credential_pairs_to_save),
          /*delete_key_filter=*/
          base::BindRepeating(&ShouldDeleteEntry,
                              std::move(keys_to_not_delete)),
          base::BindOnce(
              &NearbyPresenceCredentialStorage::OnLocalPublicCredentialsSaved,
              weak_ptr_factory_.GetWeakPtr(), std::move(local_credentials),
              std::move(on_credentials_fully_saved_callback)));
      break;
    case (mojom::PublicCredentialType::kRemotePublicCredential):
      // The only remote credentials we can receive are public credentials
      // (we can't see other devices' private credentials). Thus, in the
      // remote case, only public credentials need to be saved.
      remote_public_db_->UpdateEntriesWithRemoveFilter(
          /*entries_to_save=*/std::move(credential_pairs_to_save),
          /*delete_key_filter=*/
          base::BindRepeating(&ShouldDeleteEntry,
                              std::move(keys_to_not_delete)),
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

  auto on_credentials_received_callback = base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPublicCredentialsRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      /*retrieval_start_time=*/base::TimeTicks::Now(), public_credential_type);

  switch (public_credential_type) {
    case (mojom::PublicCredentialType::kLocalPublicCredential):
      local_public_db_->LoadEntries(
          std::move(on_credentials_received_callback));
      break;
    case (mojom::PublicCredentialType::kRemotePublicCredential):
      remote_public_db_->LoadEntries(
          std::move(on_credentials_received_callback));
      break;
  }
}

void NearbyPresenceCredentialStorage::GetPrivateCredentials(
    GetPrivateCredentialsCallback callback) {
  CHECK(callback);
  private_db_->LoadEntries(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnPrivateCredentialsRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      /*retrieval_start_time=*/base::TimeTicks::Now()));
}

void NearbyPresenceCredentialStorage::UpdateLocalCredential(
    mojom::LocalCredentialPtr local_credential,
    UpdateLocalCredentialCallback callback) {
  CHECK(callback);

  ::nearby::internal::LocalCredential local_credential_proto =
      proto::LocalCredentialFromMojom(local_credential.get());

  // |UpdateEntriesWithRemoveFilter()| expects a unique_ptr, so we cannot
  // create a vector with a single pair in-line using an initializer list.
  auto credential_pair_to_update = std::make_unique<std::vector<
      std::pair<std::string, ::nearby::internal::LocalCredential>>>();
  std::string id = base::NumberToString(local_credential_proto.id());
  credential_pair_to_update->emplace_back(
      std::make_pair(id, local_credential_proto));

  // Only match the credential being updated.
  leveldb_proto::KeyFilter update_filter = base::BindRepeating(
      [](const std::string& key, const std::string& target_key) {
        return key == target_key;
      },
      id);

  // TODO(b/333701895): Verify that this works as expected during a broadcast.
  private_db_->UpdateEntriesWithRemoveFilter(
      /*entries_to_save=*/std::move(credential_pair_to_update),
      /*entries_to_remove=*/update_filter,
      base::BindOnce(&NearbyPresenceCredentialStorage::OnLocalCredentialUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NearbyPresenceCredentialStorage::OnLocalCredentialUpdated(
    UpdateLocalCredentialCallback callback,
    bool success) {
  CHECK(callback);

  if (!success) {
    LOG(ERROR) << __func__ << ": failed to update private credential.";
    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kAborted);
    return;
  }

  std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kOk);
}

void NearbyPresenceCredentialStorage::OnPrivateCredentialsRetrieved(
    GetPrivateCredentialsCallback callback,
    base::TimeTicks retrieval_start_time,
    bool success,
    std::unique_ptr<std::vector<::nearby::internal::LocalCredential>> entries) {
  CHECK(callback);

  metrics::RecordCredentialStorageRetrievePrivateCredentialsResult(success);
  if (success) {
    base::TimeDelta retrieval_duration =
        base::TimeTicks::Now() - retrieval_start_time;
    metrics::RecordCredentialStorageRetrievePrivateCredentialsDuration(
        retrieval_duration);
  } else {
    LOG(ERROR) << __func__ << ": failed to retrieve private credentials";
    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kAborted,
                            std::nullopt);
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
    base::TimeTicks retrieval_start_time,
    mojom::PublicCredentialType public_credential_type,
    bool success,
    std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
        entries) {
  CHECK(callback);

  switch (public_credential_type) {
    case (mojom::PublicCredentialType::kLocalPublicCredential):
      metrics::RecordCredentialStorageRetrieveLocalPublicCredentialsResult(
          success);
      break;
    case (mojom::PublicCredentialType::kRemotePublicCredential):
      metrics::RecordCredentialStorageRetrieveRemotePublicCredentialsResult(
          success);
      break;
  }
  if (success) {
    base::TimeDelta retrieval_duration =
        base::TimeTicks::Now() - retrieval_start_time;
    switch (public_credential_type) {
      case (mojom::PublicCredentialType::kLocalPublicCredential):
        metrics::RecordCredentialStorageRetrieveLocalPublicCredentialsDuration(
            retrieval_duration);
        break;
      case (mojom::PublicCredentialType::kRemotePublicCredential):
        metrics::RecordCredentialStorageRetrieveRemotePublicCredentialsDuration(
            retrieval_duration);
        break;
    }
  } else {
    LOG(ERROR) << __func__ << ": failed to retrieve public credentials";
    std::move(callback).Run(mojo_base::mojom::AbslStatusCode::kAborted,
                            std::nullopt);
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

  metrics::RecordCredentialStorageSaveLocalPublicCredentialsResult(
      /*success=*/success);

  // If local public credentials failed to save, skip saving the
  // private credentials.
  if (!success) {
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
  base::flat_set<std::string> keys_to_not_delete;
  for (const auto& local_credential : proto_local_credentials) {
    std::string id = base::NumberToString(local_credential.id());
    credential_pairs_to_save->emplace_back(
        std::make_pair(id, local_credential));
    keys_to_not_delete.insert(id);
  }

  private_db_->UpdateEntriesWithRemoveFilter(
      /*entries_to_save=*/std::move(credential_pairs_to_save),
      /*delete_key_filter=*/
      base::BindRepeating(&ShouldDeleteEntry, keys_to_not_delete),
      base::BindOnce(
          &NearbyPresenceCredentialStorage::OnPrivateCredentialsSaved,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(on_credentials_fully_saved_callback)));
}

void NearbyPresenceCredentialStorage::OnRemotePublicCredentialsSaved(
    SaveCredentialsCallback on_credentials_fully_saved_callback,
    bool success) {
  metrics::RecordCredentialStorageSaveRemotePublicCredentialsResult(
      /*success=*/success);

  mojo_base::mojom::AbslStatusCode save_status;
  if (success) {
    save_status = mojo_base::mojom::AbslStatusCode::kOk;
  } else {
    LOG(ERROR) << __func__ << ": failed to save remote public credentials";
    save_status = mojo_base::mojom::AbslStatusCode::kAborted;
  }

  CHECK(on_credentials_fully_saved_callback);
  std::move(on_credentials_fully_saved_callback).Run(save_status);
}

void NearbyPresenceCredentialStorage::OnPrivateCredentialsSaved(
    SaveCredentialsCallback on_credentials_fully_saved_callback,
    bool success) {
  metrics::RecordCredentialStorageSavePrivateCredentialsResult(
      /*success=*/success);

  mojo_base::mojom::AbslStatusCode save_status;
  if (success) {
    save_status = mojo_base::mojom::AbslStatusCode::kOk;
  } else {
    LOG(ERROR) << __func__ << ": failed to save private credentials";
    save_status = mojo_base::mojom::AbslStatusCode::kAborted;
  }

  CHECK(on_credentials_fully_saved_callback);
  std::move(on_credentials_fully_saved_callback).Run(save_status);
}

void NearbyPresenceCredentialStorage::OnPrivateDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    base::TimeTicks initialization_start_time,
    leveldb_proto::Enums::InitStatus private_db_initialization_status) {
  if (private_db_initialization_status ==
      leveldb_proto::Enums::InitStatus::kOK) {
    metrics::RecordCredentialStoragePrivateInitializationResult(
        /*success=*/true);

    base::TimeDelta initialization_duration =
        base::TimeTicks::Now() - initialization_start_time;
    metrics::RecordCredentialStoragePrivateDatabaseInitializationDuration(
        initialization_duration);
  } else {
    // If the private initialization failed, do not attempt to initialize the
    // public databases.
    metrics::RecordCredentialStoragePrivateInitializationResult(
        /*success=*/false);
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
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized),
      /*initialization_start_time=*/base::TimeTicks::Now()));
}

void NearbyPresenceCredentialStorage::OnLocalPublicDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    base::TimeTicks initialization_start_time,
    leveldb_proto::Enums::InitStatus local_public_db_initialization_status) {
  if (local_public_db_initialization_status ==
      leveldb_proto::Enums::InitStatus::kOK) {
    metrics::RecordCredentialStorageLocalPublicInitializationResult(
        /*success=*/true);

    base::TimeDelta initialization_duration =
        base::TimeTicks::Now() - initialization_start_time;
    metrics::RecordCredentialStorageLocalPublicDatabaseInitializationDuration(
        initialization_duration);
  } else {
    // If the local public initialization failed, do not attempt to initialize
    // the remote public database.
    metrics::RecordCredentialStorageLocalPublicInitializationResult(
        /*success=*/false);
    LOG(ERROR) << __func__
               << ": failed to initialize local public credential database "
                  "with initialization status: "
               << local_public_db_initialization_status;
    std::move(on_fully_initialized).Run(/*success=*/false);
    return;
  }

  remote_public_db_->Init(base::BindOnce(
      &NearbyPresenceCredentialStorage::OnRemotePublicDatabaseInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_fully_initialized),
      /*initialization_start_time=*/base::TimeTicks::Now()));
}

void NearbyPresenceCredentialStorage::OnRemotePublicDatabaseInitialized(
    base::OnceCallback<void(bool)> on_fully_initialized,
    base::TimeTicks initialization_start_time,
    leveldb_proto::Enums::InitStatus remote_public_db_initialization_status) {
  if (remote_public_db_initialization_status ==
      leveldb_proto::Enums::InitStatus::kOK) {
    metrics::RecordCredentialStorageRemotePublicInitializationResult(
        /*success=*/true);

    base::TimeDelta initialization_duration =
        base::TimeTicks::Now() - initialization_start_time;
    metrics::RecordCredentialStorageRemotePublicDatabaseInitializationDuration(
        initialization_duration);
  } else {
    metrics::RecordCredentialStorageRemotePublicInitializationResult(
        /*success=*/false);
    LOG(ERROR) << __func__
               << ": failed to initialize remote public credential database "
                  "with initialization status: "
               << remote_public_db_initialization_status;
    std::move(on_fully_initialized).Run(/*success=*/false);
    return;
  }

  CHECK(pending_receiver_);
  // All databases were successfully initialized, so it's safe to process
  // interface calls.
  receiver_.Bind(std::move(pending_receiver_));

  std::move(on_fully_initialized).Run(/*success=*/true);

  RecordCredentialsCountAndSize();
}

void NearbyPresenceCredentialStorage::RecordCredentialsCountAndSize() {
  local_public_db_->LoadEntries(
      base::BindOnce(&NearbyPresenceCredentialStorage::
                         RecordLocalSharedCredentialsCountAndSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialStorage::RecordLocalSharedCredentialsCountAndSize(
    bool success,
    std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
        entries) {
  if (!success) {
    LOG(ERROR)
        << __func__
        << ": failed to load entries for local shared credential database.";
    return;
  }

  metrics::RecordNumberOfLocalSharedCredentials(entries->size());

  size_t size_of_credentials_in_bytes = 0;
  for (const auto& entry : *entries) {
    size_of_credentials_in_bytes += entry.ByteSizeLong();
  }
  metrics::RecordSizeOfLocalSharedCredentials(size_of_credentials_in_bytes);

  remote_public_db_->LoadEntries(
      base::BindOnce(&NearbyPresenceCredentialStorage::
                         RecordRemoteSharedCredentialsCountAndSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceCredentialStorage::RecordRemoteSharedCredentialsCountAndSize(
    bool success,
    std::unique_ptr<std::vector<::nearby::internal::SharedCredential>>
        entries) {
  if (!success) {
    LOG(ERROR)
        << __func__
        << ": failed to load entries for remote shared credential database.";
    return;
  }

  metrics::RecordNumberOfRemoteSharedCredentials(entries->size());

  size_t size_of_credentials_in_bytes = 0;
  for (const auto& entry : *entries) {
    size_of_credentials_in_bytes += entry.ByteSizeLong();
  }
  metrics::RecordSizeOfRemoteSharedCredentials(size_of_credentials_in_bytes);
}

}  // namespace ash::nearby::presence
