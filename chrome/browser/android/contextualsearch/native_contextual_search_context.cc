// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/native_contextual_search_context.h"

#include <string>

#include "base/android/jni_string.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextualSearchContext_jni.h"

NativeContextualSearchContext::NativeContextualSearchContext(JNIEnv* env,
                                                             jobject obj) {
  java_object_.Reset(env, obj);
}

NativeContextualSearchContext::~NativeContextualSearchContext() = default;

base::WeakPtr<ContextualSearchContext>
NativeContextualSearchContext::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
base::WeakPtr<NativeContextualSearchContext>
NativeContextualSearchContext::FromJavaContextualSearchContext(
    const base::android::JavaRef<jobject>& j_contextual_search_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (j_contextual_search_context.is_null())
    return nullptr;

  NativeContextualSearchContext* contextual_search_context =
      reinterpret_cast<NativeContextualSearchContext*>(
          Java_ContextualSearchContext_getNativePointer(
              base::android::AttachCurrentThread(),
              j_contextual_search_context));
  return contextual_search_context->weak_ptr_factory_.GetWeakPtr();
}

void NativeContextualSearchContext::SetResolveProperties(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_home_country,
    jboolean j_may_send_base_page_url) {
  std::string home_country =
      base::android::ConvertJavaStringToUTF8(env, j_home_country);
  ContextualSearchContext::SetResolveProperties(home_country,
                                                j_may_send_base_page_url);
}

void NativeContextualSearchContext::AdjustSelection(JNIEnv* env,
                                                    jobject obj,
                                                    jint j_start_adjust,
                                                    jint j_end_adjust) {
  ContextualSearchContext::AdjustSelection(j_start_adjust, j_end_adjust);
}

void NativeContextualSearchContext::PrepareToResolve(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean j_is_exact_resolve,
    const base::android::JavaParamRef<jstring>& j_related_searches_stamp) {
  std::string related_searches_stamp =
      base::android::ConvertJavaStringToUTF8(env, j_related_searches_stamp);
  ContextualSearchContext::PrepareToResolve(j_is_exact_resolve,
                                            related_searches_stamp);
}

base::android::ScopedJavaLocalRef<jstring>
NativeContextualSearchContext::DetectLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) const {
  std::string language = ContextualSearchContext::DetectLanguage();
  base::android::ScopedJavaLocalRef<jstring> j_language =
      base::android::ConvertUTF8ToJavaString(env, language);
  return j_language;
}

void NativeContextualSearchContext::SetTranslationLanguages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_detected_language,
    const base::android::JavaParamRef<jstring>& j_target_language,
    const base::android::JavaParamRef<jstring>& j_fluent_languages) {
  std::string detected_language =
      base::android::ConvertJavaStringToUTF8(env, j_detected_language);
  std::string target_language =
      base::android::ConvertJavaStringToUTF8(env, j_target_language);
  std::string fluent_languages =
      base::android::ConvertJavaStringToUTF8(env, j_fluent_languages);
  ContextualSearchContext::SetTranslationLanguages(
      detected_language, target_language, fluent_languages);
}

// Java wrapper boilerplate

void NativeContextualSearchContext::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_ContextualSearchContext_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  NativeContextualSearchContext* context =
      new NativeContextualSearchContext(env, obj);
  return reinterpret_cast<intptr_t>(context);
}
