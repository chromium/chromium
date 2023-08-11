// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_CONFIG_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_CONFIG_ANDROID_H_

#include <memory>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_storage_android.h"

class Profile;

// Maps each PersistedTabData client to a storage implementation and unique
// identifier (to avoid collisions between clients who use the same storage)
class PersistedTabDataConfigAndroid {
 public:
  PersistedTabDataConfigAndroid(
      PersistedTabDataStorageAndroid* persisted_tab_data_storage_android,
      const char* data_id);
  ~PersistedTabDataConfigAndroid();
  static std::unique_ptr<PersistedTabDataConfigAndroid> Get(
      const void* user_data_key,
      Profile* profile);

  PersistedTabDataStorageAndroid* persisted_tab_data_storage_android() {
    return persisted_tab_data_storage_.get();
  }
  const char* data_id() { return data_id_; }

  static std::unique_ptr<std::vector<PersistedTabDataStorageAndroid*>>
  GetAllStorage(Profile* profile);

 private:
  raw_ptr<PersistedTabDataStorageAndroid> persisted_tab_data_storage_;
  raw_ptr<const char> data_id_;
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_CONFIG_ANDROID_H_
