// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/persisted_tab_data/language_persisted_tab_data_android.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/persisted_tab_data/language_data.pb.h"
#include "chrome/browser/android/tab_android.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

LanguagePersistedTabDataAndroid::LanguagePersistedTabDataAndroid(
    TabAndroid* tab_android)
    : PersistedTabDataAndroid(tab_android,
                              LanguagePersistedTabDataAndroid::UserDataKey()),
      tab_(tab_android) {}

LanguagePersistedTabDataAndroid::~LanguagePersistedTabDataAndroid() {
  if (translate_driver_) {
    translate_driver_->RemoveLanguageDetectionObserver(this);
  }
}

// static
void LanguagePersistedTabDataAndroid::From(TabAndroid* tab_android,
                                           FromCallback from_callback) {
  PersistedTabDataAndroid::From(
      tab_android->GetTabAndroidWeakPtr(),
      LanguagePersistedTabDataAndroid::UserDataKey(),
      base::BindOnce([](TabAndroid* tab_android)
                         -> std::unique_ptr<PersistedTabDataAndroid> {
        return std::make_unique<LanguagePersistedTabDataAndroid>(tab_android);
      }),
      std::move(from_callback));
}

const void* LanguagePersistedTabDataAndroid::UserDataKey() {
  return &LanguagePersistedTabDataAndroid::kUserDataKey;
}

void LanguagePersistedTabDataAndroid::RegisterTranslateDriver(
    translate::TranslateDriver* translate_driver) {
  DCHECK(translate_driver);
  if (translate_driver_ == translate_driver) {
    return;
  }
  if (translate_driver_ != nullptr) {
    translate_driver_->RemoveLanguageDetectionObserver(this);
  }

  translate_driver_ = translate_driver;

  if (translate_driver_) {
    translate_driver_->AddLanguageDetectionObserver(this);
  }
}

void LanguagePersistedTabDataAndroid::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (!tab_ || !tab_->web_contents()) {
    return;
  }
  SetLanguageDetails(details.adopted_language, details.model_reliability_score);

  content::WebContents* web_contents = tab_->web_contents();
  if (!web_contents) {
    return;
  }

  content_capture::OnscreenContentProvider* onscreen_content_provider =
      content_capture::OnscreenContentProvider::FromWebContents(web_contents);

  if (!onscreen_content_provider) {
    return;
  }

  onscreen_content_provider->DidUpdateLanguageDetails(detected_language_code_,
                                                      language_confidence_);

  DVLOG(1) << "LanguagePersistedTabDataAndroid: DidUpdateLanguageDetails "
              "directly called for: "
           << detected_language_code_
           << " (Confidence: " << language_confidence_ << ")";
}

void LanguagePersistedTabDataAndroid::OnTranslateDriverDestroyed(
    translate::TranslateDriver* driver) {
  if (translate_driver_) {
    translate_driver_->RemoveLanguageDetectionObserver(this);
  }
  translate_driver_ = nullptr;
}

std::unique_ptr<const std::vector<uint8_t>>
LanguagePersistedTabDataAndroid::Serialize() {
  language::LanguageData data;
  data.set_language_code(detected_language_code_);
  data.set_language_confidence(language_confidence_);

  auto serialized_data =
      std::make_unique<std::vector<uint8_t>>(data.ByteSizeLong());
  data.SerializeToArray(serialized_data->data(), serialized_data->size());
  return serialized_data;
}

void LanguagePersistedTabDataAndroid::Deserialize(
    const std::vector<uint8_t>& data) {
  language::LanguageData language_data;
  if (!language_data.ParseFromArray(data.data(), data.size())) {
    return;
  }

  if (language_data.has_language_code()) {
    detected_language_code_ = language_data.language_code();
  }
  if (language_data.has_language_confidence()) {
    language_confidence_ = language_data.language_confidence();
  }
}

void LanguagePersistedTabDataAndroid::SetLanguageDetails(
    const std::string& language_code,
    float confidence) {
  if (language_code.empty()) {
    return;
  }
  if (detected_language_code_ == language_code &&
      language_confidence_ == confidence) {
    return;
  }

  detected_language_code_ = language_code;
  language_confidence_ = confidence;
  Save();
}

void LanguagePersistedTabDataAndroid::ExistsForTesting(
    TabAndroid* tab_android,
    base::OnceCallback<void(bool)> exists_callback) {
  PersistedTabDataAndroid::ExistsForTesting(
      tab_android, LanguagePersistedTabDataAndroid::UserDataKey(),
      std::move(exists_callback));
}

TAB_ANDROID_USER_DATA_KEY_IMPL(LanguagePersistedTabDataAndroid)
