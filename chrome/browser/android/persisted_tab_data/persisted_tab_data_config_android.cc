// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_config_android.h"

PersistedTabDataConfigAndroid::~PersistedTabDataConfigAndroid() = default;

PersistedTabDataConfigAndroid::PersistedTabDataConfigAndroid(
    std::unique_ptr<PersistedTabDataStorageAndroid>
        persisted_tab_data_storage_android,
    const std::string* data_id)
    : persisted_tab_data_storage_(
          std::move(persisted_tab_data_storage_android)),
      data_id_(data_id) {}
