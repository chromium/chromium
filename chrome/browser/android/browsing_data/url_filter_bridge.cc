// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/url_filter_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/UrlFilterBridge_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

UrlFilterBridge::UrlFilterBridge(
    const base::Callback<bool(const GURL&)>& url_filter)
    : url_filter_(url_filter),
      j_bridge_(Java_UrlFilterBridge_create(
          base::android::AttachCurrentThread(),
          reinterpret_cast<uintptr_t>(this))) {}

UrlFilterBridge::~UrlFilterBridge() {}

void UrlFilterBridge::Destroy(JNIEnv* env,
                              const JavaParamRef<jobject>& obj) {
  delete this;
}

bool UrlFilterBridge::MatchesUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl) const {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  return url_filter_.Run(url);
}
