// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_PREFERENCE_HELPER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_PREFERENCE_HELPER_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/core/browser/contextual_search_preference.h"

// Native portion of a class that helps coordinate user preferences for
// Contextual Search with Unified Consent.
// TODO(donnd): Check if this is still needed based on changes to Unified
// Consent.
class ContextualSearchPreferenceHelper {
 public:
  ContextualSearchPreferenceHelper(JNIEnv* env, jobject obj);
  ~ContextualSearchPreferenceHelper();

  // Gets the previously stored metadata about a possible previous preference
  // change made by Unified Consent.
  jint GetPreferenceMetadata(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  // Calls the destructor.  Should be called when this native object is no
  // longer needed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchPreferenceHelper);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_PREFERENCE_HELPER_H_
