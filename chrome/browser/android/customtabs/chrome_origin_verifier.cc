// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/chrome_origin_verifier.h"

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "components/content_relationship_verification/origin_verifier.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/browserservices/verification/jni_headers/ChromeOriginVerifier_jni.h"

namespace customtabs {

// static variables are zero-initialized.
int ChromeOriginVerifier::clear_browsing_data_call_count_for_tests_;

// static
void ChromeOriginVerifier::ClearBrowsingData() {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ChromeOriginVerifier_clearBrowsingData(env);
  clear_browsing_data_call_count_for_tests_++;
}

// static
int ChromeOriginVerifier::GetClearBrowsingDataCallCountForTesting() {
  return ChromeOriginVerifier::clear_browsing_data_call_count_for_tests_;
}

static jlong JNI_ChromeOriginVerifier_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  if (!g_browser_process)
    return 0;

  return OriginVerifier::Init(env, obj, jbrowser_context_handle);
}

}  // namespace customtabs
