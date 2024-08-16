// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_RESUMPTION_SEARCH_RESUMPTION_MODULE_BRIDGE_H_
#define CHROME_BROWSER_SEARCH_RESUMPTION_SEARCH_RESUMPTION_MODULE_BRIDGE_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/search/start_suggest_service.h"

class Profile;

namespace search_resumption_module {
// Bridge between the C++ and the Java for fetching suggestions in the search
// resumption module on NTP.
class SearchResumptionModuleBridge {
 public:
  SearchResumptionModuleBridge(JNIEnv* env,
                               const jni_zero::JavaRef<jobject>& obj,
                               Profile* profile);
  SearchResumptionModuleBridge(const SearchResumptionModuleBridge&) = delete;
  SearchResumptionModuleBridge& operator=(const SearchResumptionModuleBridge&) =
      delete;

  void Destroy(JNIEnv* env, const jni_zero::JavaParamRef<jobject>& obj);

  void FetchSuggestions(JNIEnv* env,
                        const jni_zero::JavaParamRef<jobject>& obj,
                        const jni_zero::JavaParamRef<jstring>& j_page_url);

 private:
  ~SearchResumptionModuleBridge();
  void OnSuggestionsReceived(std::vector<QuerySuggestion> suggestions);

  raw_ptr<StartSuggestService> start_suggest_service_;
  jni_zero::ScopedJavaGlobalRef<jobject> java_object_;

  const base::WeakPtrFactory<SearchResumptionModuleBridge> weak_ptr_factory_{
      this};
};

}  // namespace search_resumption_module
#endif  // CHROME_BROWSER_SEARCH_RESUMPTION_SEARCH_RESUMPTION_MODULE_BRIDGE_H_
