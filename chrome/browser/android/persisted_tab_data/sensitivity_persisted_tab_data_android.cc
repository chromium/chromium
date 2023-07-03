// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

#include "chrome/browser/android/persisted_tab_data/sensitivity_data.pb.h"
#include "components/search_engines/template_url_service.h"

SensitivityPersistedTabDataAndroid::SensitivityPersistedTabDataAndroid(
    TabAndroid* tab_android)
    : PersistedTabDataAndroid(
          tab_android,
          SensitivityPersistedTabDataAndroid::UserDataKey()),
      tab_(tab_android) {}

SensitivityPersistedTabDataAndroid::~SensitivityPersistedTabDataAndroid() =
    default;

std::unique_ptr<const std::vector<uint8_t>>
SensitivityPersistedTabDataAndroid::Serialize() {
  sensitivity::SensitivityData sensitivity_data;
  sensitivity_data.set_is_sensitive(is_sensitive_);
  std::unique_ptr<std::vector<uint8_t>> data =
      std::make_unique<std::vector<uint8_t>>(sensitivity_data.ByteSize());
  sensitivity_data.SerializeToArray(data->data(), sensitivity_data.ByteSize());
  return data;
}

void SensitivityPersistedTabDataAndroid::Deserialize(
    const std::vector<uint8_t>& data) {
  sensitivity::SensitivityData sensitivity_data;
  if (!sensitivity_data.ParseFromArray(data.data(), data.size())) {
    sensitivity_data.Clear();
  }
  is_sensitive_ = sensitivity_data.is_sensitive();
}

void SensitivityPersistedTabDataAndroid::OnPageContentAnnotated(
    const GURL& url,
    const optimization_guide::PageContentAnnotationsResult& result) {
  // TODO(crbug.com/1458487) implement is_sensitive data acquisition.
}

TAB_ANDROID_USER_DATA_KEY_IMPL(SensitivityPersistedTabDataAndroid)
