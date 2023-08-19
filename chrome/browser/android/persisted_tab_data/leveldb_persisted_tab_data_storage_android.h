// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LEVELDB_PERSISTED_TAB_DATA_STORAGE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LEVELDB_PERSISTED_TAB_DATA_STORAGE_ANDROID_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_storage_android.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_proto_db/session_proto_db.h"

// Level DB backed implementation of PersistedTabDataStorage
class LevelDBPersistedTabDataStorageAndroid
    : public PersistedTabDataStorageAndroid,
      public KeyedService {
 public:
  ~LevelDBPersistedTabDataStorageAndroid() override;

  // Save |data| into the database for a |tab_id| and |data_id| combination.
  void Save(int tab_id,
            const char* data_id,
            const std::vector<uint8_t>& data) override;

  // Restore |data| from  the database for a |tab_id| and |data_id| combination.
  void Restore(int tab_id,
               const char* data_id,
               RestoreCallback restore_callback) override;

  // Remove entry in the database for a given |tab_id| and |data_id|.
  void Remove(int tab_id, const char* data_id) override;

  // Remove entries in the database for all PersistedTabDataAndroid for a given
  // |tab_id|
  void RemoveAll(int tab_id) override;

 private:
  friend class LevelDBPersistedTabDataStorageAndroidFactory;
  explicit LevelDBPersistedTabDataStorageAndroid(Profile* profile);

  // Per profile/per proto storage
  raw_ptr<SessionProtoDB<persisted_state_db::PersistedStateContentProto>>
      proto_db_;
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LEVELDB_PERSISTED_TAB_DATA_STORAGE_ANDROID_H_
