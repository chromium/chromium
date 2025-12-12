// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LANGUAGE_PERSISTED_TAB_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LANGUAGE_PERSISTED_TAB_DATA_ANDROID_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/browser/web_contents.h"

// Client of PersistedTabDataAndroid class for persisting language details.
class LanguagePersistedTabDataAndroid
    : public PersistedTabDataAndroid,
      public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  explicit LanguagePersistedTabDataAndroid(TabAndroid* tab_android);
  ~LanguagePersistedTabDataAndroid() override;

  // Used to acquire LanguagePersistedTabDataAndroid for a given TabAndroid
  // Integrates with PersistedTabDataAndroid::From
  static void From(TabAndroid* tab_android, FromCallback from_callback);

  // TODO (crbug.com/468347707) : Required for TabAndroidUserData, this provides
  // a unique key for the PersistedTabDataAndroid subclass. Without this
  // explicit definition, PersistedTabDataAndroid::UserDataKey() would be used,
  // which would lead to key collisions for all subclasses.
  static const void* UserDataKey();

  // Subscribes this instance as an observer to the ContentTranslateDriver.
  void RegisterTranslateDriver(translate::TranslateDriver* driver);

  // translate::TranslateDriver::LanguageDetectionObserver implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;

  const std::string& detected_language_code() const {
    return detected_language_code_;
  }
  float language_confidence() const { return language_confidence_; }

 protected:
  std::unique_ptr<const std::vector<uint8_t>> Serialize() override;
  void Deserialize(const std::vector<uint8_t>& data) override;

 private:
  friend class TabAndroidUserData<LanguagePersistedTabDataAndroid>;

  void SetLanguageDetails(const std::string& language_code, float confidence);

  std::string detected_language_code_;
  float language_confidence_ = 0.0f;
  const raw_ptr<TabAndroid> tab_;
  // Not owned, Register manually through RegisterTranslateDriver
  raw_ptr<translate::TranslateDriver> translate_driver_ = nullptr;

  // Determine if LanguagePersistedTabDataAndroid exists for |tab_android|.
  // true/false result returned in |exists_callback|.
  static void ExistsForTesting(TabAndroid* tab_android,
                               base::OnceCallback<void(bool)> exists_callback);
  TAB_ANDROID_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_LANGUAGE_PERSISTED_TAB_DATA_ANDROID_H_
