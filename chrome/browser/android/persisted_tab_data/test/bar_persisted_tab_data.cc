// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/test/bar_persisted_tab_data.h"

#include "chrome/browser/android/persisted_tab_data/test/bar.pb.h"

BarPersistedTabDataAndroid::BarPersistedTabDataAndroid(TabAndroid* tab_android)
    : PersistedTabDataAndroid(tab_android,
                              BarPersistedTabDataAndroid::UserDataKey()) {}

BarPersistedTabDataAndroid::~BarPersistedTabDataAndroid() = default;

void BarPersistedTabDataAndroid::From(
    TabAndroid* tab_android,
    PersistedTabDataAndroid::FromCallback from_callback) {
  PersistedTabDataAndroid::From(
      tab_android->GetWeakPtr(), BarPersistedTabDataAndroid::UserDataKey(),
      base::BindOnce([](TabAndroid* tab_android)
                         -> std::unique_ptr<PersistedTabDataAndroid> {
        return std::make_unique<BarPersistedTabDataAndroid>(tab_android);
      }),
      std::move(from_callback));
}

void BarPersistedTabDataAndroid::SetValue(bool bar_value) {
  bar_value_ = bar_value;
  Save();
}

void BarPersistedTabDataAndroid::ExistsForTesting(
    TabAndroid* tab_android,
    base::OnceCallback<void(bool)> exists_callback) {
  PersistedTabDataAndroid::ExistsForTesting(
      tab_android, BarPersistedTabDataAndroid::UserDataKey(),
      std::move(exists_callback));
}

std::unique_ptr<const std::vector<uint8_t>>
BarPersistedTabDataAndroid::Serialize() {
  ptd::BarData bar_data;
  bar_data.set_value(bar_value_);
  std::unique_ptr<std::vector<uint8_t>> res =
      std::make_unique<std::vector<uint8_t>>(bar_data.ByteSize());
  bar_data.SerializeToArray(res->data(), bar_data.ByteSize());
  return res;
}

void BarPersistedTabDataAndroid::Deserialize(const std::vector<uint8_t>& data) {
  ptd::BarData bar_data;
  if (!bar_data.ParseFromArray(data.data(), data.size())) {
    bar_data.Clear();
  }
  bar_value_ = bar_data.value();
}

TAB_ANDROID_USER_DATA_KEY_IMPL(BarPersistedTabDataAndroid)
