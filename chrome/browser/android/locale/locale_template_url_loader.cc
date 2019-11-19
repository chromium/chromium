// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/locale_template_url_loader.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/android/chrome_jni_headers/LocaleTemplateUrlLoader_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

class PrefService;
class TemplateURL;

static jlong JNI_LocaleTemplateUrlLoader_Init(
    JNIEnv* env,
    const JavaParamRef<jstring>& jlocale) {
  Profile* profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
  return reinterpret_cast<intptr_t>(new LocaleTemplateUrlLoader(
      ConvertJavaStringToUTF8(env, jlocale),
      TemplateURLServiceFactory::GetForProfile(profile)));
}

LocaleTemplateUrlLoader::LocaleTemplateUrlLoader(const std::string& locale,
                                                 TemplateURLService* service)
    : locale_(locale), template_url_service_(service) {}

void LocaleTemplateUrlLoader::Destroy(JNIEnv* env) {
  delete this;
}

jboolean LocaleTemplateUrlLoader::LoadTemplateUrls(JNIEnv* env) {
  DCHECK(locale_.length() == 2);

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_list =
      GetLocalPrepopulatedEngines();

  if (prepopulated_list.empty())
    return false;

  for (const auto& data_url : prepopulated_list) {
    // Attempt to see if the URL already exists in the list of template URLs.
    //
    // Special case Google because the keyword is mutated based on the results
    // of the GoogleUrlTracker, so we need to rely on prepopulate ID instead
    // of keyword only for Google.
    //
    // Otherwise, matching based on keyword is sufficient and preferred as
    // some logically distinct search engines share the same prepopulate ID and
    // only differ on keyword.
    const TemplateURL* matching_url =
        template_url_service_->GetTemplateURLForKeyword(data_url->keyword());
    bool exists = matching_url != nullptr;
    if (!exists &&
        data_url->prepopulate_id == TemplateURLPrepopulateData::google.id) {
      auto existing_urls = template_url_service_->GetTemplateURLs();

      for (auto* existing_url : existing_urls) {
        if (existing_url->prepopulate_id() ==
            TemplateURLPrepopulateData::google.id) {
          matching_url = existing_url;
          exists = true;
          break;
        }
      }
    }

    if (exists)
      continue;

    data_url.get()->safe_for_autoreplace = true;
    std::unique_ptr<TemplateURL> turl(
        new TemplateURL(*data_url, TemplateURL::LOCAL));
    TemplateURL* added_turl = template_url_service_->Add(std::move(turl));
    if (added_turl) {
      prepopulate_ids_.push_back(added_turl->prepopulate_id());
    }
  }
  return true;
}

void LocaleTemplateUrlLoader::RemoveTemplateUrls(JNIEnv* env) {
  while (!prepopulate_ids_.empty()) {
    TemplateURL* turl = FindURLByPrepopulateID(
        template_url_service_->GetTemplateURLs(), prepopulate_ids_.back());
    if (turl && template_url_service_->GetDefaultSearchProvider() != turl) {
      template_url_service_->Remove(turl);
    }
    prepopulate_ids_.pop_back();
  }
}

void LocaleTemplateUrlLoader::OverrideDefaultSearchProvider(JNIEnv* env) {
  // If the user has changed their default search provider, no-op.
  const TemplateURL* current_dsp =
      template_url_service_->GetDefaultSearchProvider();
  if (!current_dsp ||
      current_dsp->prepopulate_id() != TemplateURLPrepopulateData::google.id) {
    return;
  }

  TemplateURL* turl =
      FindURLByPrepopulateID(template_url_service_->GetTemplateURLs(),
                             GetDesignatedSearchEngineForChina());
  if (turl) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(turl);
  }
}

void LocaleTemplateUrlLoader::SetGoogleAsDefaultSearch(JNIEnv* env) {
  // If the user has changed their default search provider, no-op.
  const TemplateURL* current_dsp =
      template_url_service_->GetDefaultSearchProvider();
  if (!current_dsp ||
      current_dsp->prepopulate_id() != GetDesignatedSearchEngineForChina()) {
    return;
  }

  TemplateURL* turl =
      FindURLByPrepopulateID(template_url_service_->GetTemplateURLs(),
                             TemplateURLPrepopulateData::google.id);
  if (turl) {
    template_url_service_->SetUserSelectedDefaultSearchProvider(turl);
  }
}

std::vector<std::unique_ptr<TemplateURLData>>
LocaleTemplateUrlLoader::GetLocalPrepopulatedEngines() {
  return TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(locale_);
}

int LocaleTemplateUrlLoader::GetDesignatedSearchEngineForChina() {
  return TemplateURLPrepopulateData::sogou.id;
}

LocaleTemplateUrlLoader::~LocaleTemplateUrlLoader() {}
