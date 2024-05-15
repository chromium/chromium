// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_config_android.h"

#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android.h"
#include "chrome/browser/android/persisted_tab_data/leveldb_persisted_tab_data_storage_android_factory.h"
#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"
#include "chrome/browser/profiles/profile.h"

namespace {
const char kSensitivityId[] = "sensitivity";
}  // namespace

PersistedTabDataConfigAndroid::~PersistedTabDataConfigAndroid() = default;

PersistedTabDataConfigAndroid::PersistedTabDataConfigAndroid(
    PersistedTabDataStorageAndroid* persisted_tab_data_storage_android,
    const char* data_id)
    : persisted_tab_data_storage_(persisted_tab_data_storage_android),
      data_id_(data_id) {}

std::unique_ptr<PersistedTabDataConfigAndroid>
PersistedTabDataConfigAndroid::Get(const void* user_data_key,
                                   Profile* profile) {
  if (user_data_key == SensitivityPersistedTabDataAndroid::UserDataKey()) {
    return std::make_unique<PersistedTabDataConfigAndroid>(
        LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
            ->GetForBrowserContext(profile),
        kSensitivityId);
  }
  NOTREACHED_IN_MIGRATION() << "Unknown UserDataKey";
  return nullptr;
}

std::unique_ptr<std::vector<PersistedTabDataStorageAndroid*>>
PersistedTabDataConfigAndroid::GetAllStorage(Profile* profile) {
  std::unique_ptr<std::vector<PersistedTabDataStorageAndroid*>> storage =
      std::make_unique<std::vector<PersistedTabDataStorageAndroid*>>();
  if (profile) {
    DCHECK(LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
               ->GetForBrowserContext(profile));
    storage->push_back(
        LevelDBPersistedTabDataStorageAndroidFactory::GetInstance()
            ->GetForBrowserContext(profile));
  }
  return storage;
}
