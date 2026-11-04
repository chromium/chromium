// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/native_contextual_search_context.h"

#include <string>

#include "base/android/jni_string.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextualSearchContext_jni.h"

NativeContextualSearchContext::NativeContextualSearchContext(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  java_object_.Reset(env, obj);

  bool use_snippet_as_subtitle =
      base::FeatureList::IsEnabled(chrome::android::kTouchToSearchCallout) &&
      chrome::android::kTouchToSearchCalloutSnippetAsSubtitle.Get();

  ContextualSearchContext::SetUseSnippetAsSubtitle(use_snippet_as_subtitle);
}

NativeContextualSearchContext::~NativeContextualSearchContext() = default;

base::WeakPtr<ContextualSearchContext>
NativeContextualSearchContext::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void NativeContextualSearchContext::SetResolveProperties(
    JNIEnv* env,
    const std::string& home_country,
    bool j_may_send_base_page_url) {
  ContextualSearchContext::SetResolveProperties(home_country,
                                                j_may_send_base_page_url);
}

void NativeContextualSearchContext::AdjustSelection(JNIEnv* env,
                                                    int32_t j_start_adjust,
                                                    int32_t j_end_adjust) {
  ContextualSearchContext::AdjustSelection(j_start_adjust, j_end_adjust);
}

void NativeContextualSearchContext::PrepareToResolve(
    JNIEnv* env,
    bool j_is_exact_resolve,
    const std::string& related_searches_stamp) {
  ContextualSearchContext::PrepareToResolve(j_is_exact_resolve,
                                            related_searches_stamp);
}

std::string NativeContextualSearchContext::DetectLanguage(JNIEnv* env) const {
  std::string language = ContextualSearchContext::DetectLanguage();
  return language;
}

void NativeContextualSearchContext::SetTranslationLanguages(
    JNIEnv* env,
    const std::string& detected_language,
    const std::string& target_language,
    const std::string& fluent_languages) {
  ContextualSearchContext::SetTranslationLanguages(
      detected_language, target_language, fluent_languages);
}

// Java wrapper boilerplate

static jni_zero::JniUniquePtr<NativeContextualSearchContext>
JNI_ContextualSearchContext_Init(JNIEnv* env,
                                 const base::android::JavaRef<jobject>& obj) {
  return jni_zero::MakeUnique<NativeContextualSearchContext>(env, obj);
}

DEFINE_JNI(ContextualSearchContext)
