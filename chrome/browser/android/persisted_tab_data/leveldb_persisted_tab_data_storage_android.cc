// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

// Callback for inserting and deleting content.
void OnUpdateCallback(bool success) {
  if (!success) {
    LOG(WARNING) << "There was an error modifying PersistedStateDB";
  }
}

void OnLoadCallback(
    PersistedTabDataStorageAndroid::RestoreCallback restore_callback,
    bool success,
    std::vector<SessionProtoDB<
        persisted_state_db::PersistedStateContentProto>::KeyAndValue> data) {
  if (!success) {
    LOG(WARNING) << "There was an error loading from PersistedStateDB";
    data.clear();
  }
  // The level DB database matches prefixes, but we are interested in an exact
  // match so we just want the first value in the event that an entry is found.
  std::string res_str = data.empty() ? "" : data[0].second.content_data();
  const std::vector<uint8_t> res(res_str.begin(), res_str.end());
  std::move(restore_callback).Run(res);
}

const std::string GetKey(int tab_id, const char* data_id) {
  return base::StringPrintf("%d-%s", tab_id, data_id);
}

const std::string GetKeyAll(int tab_id) {
  return base::StringPrintf("%d-", tab_id);
}

}  // namespace

LevelDBPersistedTabDataStorageAndroid::
    ~LevelDBPersistedTabDataStorageAndroid() = default;

void LevelDBPersistedTabDataStorageAndroid::Save(
    int tab_id,
    const char* data_id,
    const std::vector<uint8_t>& data) {
  persisted_state_db::PersistedStateContentProto proto;
  std::string key = GetKey(tab_id, data_id);
  proto.set_key(key);
  proto.set_content_data(data.data(), data.size());
  proto_db_->InsertContent(key, proto, base::BindOnce(&OnUpdateCallback));
}

void LevelDBPersistedTabDataStorageAndroid::Restore(
    int tab_id,
    const char* data_id,
    RestoreCallback restore_callback) {
  proto_db_->LoadContentWithPrefix(
      GetKey(tab_id, data_id),
      base::BindOnce(&OnLoadCallback, std::move(restore_callback)));
}

void LevelDBPersistedTabDataStorageAndroid::Remove(int tab_id,
                                                   const char* data_id) {
  proto_db_->DeleteContentWithPrefix(GetKey(tab_id, data_id),
                                     base::BindOnce(&OnUpdateCallback));
}

void LevelDBPersistedTabDataStorageAndroid::RemoveAll(int tab_id) {
  proto_db_->DeleteContentWithPrefix(GetKeyAll(tab_id),
                                     base::BindOnce(&OnUpdateCallback));
}

LevelDBPersistedTabDataStorageAndroid::LevelDBPersistedTabDataStorageAndroid(
    Profile* profile)
    : proto_db_(
          SessionProtoDBFactory<
              persisted_state_db::PersistedStateContentProto>::GetInstance()
              ->GetForProfile(profile)) {}
