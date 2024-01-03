// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_NATIVE_CONTEXTUAL_SEARCH_CONTEXT_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_NATIVE_CONTEXTUAL_SEARCH_CONTEXT_H_

#include "base/android/jni_android.h"
#include "components/contextual_search/core/browser/contextual_search_context.h"
#include "url/gurl.h"

// A ContextualSearchContext subclass that is modifiable via JNI. This is the
// native implementation of the Java ContextualSearchContext; Instance lifetimes
// are managed by the associated Java object.
class NativeContextualSearchContext final : public ContextualSearchContext {
 public:
  NativeContextualSearchContext(JNIEnv* env, jobject obj);

  NativeContextualSearchContext(const NativeContextualSearchContext&) = delete;
  NativeContextualSearchContext& operator=(
      const NativeContextualSearchContext&) = delete;

  ~NativeContextualSearchContext() override;

  // ContextualSearchContext
  base::WeakPtr<ContextualSearchContext> AsWeakPtr() override;

  // Calls the destructor.  Should be called when this native object is no
  // longer needed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Returns the NativeContextualSearchContext given the Java object.
  static base::WeakPtr<NativeContextualSearchContext>
  FromJavaContextualSearchContext(
      const base::android::JavaRef<jobject>& j_contextual_search_context);

  // Sets the properties needed to resolve a context.
  void SetResolveProperties(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jstring>& j_home_country,
      jboolean j_may_send_base_page_url);

  // Adjust the current selection offsets by the given signed amounts.
  void AdjustSelection(JNIEnv* env,
                       jobject obj,
                       jint j_start_adjust,
                       jint j_end_adjust);

  // Prepares the context to be used in a resolve request by supplying last
  // minute parameters.
  // |j_is_exact_resolve| indicates if the resolved term should be an exact
  // match for the selection range instead of an expandable selection.
  // |j_related_searches_stamp| is a value to stamp onto search URLs to
  // identify related searches. If the string is empty then Related Searches
  // are not being requested.
  void PrepareToResolve(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean j_is_exact_resolve,
      const base::android::JavaParamRef<jstring>& j_related_searches_stamp);

  // Detects the language of the context using CLD from the translate utility.
  base::android::ScopedJavaLocalRef<jstring> DetectLanguage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  // Sets the languages to remember for use in translation.
  // See |GetTranslationLanguages|.
  void SetTranslationLanguages(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_detected_language,
      const base::android::JavaParamRef<jstring>& j_target_language,
      const base::android::JavaParamRef<jstring>& j_fluent_languages);

 private:
  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  base::WeakPtrFactory<NativeContextualSearchContext> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_NATIVE_CONTEXTUAL_SEARCH_CONTEXT_H_
