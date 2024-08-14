// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "search_resumption_module_bridge.h"

#include "base/android/jni_array.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_resumption/start_suggest_service_factory.h"
#include "components/search/start_suggest_service.h"
#include "components/search_engines/search_terms_data.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/search_resumption/jni_headers/SearchResumptionModuleBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using RequestSource = SearchTermsData::RequestSource;

namespace search_resumption_module {
SearchResumptionModuleBridge::SearchResumptionModuleBridge(
    JNIEnv* env,
    const JavaRef<jobject>& jobj,
    Profile* profile)
    : java_object_(env, jobj) {
  CHECK(!profile->IsOffTheRecord());
  start_suggest_service_ =
      StartSuggestServiceFactory::GetInstance()->GetForBrowserContext(profile);
}

void SearchResumptionModuleBridge::Destroy(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  delete this;
}

void SearchResumptionModuleBridge::FetchSuggestions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_page_url) {
  if (start_suggest_service_ == nullptr) {
    return;
  }

  TemplateURLRef::SearchTermsArgs args;
  args.request_source = RequestSource::NTP_MODULE;
  args.current_page_url = ConvertJavaStringToUTF8(env, j_page_url);
  start_suggest_service_->FetchSuggestions(
      args,
      base::BindOnce(&SearchResumptionModuleBridge::OnSuggestionsReceived,
                     weak_ptr_factory_.GetMutableWeakPtr()),
      true /* fetch_from_server */);
}

SearchResumptionModuleBridge::~SearchResumptionModuleBridge() = default;

void SearchResumptionModuleBridge::OnSuggestionsReceived(
    std::vector<QuerySuggestion> suggestions) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  std::vector<const std::u16string*> titles;
  titles.reserve(suggestions.size());
  std::vector<const GURL*> urls;
  urls.reserve(suggestions.size());
  for (const auto& suggestion : suggestions) {
    titles.push_back(&suggestion.query);
    urls.push_back(&suggestion.destination_url);
  }
  Java_SearchResumptionModuleBridge_onSuggestionsReceived(env, java_object_,
                                                          titles, urls);
}

static jlong JNI_SearchResumptionModuleBridge_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    Profile* profile) {
  SearchResumptionModuleBridge* native_bridge =
      new SearchResumptionModuleBridge(env, obj, profile);
  return reinterpret_cast<intptr_t>(native_bridge);
}

}  // namespace search_resumption_module
