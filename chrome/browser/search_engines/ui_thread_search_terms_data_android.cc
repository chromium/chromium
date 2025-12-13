// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/ui_thread_search_terms_data_android.h"

#include "base/no_destructor.h"
#include "chrome/browser/android/locale/locale_manager.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "content/public/browser/browser_thread.h"

// static
std::u16string& SearchTermsDataAndroid::GetRlzParameterValue() {
  static base::NoDestructor<std::u16string> rlz_parameter_value;
  return *rlz_parameter_value;
}

// static
std::string& SearchTermsDataAndroid::GetSearchClient() {
  static base::NoDestructor<std::string> search_client;
  return *search_client;
}

// static
std::optional<std::string>& SearchTermsDataAndroid::GetCustomTabSearchClient() {
  static base::NoDestructor<std::optional<std::string>>
      custom_tab_search_client;
  return *custom_tab_search_client;
}

std::u16string UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Android doesn't use the rlz library.  Instead, it manages the rlz string
  // on its own.
  return SearchTermsDataAndroid::GetRlzParameterValue();
}

std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return SearchTermsDataAndroid::GetCustomTabSearchClient().value_or(
      SearchTermsDataAndroid::GetSearchClient());
}

std::string UIThreadSearchTermsData::GetYandexReferralID() const {
  return LocaleManager::GetYandexReferralID();
}

std::string UIThreadSearchTermsData::GetMailRUReferralID() const {
  return LocaleManager::GetMailRUReferralID();
}
