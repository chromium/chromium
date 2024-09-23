// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/test/foo_persisted_tab_data.h"

#include "chrome/browser/android/persisted_tab_data/test/foo.pb.h"

FooPersistedTabDataAndroid::FooPersistedTabDataAndroid(TabAndroid* tab_android)
    : PersistedTabDataAndroid(tab_android,
                              FooPersistedTabDataAndroid::UserDataKey()) {}

FooPersistedTabDataAndroid::~FooPersistedTabDataAndroid() = default;

void FooPersistedTabDataAndroid::From(
    TabAndroid* tab_android,
    PersistedTabDataAndroid::FromCallback from_callback) {
  PersistedTabDataAndroid::From(
      tab_android->GetWeakPtr(), FooPersistedTabDataAndroid::UserDataKey(),
      base::BindOnce([](TabAndroid* tab_android)
                         -> std::unique_ptr<PersistedTabDataAndroid> {
        return std::make_unique<FooPersistedTabDataAndroid>(tab_android);
      }),
      std::move(from_callback));
}

void FooPersistedTabDataAndroid::SetValue(int32_t foo_value) {
  foo_value_ = foo_value;
  Save();
}

void FooPersistedTabDataAndroid::ExistsForTesting(
    TabAndroid* tab_android,
    base::OnceCallback<void(bool)> exists_callback) {
  PersistedTabDataAndroid::ExistsForTesting(
      tab_android, FooPersistedTabDataAndroid::UserDataKey(),
      std::move(exists_callback));
}

std::unique_ptr<const std::vector<uint8_t>>
FooPersistedTabDataAndroid::Serialize() {
  ptd::FooData foo_data;
  foo_data.set_value(foo_value_);
  std::unique_ptr<std::vector<uint8_t>> res =
      std::make_unique<std::vector<uint8_t>>(foo_data.ByteSize());
  foo_data.SerializeToArray(res->data(), foo_data.ByteSize());
  return res;
}

void FooPersistedTabDataAndroid::Deserialize(const std::vector<uint8_t>& data) {
  ptd::FooData foo_data;
  if (!foo_data.ParseFromArray(data.data(), data.size())) {
    foo_data.Clear();
  }
  foo_value_ = foo_data.value();
}

TAB_ANDROID_USER_DATA_KEY_IMPL(FooPersistedTabDataAndroid)
