// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_CONFIG_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_CONFIG_ANDROID_H_

#include <memory>
#include <unordered_map>

#include "base/no_destructor.h"
#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_storage_android.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

namespace {
const char kSensitivityId[] = "sensitivity";
}  // namespace

// Maps each PersistedTabData client to a storage implementation and unique
// identifier (to avoid collisions between clients who use the same storage)
class PersistedTabDataConfigAndroid {
 public:
  ~PersistedTabDataConfigAndroid();
  static PersistedTabDataConfigAndroid* Get(const void* user_data_key) {
    static base::NoDestructor<std::unordered_map<
        const void*, std::unique_ptr<PersistedTabDataConfigAndroid>>>
        lookup_;
    if (lookup_.get()->empty()) {
      lookup_.get()->emplace(
          SensitivityPersistedTabDataAndroid::UserDataKey(),
          new PersistedTabDataConfigAndroid(
              std::make_unique<LevelDBPersistedTabDataStorageAndroid>(),
              new std::string(kSensitivityId)));
    }
    return lookup_.get()->find(user_data_key)->second.get();
  }

  PersistedTabDataStorageAndroid* persisted_tab_data_storage_android() {
    return persisted_tab_data_storage_.get();
  }
  const std::string& data_id() { return *data_id_; }

 private:
  PersistedTabDataConfigAndroid(std::unique_ptr<PersistedTabDataStorageAndroid>
                                    persisted_tab_data_storage_android,
                                const std::string* data_id);
  std::unique_ptr<PersistedTabDataStorageAndroid> persisted_tab_data_storage_;
  raw_ptr<const std::string> data_id_;
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_CONFIG_ANDROID_H_
