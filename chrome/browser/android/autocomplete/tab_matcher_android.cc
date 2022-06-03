// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autocomplete/tab_matcher_android.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/web_contents_user_data.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_android_user_data.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

namespace {
class AutocompleteClientTabAndroidUserData
    : public TabAndroidUserData<AutocompleteClientTabAndroidUserData>,
      public TabAndroid::Observer {
 public:
  ~AutocompleteClientTabAndroidUserData() override {
    tab_->RemoveObserver(this);
  }

  const GURL& GetStrippedURL() const { return stripped_url_; }

  bool IsInitialized() const { return initialized_; }

  void UpdateStrippedURL(const GURL& url,
                         const TemplateURLService* template_url_service) {
    initialized_ = true;
    if (url.is_valid()) {
      stripped_url_ = AutocompleteMatch::GURLToStrippedGURL(
          url, AutocompleteInput(), template_url_service, std::u16string());
    }
  }

  // TabAndroid::Observer implementation
  void OnInitWebContents(TabAndroid* tab) override {
    tab->RemoveUserData(UserDataKey());
  }

 private:
  explicit AutocompleteClientTabAndroidUserData(TabAndroid* tab) : tab_(tab) {
    DCHECK(tab);
    tab->AddObserver(this);
  }
  friend class TabAndroidUserData<AutocompleteClientTabAndroidUserData>;

  raw_ptr<TabAndroid> tab_;
  bool initialized_ = false;
  GURL stripped_url_;

  TAB_ANDROID_USER_DATA_KEY_DECL();
};
TAB_ANDROID_USER_DATA_KEY_IMPL(AutocompleteClientTabAndroidUserData)
}  // namespace

bool TabMatcherAndroid::IsTabOpenWithURL(const GURL& url,
                                         const AutocompleteInput* input) const {
  return false;
}

void TabMatcherAndroid::FindMatchingTabs(GURLToTabInfoMap* map,
                                         const AutocompleteInput* input) const {
}
