// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_PROFILE_PROTO_DB_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_PROFILE_PROTO_DB_H_

#include <queue>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/browser_context.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"

class ProfileProtoDBTest;

template <typename T>
class ProfileProtoDBFactory;

// General purpose per profile, per proto key -> proto database where the
// template is the proto which is being stored. A ProfileProtoDB should be
// acquired using ProfileProtoDBFactory. ProfileProtoDB is a wrapper on top of
// leveldb_proto which:
// - Is specifically for databases which are per profile and per proto
//   (leveldb_proto is a proto database which may or may not be per profile).
// - Provides a simplified interface for the use cases that surround
//   ProfileProtoDB such as providing LoadContentWithPrefix instead of the
//   more generic API in
//   leveldb_proto which requires a filter to be passed in.
// - Is a KeyedService to support the per profile nature of the database.
template <typename T>
class ProfileProtoDB : public KeyedService {
 public:
  using KeyAndValue = std::pair<std::string, T>;

  // Callback which is used when content is acquired.
  using LoadCallback = base::OnceCallback<void(bool, std::vector<KeyAndValue>)>;

  // Used for confirming an operation was completed successfully (e.g.
  // insert, delete). This will be invoked on a different SequenceRunner
  // to ProfileProtoDB.
  using OperationCallback = base::OnceCallback<void(bool)>;

  // Represents an entry in the database.
  using ContentEntry = typename leveldb_proto::ProtoDatabase<T>::KeyEntryVector;

  ProfileProtoDB(const ProfileProtoDB&) = delete;
  ProfileProtoDB& operator=(const ProfileProtoDB&) = delete;
  ~ProfileProtoDB() override;

  // Loads the entry for the key and passes it to the callback.
  void LoadOneEntry(const std::string& key, LoadCallback callback);

  // Loads all entries within the databse and passes them to the callback.
  void LoadAllEntries(LoadCallback callback);

  // Loads the content data matching a prefix for the key and passes them to the
  // callback.
  void LoadContentWithPrefix(const std::string& key_prefix,
                             LoadCallback callback);

  // Inserts a value for a given key and passes the result (success/failure) to
  // OperationCallback.
  void InsertContent(const std::string& key,
                     const T& value,
                     OperationCallback callback);

  // Deletes the entry with certain key in the database.
  void DeleteOneEntry(const std::string& key, OperationCallback callback);

  // Deletes content in the database, matching all keys which have a prefix
  // that matches the key.
  void DeleteContentWithPrefix(const std::string& key_prefix,
                               OperationCallback callback);

  // Delete all content in the database.
  void DeleteAllContent(OperationCallback callback);

  // Destroy the cached instance of the database (databases are cached per
  // profile).
  void Destroy() const;

 private:
  friend class ::ProfileProtoDBTest;
  template <typename U>
  friend class ::ProfileProtoDBFactory;

  // Initializes the database.
  ProfileProtoDB(content::BrowserContext* browser_context,
                 leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
                 const base::FilePath& database_dir,
                 leveldb_proto::ProtoDbType proto_db_type);

  // Used for testing.
  ProfileProtoDB(
      std::unique_ptr<leveldb_proto::ProtoDatabase<T>> storage_database,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Passes back database status following database initialization.
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);

  // Callback when one entry is loaded.
  void OnLoadOneEntry(LoadCallback callback,
                      bool success,
                      std::unique_ptr<T> entry);

  // Callback when content is loaded.
  void OnLoadContent(LoadCallback callback,
                     bool success,
                     std::unique_ptr<std::vector<T>> content);

  // Callback when an operation (e.g. insert or delete) is called.
  void OnOperationCommitted(OperationCallback callback, bool success);

  // Returns true if initialization status of database is not yet known.
  bool InitStatusUnknown() const;

  // Returns true if the database failed to initialize.
  bool FailedToInit() const;

  // Browser context associated with ProfileProtoDB (ProfileProtoDB are per
  // profile).
  content::BrowserContext* browser_context_;

  // Status of the database initialization.
  base::Optional<leveldb_proto::Enums::InitStatus> database_status_;

  // The database for storing content storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<T>> storage_database_;

  // Store operations until the database is initialized at which point
  // |deferred_operations_| is flushed and all operations are executed.
  std::vector<base::OnceClosure> deferred_operations_;

  base::WeakPtrFactory<ProfileProtoDB> weak_ptr_factory_{this};
};

namespace {

leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

}  // namespace

template <typename T>
ProfileProtoDB<T>::~ProfileProtoDB() = default;

template <typename T>
void ProfileProtoDB<T>::LoadOneEntry(const std::string& key,
                                     LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &ProfileProtoDB::LoadOneEntry, weak_ptr_factory_.GetWeakPtr(), key,
        std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->GetEntry(
        key,
        base::BindOnce(&ProfileProtoDB::OnLoadOneEntry,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void ProfileProtoDB<T>::LoadAllEntries(LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(
        base::BindOnce(&ProfileProtoDB::LoadAllEntries,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->LoadEntries(
        base::BindOnce(&ProfileProtoDB::OnLoadContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void ProfileProtoDB<T>::LoadContentWithPrefix(const std::string& key_prefix,
                                              LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &ProfileProtoDB::LoadContentWithPrefix, weak_ptr_factory_.GetWeakPtr(),
        key_prefix, std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->LoadEntriesWithFilter(
        base::BindRepeating(&DatabasePrefixFilter, key_prefix),
        CreateReadOptions(),
        /* target_prefix */ "",
        base::BindOnce(&ProfileProtoDB::OnLoadContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

// Inserts a value for a given key and passes the result (success/failure) to
// OperationCallback.
template <typename T>
void ProfileProtoDB<T>::InsertContent(const std::string& key,
                                      const T& value,
                                      OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &ProfileProtoDB::InsertContent, weak_ptr_factory_.GetWeakPtr(), key,
        std::move(value), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    auto contents_to_save = std::make_unique<ContentEntry>();
    contents_to_save->emplace_back(key, value);
    storage_database_->UpdateEntries(
        std::move(contents_to_save),
        std::make_unique<std::vector<std::string>>(),
        base::BindOnce(&ProfileProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void ProfileProtoDB<T>::DeleteOneEntry(const std::string& key,
                                       OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &ProfileProtoDB::DeleteOneEntry, weak_ptr_factory_.GetWeakPtr(), key,
        std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    auto keys = std::make_unique<std::vector<std::string>>();
    keys->push_back(key);
    storage_database_->UpdateEntries(
        std::make_unique<ContentEntry>(), std::move(keys),
        base::BindOnce(&ProfileProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

// Deletes content in the database, matching all keys which have a prefix
// that matches the key.
template <typename T>
void ProfileProtoDB<T>::DeleteContentWithPrefix(const std::string& key_prefix,
                                                OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &ProfileProtoDB::DeleteContentWithPrefix,
        weak_ptr_factory_.GetWeakPtr(), key_prefix, std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    storage_database_->UpdateEntriesWithRemoveFilter(
        std::make_unique<ContentEntry>(),
        std::move(base::BindRepeating(&DatabasePrefixFilter, key_prefix)),
        base::BindOnce(&ProfileProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

// Delete all content in the database.
template <typename T>
void ProfileProtoDB<T>::DeleteAllContent(OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(
        base::BindOnce(&ProfileProtoDB::DeleteAllContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    storage_database_->Destroy(std::move(callback));
  }
}

template <typename T>
void ProfileProtoDB<T>::Destroy() const {
  ProfileProtoDBFactory<T>::GetInstance()->Disassociate(browser_context_);
}

template <typename T>
ProfileProtoDB<T>::ProfileProtoDB(
    content::BrowserContext* browser_context,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_dir,
    leveldb_proto::ProtoDbType proto_db_type)
    : browser_context_(browser_context),
      database_status_(base::nullopt),
      storage_database_(proto_database_provider->GetDB<T>(
          proto_db_type,
          database_dir,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}))) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must implement 'google::protobuf::MessageLite'");
  storage_database_->Init(base::BindOnce(&ProfileProtoDB::OnDatabaseInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

// Used for tests.
template <typename T>
ProfileProtoDB<T>::ProfileProtoDB(
    std::unique_ptr<leveldb_proto::ProtoDatabase<T>> storage_database,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : database_status_(base::nullopt),
      storage_database_(std::move(storage_database)) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must implement 'google::protobuf::MessageLite'");
  storage_database_->Init(base::BindOnce(&ProfileProtoDB::OnDatabaseInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

// Passes back database status following database initialization.
template <typename T>
void ProfileProtoDB<T>::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  database_status_ =
      base::make_optional<leveldb_proto::Enums::InitStatus>(status);
  for (auto& deferred_operation : deferred_operations_) {
    std::move(deferred_operation).Run();
  }
  deferred_operations_.clear();
}

// Callback when one entry is loaded.
template <typename T>
void ProfileProtoDB<T>::OnLoadOneEntry(LoadCallback callback,
                                       bool success,
                                       std::unique_ptr<T> entry) {
  std::vector<KeyAndValue> results;
  if (success && entry) {
    results.emplace_back(entry->key(), *entry);
  }
  std::move(callback).Run(success, std::move(results));
}

// Callback when content is loaded.
template <typename T>
void ProfileProtoDB<T>::OnLoadContent(LoadCallback callback,
                                      bool success,
                                      std::unique_ptr<std::vector<T>> content) {
  std::vector<KeyAndValue> results;
  if (success) {
    for (const auto& proto : *content) {
      // TODO(crbug.com/1157881) relax requirement for proto to have a key field
      // and return key value pairs OnLoadContent.
      results.emplace_back(proto.key(), proto);
    }
  }
  std::move(callback).Run(success, std::move(results));
}

// Callback when an operation (e.g. insert or delete) is called.
template <typename T>
void ProfileProtoDB<T>::OnOperationCommitted(OperationCallback callback,
                                             bool success) {
  std::move(callback).Run(success);
}

// Returns true if initialization status of database is not yet known.
template <typename T>
bool ProfileProtoDB<T>::InitStatusUnknown() const {
  return database_status_ == base::nullopt;
}

// Returns true if the database failed to initialize.
template <typename T>
bool ProfileProtoDB<T>::FailedToInit() const {
  return database_status_.has_value() &&
         database_status_.value() != leveldb_proto::Enums::InitStatus::kOK;
}

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_PROFILE_PROTO_DB_H_
