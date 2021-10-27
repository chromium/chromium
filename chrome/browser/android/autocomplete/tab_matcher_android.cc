// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autocomplete/tab_matcher_android.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/web_contents_user_data.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_android_user_data.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/omnibox/jni_headers/ChromeAutocompleteProviderClient_jni.h"
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

  TabAndroid* tab_;
  bool initialized_ = false;
  GURL stripped_url_;

  TAB_ANDROID_USER_DATA_KEY_DECL();
};
TAB_ANDROID_USER_DATA_KEY_IMPL(AutocompleteClientTabAndroidUserData)
}  // namespace

bool TabMatcherAndroid::IsTabOpenWithURL(const GURL& url,
                                         const AutocompleteInput* input) const {
  return GetTabOpenWithURL(url, input) != nullptr;
}

TabAndroid* TabMatcherAndroid::GetTabOpenWithURL(
    const GURL& url,
    const AutocompleteInput* input) const {
  const AutocompleteInput empty_input;
  if (!input)
    input = &empty_input;
  const GURL stripped_url = AutocompleteMatch::GURLToStrippedGURL(
      url, *input, client_.GetTemplateURLService(), std::u16string());

  std::vector<TabModel*> tab_models;
  for (TabModel* model : TabModelList::models()) {
    if (profile_ != model->GetProfile())
      continue;

    tab_models.push_back(model);
  }

  std::vector<TabAndroid*> all_tabs = GetAllHiddenAndNonCCTTabs(tab_models);

  for (TabAndroid* tab : all_tabs) {
    // Browser did not load the tab yet after Chrome started. To avoid
    // reloading WebContents, we just compare URLs.
    AutocompleteClientTabAndroidUserData::CreateForTabAndroid(tab);
    AutocompleteClientTabAndroidUserData* user_data =
        AutocompleteClientTabAndroidUserData::FromTabAndroid(tab);
    DCHECK(user_data);
    if (!user_data->IsInitialized()) {
      user_data->UpdateStrippedURL(tab->GetURL(),
                                   client_.GetTemplateURLService());
    }

    const GURL& tab_stripped_url = user_data->GetStrippedURL();
    if (tab_stripped_url == stripped_url)
      return tab;
  }

  return nullptr;
}

std::vector<TabAndroid*> TabMatcherAndroid::GetAllHiddenAndNonCCTTabs(
    const std::vector<TabModel*>& tab_models) const {
  using chrome::android::ActivityType;

  if (tab_models.size() == 0)
    return std::vector<TabAndroid*>();

  JNIEnv* env = base::android::AttachCurrentThread();
  jclass tab_model_clazz = TabModelJniBridge::GetClazz(env);
  base::android::ScopedJavaLocalRef<jobjectArray> j_tab_model_array(
      env, env->NewObjectArray(tab_models.size(), tab_model_clazz, nullptr));
  // Get all the hidden and non CCT tabs. Filter the tabs in CCT tabmodel first.
  for (size_t i = 0; i < tab_models.size(); ++i) {
    ActivityType type = tab_models[i]->activity_type();
    if (type == ActivityType::kCustomTab ||
        type == ActivityType::kTrustedWebActivity) {
      continue;
    }
    env->SetObjectArrayElement(j_tab_model_array.obj(), i,
                               tab_models[i]->GetJavaObject().obj());
  }

  base::android::ScopedJavaLocalRef<jobjectArray> j_tabs =
      Java_ChromeAutocompleteProviderClient_getAllHiddenTabs(env,
                                                             j_tab_model_array);
  if (j_tabs.is_null())
    return std::vector<TabAndroid*>();

  return TabAndroid::GetAllNativeTabs(env, j_tabs);
}
