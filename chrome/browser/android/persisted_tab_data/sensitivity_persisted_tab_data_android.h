// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_SENSITIVITY_PERSISTED_TAB_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_SENSITIVITY_PERSISTED_TAB_DATA_ANDROID_H_

#include <vector>

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

// Client of PersistedTabDataAndroid
class SensitivityPersistedTabDataAndroid
    : public PersistedTabDataAndroid,
      public optimization_guide::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit SensitivityPersistedTabDataAndroid(TabAndroid* tab_android);
  ~SensitivityPersistedTabDataAndroid() override;

  // Used to acquire SensitivityPersistedTabDataAndroid for a given TabAndroid
  // Integrates with PersistedTabDataAndroid::From
  static void From(TabAndroid* tab_android, FromCallback from_callback) {
    PersistedTabDataAndroid::From(
        tab_android, SensitivityPersistedTabDataAndroid::UserDataKey(),
        base::BindOnce(
            [](TabAndroid* tab_android)
                -> std::unique_ptr<PersistedTabDataAndroid> {
              if (tab_android->GetUserData(UserDataKey())) {
                return base::WrapUnique(static_cast<PersistedTabDataAndroid*>(
                    tab_android->GetUserData(UserDataKey())));
              }
              std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda =
                  std::make_unique<SensitivityPersistedTabDataAndroid>(
                      tab_android);
              tab_android->SetUserData(UserDataKey(), std::move(sptda));
              return sptda;
            },
            tab_android),
        std::move(from_callback),
        base::BindOnce(
            [](TabAndroid* tab_android, const std::vector<uint8_t>& data)
                -> std::unique_ptr<PersistedTabDataAndroid> {
              std::unique_ptr<SensitivityPersistedTabDataAndroid> sptda =
                  std::make_unique<SensitivityPersistedTabDataAndroid>(
                      tab_android);
              // TODO(crbug.com/1458486) move to background thread as an
              // optimization.
              sptda->Deserialize(data);
              return sptda;
            },
            tab_android));
  }

  std::unique_ptr<const std::vector<uint8_t>> Serialize() override;
  void Deserialize(const std::vector<uint8_t>& data) override;

  void set_is_sensitive(bool is_sensitive) {
    is_sensitive_ = is_sensitive;
    Save();
  }

  bool is_sensitive() { return is_sensitive_; }

  // optimization_guide::PageContentAnnotationsService::PageContentAnnotationsObserver
  void OnPageContentAnnotated(
      const GURL& url,
      const optimization_guide::PageContentAnnotationsResult& result) override;

 private:
  friend class TabAndroidUserData<SensitivityPersistedTabDataAndroid>;
  friend class SensitivityPersistedTabDataAndroidBrowserTest;
  // TODO(crbug.com/1457995) Consider making is_sensitive_ absl::option<bool>
  bool is_sensitive_;
  raw_ptr<TabAndroid> tab_;

  TAB_ANDROID_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_SENSITIVITY_PERSISTED_TAB_DATA_ANDROID_H_
