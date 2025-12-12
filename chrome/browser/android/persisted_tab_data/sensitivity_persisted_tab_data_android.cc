// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/sensitivity_persisted_tab_data_android.h"

#include "chrome/browser/android/persisted_tab_data/sensitivity_data.pb.h"
#include "components/content_capture/browser/onscreen_content_provider.h"

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
  if (page_content_annotations_service_ != nullptr) {
    page_content_annotations_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kContentVisibility, this);
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
      tab_android->GetTabAndroidWeakPtr(),
      SensitivityPersistedTabDataAndroid::UserDataKey(),
      base::BindOnce([](TabAndroid* tab_android)
                         -> std::unique_ptr<PersistedTabDataAndroid> {
        return std::make_unique<SensitivityPersistedTabDataAndroid>(
            tab_android);
      }),
      std::move(from_callback));
}

const void* SensitivityPersistedTabDataAndroid::UserDataKey() {
  return &SensitivityPersistedTabDataAndroid::kUserDataKey;
}

std::unique_ptr<const std::vector<uint8_t>>
SensitivityPersistedTabDataAndroid::Serialize() {
  sensitivity::SensitivityData sensitivity_data;
  sensitivity_data.set_is_sensitive(is_sensitive_);
  sensitivity_data.set_sensitivity_score(sensitivity_score_);
  std::unique_ptr<std::vector<uint8_t>> data =
      std::make_unique<std::vector<uint8_t>>(sensitivity_data.ByteSizeLong());
  sensitivity_data.SerializeToArray(data->data(), data->size());
  return data;
}

void SensitivityPersistedTabDataAndroid::Deserialize(
    const std::vector<uint8_t>& data) {
  sensitivity::SensitivityData sensitivity_data;
  if (!sensitivity_data.ParseFromArray(data.data(), data.size())) {
    sensitivity_data.Clear();
  }
  is_sensitive_ = sensitivity_data.is_sensitive();
  sensitivity_score_ = sensitivity_data.has_sensitivity_score()
                           ? sensitivity_data.sensitivity_score()
                           : -1.0;
}

void SensitivityPersistedTabDataAndroid::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  if (tab_->GetURL() != visit.url) {
    return;
  }
  set_sensitivity_score(result.GetContentVisibilityScore());

  if (!tab_->web_contents()) {
    return;
  }
  content_capture::OnscreenContentProvider* onscreen_content_provider =
      content_capture::OnscreenContentProvider::FromWebContents(
          tab_->web_contents());
  if (onscreen_content_provider) {
    onscreen_content_provider->DidUpdateSensitivityScore(
        result.GetContentVisibilityScore());
  }
}

void SensitivityPersistedTabDataAndroid::ExistsForTesting(
    TabAndroid* tab_android,
    base::OnceCallback<void(bool)> exists_callback) {
  PersistedTabDataAndroid::ExistsForTesting(
      tab_android, SensitivityPersistedTabDataAndroid::UserDataKey(),
      std::move(exists_callback));
}

TAB_ANDROID_USER_DATA_KEY_IMPL(SensitivityPersistedTabDataAndroid)
