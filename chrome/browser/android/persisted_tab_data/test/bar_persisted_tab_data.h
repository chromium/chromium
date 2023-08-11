// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_TEST_BAR_PERSISTED_TAB_DATA_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_TEST_BAR_PERSISTED_TAB_DATA_H_

#include <vector>

#include "chrome/browser/android/persisted_tab_data/persisted_tab_data_android.h"

// Test Class for testing PersistedTabDataAndroid
class BarPersistedTabDataAndroid : public PersistedTabDataAndroid {
 public:
  explicit BarPersistedTabDataAndroid(TabAndroid* tab_android);
  ~BarPersistedTabDataAndroid() override;

  static void From(TabAndroid* tab_android, FromCallback from_callback);
  void SetValue(bool bar_value);
  static void ExistsForTesting(TabAndroid* tab_android,
                               base::OnceCallback<void(bool)> exists_callback);

 protected:
  std::unique_ptr<const std::vector<uint8_t>> Serialize() override;
  void Deserialize(const std::vector<uint8_t>& data) override;

 private:
  friend class TabAndroidUserData<BarPersistedTabDataAndroid>;
  bool bar_value_;

  TAB_ANDROID_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_TEST_BAR_PERSISTED_TAB_DATA_H_
