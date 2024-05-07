// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/ui_thread_search_terms_data_android.h"

#include "base/lazy_instance.h"
#include "chrome/browser/android/locale/locale_manager.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "content/public/browser/browser_thread.h"

base::LazyInstance<std::u16string>::Leaky
    SearchTermsDataAndroid::rlz_parameter_value_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::string>::Leaky
    SearchTermsDataAndroid::search_client_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::optional<std::string>>::Leaky
    SearchTermsDataAndroid::custom_tab_search_client_ =
        LAZY_INSTANCE_INITIALIZER;

std::u16string UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Android doesn't use the rlz library.  Instead, it manages the rlz string
  // on its own.
  return SearchTermsDataAndroid::rlz_parameter_value_.Get();
}

std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return SearchTermsDataAndroid::custom_tab_search_client_.Get().value_or(
      SearchTermsDataAndroid::search_client_.Get());
}

std::string UIThreadSearchTermsData::GetYandexReferralID() const {
  return LocaleManager::GetYandexReferralID();
}

std::string UIThreadSearchTermsData::GetMailRUReferralID() const {
  return LocaleManager::GetMailRUReferralID();
}
