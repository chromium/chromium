// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_SENSITIVITY_PERSISTED_TAB_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_SENSITIVITY_PERSISTED_TAB_DATA_ANDROID_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"

// Client of PersistedTabDataAndroid
class SensitivityPersistedTabDataAndroid
    : public PersistedTabDataAndroid,
      public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit SensitivityPersistedTabDataAndroid(TabAndroid* tab_android);

  void RegisterPCAService(
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service);
  ~SensitivityPersistedTabDataAndroid() override;

  // Used to acquire SensitivityPersistedTabDataAndroid for a given TabAndroid
  // Integrates with PersistedTabDataAndroid::From
  static void From(TabAndroid* tab_android, FromCallback from_callback);

  void set_is_sensitive(bool is_sensitive) {
    is_sensitive_ = is_sensitive;
    Save();
  }

  bool is_sensitive() { return is_sensitive_; }

  // page_content_annotations::PageContentAnnotationsService::PageContentAnnotationsObserver
  void OnPageContentAnnotated(
      const GURL& url,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

 protected:
  std::unique_ptr<const std::vector<uint8_t>> Serialize() override;
  void Deserialize(const std::vector<uint8_t>& data) override;

 private:
  friend class TabAndroidUserData<SensitivityPersistedTabDataAndroid>;
  friend class SensitivityPersistedTabDataAndroidBrowserTest;
  // TODO(crbug.com/40273829) Consider making is_sensitive_ absl::option<bool>
  bool is_sensitive_ = false;
  raw_ptr<TabAndroid> tab_;

  // Not owned. Register manually through RegisterPCAService
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_ = nullptr;

  // Determine if SensitivityPersistedTabDataAndroid exists for |tab_android|.
  // true/false result returned in |exists_callback|.
  static void ExistsForTesting(TabAndroid* tab_android,
                               base::OnceCallback<void(bool)> exists_callback);

  TAB_ANDROID_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_SENSITIVITY_PERSISTED_TAB_DATA_ANDROID_H_
