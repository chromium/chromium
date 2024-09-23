// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/trusted_cdn.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TrustedCdn_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

TrustedCdn::TrustedCdn(JNIEnv* env, const JavaParamRef<jobject>& obj)
    : jobj_(env, obj) {}

TrustedCdn::~TrustedCdn() = default;

void TrustedCdn::SetWebContents(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jobject>& jweb_contents) {
  web_contents_ = WebContents::FromJavaWebContents(jweb_contents);
}

void TrustedCdn::ResetWebContents(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  web_contents_ = nullptr;
}

void TrustedCdn::OnDestroyed(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject> TrustedCdn::GetPublisherUrl(
    JNIEnv* env) {
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    return url::GURLAndroid::EmptyGURL(env);
  }

  if (offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          web_contents_)) {
    return url::GURLAndroid::EmptyGURL(env);
  }

  return url::GURLAndroid::FromNativeGURL(
      env,
      embedder_support::GetPublisherURL(web_contents_->GetPrimaryMainFrame()));
}

static jlong JNI_TrustedCdn_Init(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new TrustedCdn(env, obj));
}
