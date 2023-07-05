// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_config_android.h"

#include "base/no_destructor.h"
#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

PersistedTabDataConfigAndroid::~PersistedTabDataConfigAndroid() = default;

PersistedTabDataConfigAndroid::PersistedTabDataConfigAndroid(
    std::unique_ptr<PersistedTabDataStorageAndroid>
        persisted_tab_data_storage_android,
    const std::string* data_id)
    : persisted_tab_data_storage_(
          std::move(persisted_tab_data_storage_android)),
      data_id_(data_id) {}

PersistedTabDataConfigAndroid* PersistedTabDataConfigAndroid::Get(
    const void* user_data_key) {
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
