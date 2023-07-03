// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_config_android.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_storage_android.h"
#include "chrome/browser/android/tab_android.h"

PersistedTabDataAndroid::PersistedTabDataAndroid(TabAndroid* tab_android,
                                                 const void* user_data_key)
    : persisted_tab_data_storage_android_(
          PersistedTabDataConfigAndroid::Get(user_data_key)
              ->persisted_tab_data_storage_android()),
      data_id_(PersistedTabDataConfigAndroid::Get(user_data_key)->data_id()),
      tab_id_(tab_android->GetAndroidId()) {}

PersistedTabDataAndroid::~PersistedTabDataAndroid() = default;

void PersistedTabDataAndroid::From(TabAndroid* tab_android,
                                   const void* user_data_key,
                                   SupplierCallback supplier_callback,
                                   FromCallback from_callback,
                                   DeserializerCallback deserializer_callback) {
  if (tab_android->GetUserData(user_data_key)) {
    std::move(from_callback)
        .Run(static_cast<PersistedTabDataAndroid*>(
            tab_android->GetUserData(user_data_key)));
  } else {
    PersistedTabDataConfigAndroid* persisted_tab_data_config_android =
        PersistedTabDataConfigAndroid::Get(user_data_key);
    persisted_tab_data_config_android->persisted_tab_data_storage_android()
        ->Restore(
            tab_android->GetAndroidId(),
            persisted_tab_data_config_android->data_id(),
            base::BindOnce(
                [](TabAndroid* tab_android, SupplierCallback supplier_callback,
                   FromCallback from_callback,
                   DeserializerCallback deserializer_callback,
                   const void* user_data_key,
                   const std::vector<uint8_t>& data) {
                  if (data.empty()) {
                    // No PersistedTabData found - acquire using supplier.
                    std::unique_ptr<PersistedTabDataAndroid>
                        persisted_tab_data_android =
                            std::move(supplier_callback).Run();
                    // Save for re-use after restarts.
                    persisted_tab_data_android->Save();
                    tab_android->SetUserData(
                        user_data_key, std::move(persisted_tab_data_android));
                  } else {
                    // Deserialize PersistedTabData found in storage.
                    tab_android->SetUserData(
                        user_data_key,
                        std::move(deserializer_callback).Run(data));
                  }
                  std::move(from_callback)
                      .Run(static_cast<PersistedTabDataAndroid*>(
                          tab_android->GetUserData(user_data_key)));
                },
                tab_android, std::move(supplier_callback),
                std::move(from_callback), std::move(deserializer_callback),
                user_data_key));
  }
}

void PersistedTabDataAndroid::Save() {
  persisted_tab_data_storage_android_->Save(tab_id_, data_id_,
                                            *Serialize().get());
}

void PersistedTabDataAndroid::Remove() {
  persisted_tab_data_storage_android_->Remove(tab_id_, data_id_);
}

TAB_ANDROID_USER_DATA_KEY_IMPL(PersistedTabDataAndroid)
