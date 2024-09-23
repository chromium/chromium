// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/locale/locale_template_url_loader.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/check_deref.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/locale/jni_headers/LocaleTemplateUrlLoader_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

static jlong JNI_LocaleTemplateUrlLoader_Init(
    JNIEnv* env,
    const JavaParamRef<jstring>& jlocale,
    Profile* profile) {
  return reinterpret_cast<intptr_t>(new LocaleTemplateUrlLoader(
      ConvertJavaStringToUTF8(env, jlocale),
      TemplateURLServiceFactory::GetForProfile(profile), profile));
}

LocaleTemplateUrlLoader::LocaleTemplateUrlLoader(const std::string& locale,
                                                 TemplateURLService* service,
                                                 Profile* profile)
    : locale_(locale), template_url_service_(service) {
  profile_observation_.Observe(profile);
}

void LocaleTemplateUrlLoader::Destroy(JNIEnv* env) {
  delete this;
}

void LocaleTemplateUrlLoader::OnProfileWillBeDestroyed(Profile* profile) {
  // There is a risk that java keeps a reference to this loader and attempts to
  // use it even if we started destroying the profile on the native side. To
  // protect against this we remove access to the `template_url_service_` and
  // stub out subsequent the calls.
  profile_observation_.Reset();
  template_url_service_ = nullptr;
}

jboolean LocaleTemplateUrlLoader::LoadTemplateUrls(JNIEnv* env) {
  DCHECK(locale_.length() == 2);

  if (!template_url_service_) {
    // TODO(b/318339172): Test profile state from Java, switch to CHECK here.
    base::debug::DumpWithoutCrashing();  // Investigating b/317335096.
    return false;
  }

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_list =
      GetLocalPrepopulatedEngines();

  if (prepopulated_list.empty()) {
    return false;
  }

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

      for (TemplateURL* existing_url : existing_urls) {
        if (existing_url->prepopulate_id() ==
            TemplateURLPrepopulateData::google.id) {
          matching_url = existing_url;
          exists = true;
          break;
        }
      }
    }

    if (exists) {
      continue;
    }

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
  if (!template_url_service_) {
    // TODO(b/318339172): Test profile state from Java, switch to CHECK here.
    base::debug::DumpWithoutCrashing();  // Investigating b/317335096.
    return;
  }

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
  if (!template_url_service_) {
    // TODO(b/318339172): Test profile state from Java, switch to CHECK here.
    base::debug::DumpWithoutCrashing();  // Investigating b/317335096.
    return;
  }

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
  if (!template_url_service_) {
    // TODO(b/318339172): Test profile state from Java, switch to CHECK here.
    base::debug::DumpWithoutCrashing();  // Investigating b/317335096.
    return;
  }

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
  if (!template_url_service_) {
    // TODO(b/318339172): Test profile state from Java, switch to CHECK here.
    base::debug::DumpWithoutCrashing();  // Investigating b/317335096.
    return std::vector<std::unique_ptr<TemplateURLData>>();
  }

  return template_url_service_->GetTemplateURLsForCountry(locale_);
}

int LocaleTemplateUrlLoader::GetDesignatedSearchEngineForChina() {
  return TemplateURLPrepopulateData::sogou.id;
}

LocaleTemplateUrlLoader::~LocaleTemplateUrlLoader() {}
