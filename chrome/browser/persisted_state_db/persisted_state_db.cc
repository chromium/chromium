// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/persisted_state_db.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"

#if defined(OS_ANDROID)

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/persisted_state_db/persisted_state_db_factory.h"
#include "chrome/browser/tab/jni_headers/LevelDBPersistedTabDataStorage_jni.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"

#endif  // defined(OS_ANDROID)

namespace {

const char kPersistedStateDBFolder[] = "persisted_state_db";
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

PersistedStateDB::~PersistedStateDB() = default;

void PersistedStateDB::LoadContent(const std::string& key,
                                   LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &PersistedStateDB::LoadContent, weak_ptr_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->LoadEntriesWithFilter(
        base::BindRepeating(&DatabasePrefixFilter, key), CreateReadOptions(),
        /* target_prefix */ "",
        base::BindOnce(&PersistedStateDB::OnLoadContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void PersistedStateDB::InsertContent(const std::string& key,
                                     const std::vector<uint8_t>& value,
                                     OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &PersistedStateDB::InsertContent, weak_ptr_factory_.GetWeakPtr(),
        std::move(key), std::move(value), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    auto contents_to_save = std::make_unique<ContentEntry>();
    persisted_state_db::PersistedStateContentProto proto;
    proto.set_key(key);
    proto.set_content_data(value.data(), value.size());
    contents_to_save->emplace_back(proto.key(), std::move(proto));
    storage_database_->UpdateEntries(
        std::move(contents_to_save),
        std::make_unique<std::vector<std::string>>(),
        base::BindOnce(&PersistedStateDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void PersistedStateDB::DeleteContent(const std::string& key,
                                     OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &PersistedStateDB::DeleteContent, weak_ptr_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    storage_database_->UpdateEntriesWithRemoveFilter(
        std::make_unique<ContentEntry>(),
        std::move(base::BindRepeating(&DatabasePrefixFilter, std::move(key))),
        base::BindOnce(&PersistedStateDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void PersistedStateDB::DeleteAllContent(OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(
        base::BindOnce(&PersistedStateDB::DeleteAllContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else if (FailedToInit()) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), false));
  } else {
    storage_database_->Destroy(std::move(callback));
  }
}

PersistedStateDB::PersistedStateDB(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_directory)
    : database_status_(base::nullopt),
      storage_database_(
          proto_database_provider
              ->GetDB<persisted_state_db::PersistedStateContentProto>(
                  leveldb_proto::ProtoDbType::PERSISTED_STATE_DATABASE,
                  profile_directory.AppendASCII(kPersistedStateDBFolder),
                  base::ThreadPool::CreateSequencedTaskRunner(
                      {base::MayBlock(), base::TaskPriority::USER_VISIBLE}))) {
  storage_database_->Init(
      base::BindOnce(&PersistedStateDB::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

PersistedStateDB::PersistedStateDB(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        persisted_state_db::PersistedStateContentProto>> storage_database,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : database_status_(base::nullopt),
      storage_database_(std::move(storage_database)) {
  storage_database_->Init(
      base::BindOnce(&PersistedStateDB::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PersistedStateDB::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  database_status_ =
      base::make_optional<leveldb_proto::Enums::InitStatus>(status);
  for (auto& deferred_operation : deferred_operations_) {
    std::move(deferred_operation).Run();
  }
  deferred_operations_.clear();
}

void PersistedStateDB::OnLoadContent(
    LoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<persisted_state_db::PersistedStateContentProto>>
        content) {
  std::vector<KeyAndValue> results;
  if (success) {
    for (const auto& proto : *content) {
      DCHECK(proto.has_key());
      DCHECK(proto.has_content_data());
      results.emplace_back(proto.key(),
                           std::vector<uint8_t>(proto.content_data().begin(),
                                                proto.content_data().end()));
    }
  }
  std::move(callback).Run(success, std::move(results));
}

void PersistedStateDB::OnOperationCommitted(OperationCallback callback,
                                            bool success) {
  std::move(callback).Run(success);
}

bool PersistedStateDB::InitStatusUnknown() const {
  return database_status_ == base::nullopt;
}

bool PersistedStateDB::FailedToInit() const {
  return database_status_.has_value() &&
         database_status_.value() != leveldb_proto::Enums::InitStatus::kOK;
}

#if defined(OS_ANDROID)

namespace {
void OnUpdateCallback(
    const base::android::JavaRef<jobject>& joncomplete_for_testing,
    bool success) {
  if (!success)
    LOG(WARNING) << "There was an error modifying PersistedStateDB";
  // Callback for save and delete is only used in tests for synchronization.
  // Otherwise the callback is a no-op.
  if (joncomplete_for_testing)
    base::android::RunRunnableAndroid(joncomplete_for_testing);
}

void OnLoadCallback(const base::android::JavaRef<jobject>& jcallback,
                    bool success,
                    std::vector<PersistedStateDB::KeyAndValue> data) {
  if (!success)
    LOG(WARNING) << "There was an error loading from PersistedStateDB";
  base::android::RunObjectCallbackAndroid(
      jcallback, base::android::ToJavaByteArray(
                     base::android::AttachCurrentThread(),
                     data.empty() ? std::vector<uint8_t>() : data[0].second));
}
}  // namespace

void PersistedStateDB::Save(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jbyteArray>& byte_array,
    const base::android::JavaRef<jobject>& joncomplete_for_testing) {
  std::vector<uint8_t> data;
  base::android::JavaByteArrayToByteVector(env, byte_array, &data);
  InsertContent(base::android::ConvertJavaStringToUTF8(env, jkey), data,
                base::BindOnce(&OnUpdateCallback,
                               base::android::ScopedJavaGlobalRef<jobject>(
                                   joncomplete_for_testing)));
}

void PersistedStateDB::Load(JNIEnv* env,
                            const base::android::JavaParamRef<jstring>& jkey,
                            const base::android::JavaRef<jobject>& jcallback) {
  LoadContent(
      base::android::ConvertJavaStringToUTF8(env, jkey),
      base::BindOnce(&OnLoadCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback)));
}

void PersistedStateDB::Delete(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaRef<jobject>& joncomplete_for_testing) {
  DeleteContent(base::android::ConvertJavaStringToUTF8(env, jkey),
                base::BindOnce(&OnUpdateCallback,
                               base::android::ScopedJavaGlobalRef<jobject>(
                                   joncomplete_for_testing)));
}

void PersistedStateDB::Destroy(JNIEnv* env) {
  delete this;
}

static void JNI_LevelDBPersistedTabDataStorage_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jprofile) {
  PersistedStateDB* persisted_state_db =
      PersistedStateDBFactory::GetInstance()->GetForProfile(
          browser_context::BrowserContextFromJavaHandle(jprofile));
  Java_LevelDBPersistedTabDataStorage_setNativePtr(
      env, obj, reinterpret_cast<intptr_t>(persisted_state_db));
}
#endif  // OS_ANDROID
