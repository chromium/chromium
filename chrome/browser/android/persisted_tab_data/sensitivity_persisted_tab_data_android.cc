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

void SensitivityPersistedTabDataAndroid::RegisterPCAService(
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service) {
  DCHECK(page_content_annotations_service);
  if (page_content_annotations_service_ == page_content_annotations_service) {
    return;
  }

  page_content_annotations_service_ = page_content_annotations_service;
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

SensitivityPersistedTabDataAndroid::~SensitivityPersistedTabDataAndroid() {
  if (page_content_annotations_service_ != nullptr) {
    page_content_annotations_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kContentVisibility, this);
  }
}

void SensitivityPersistedTabDataAndroid::From(
    TabAndroid* tab_android,
    PersistedTabDataAndroid::FromCallback from_callback) {
  PersistedTabDataAndroid::From(
      tab_android->GetWeakPtr(),
      SensitivityPersistedTabDataAndroid::UserDataKey(),
      base::BindOnce([](TabAndroid* tab_android)
                         -> std::unique_ptr<PersistedTabDataAndroid> {
        return std::make_unique<SensitivityPersistedTabDataAndroid>(
            tab_android);
      }),
      std::move(from_callback));
}

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
    const page_content_annotations::PageContentAnnotationsResult& result) {
  if (tab_->GetURL() != url) {
    return;
  }
  // Setting the cutoff value to 0.5 for binary classification of data
  // sensitivity. This value ensures that we neither overclassify nor
  // underclassify sensitive data
  set_is_sensitive(result.GetContentVisibilityScore() < 0.5);
}

void SensitivityPersistedTabDataAndroid::ExistsForTesting(
    TabAndroid* tab_android,
    base::OnceCallback<void(bool)> exists_callback) {
  PersistedTabDataAndroid::ExistsForTesting(
      tab_android, SensitivityPersistedTabDataAndroid::UserDataKey(),
      std::move(exists_callback));
}

TAB_ANDROID_USER_DATA_KEY_IMPL(SensitivityPersistedTabDataAndroid)
