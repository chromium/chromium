// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/url_filter_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/UrlFilterBridge_jni.h"

using base::android::JavaParamRef;

UrlFilterBridge::UrlFilterBridge(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter)
    : url_filter_(url_filter),
      j_bridge_(
          Java_UrlFilterBridge_create(base::android::AttachCurrentThread(),
                                      reinterpret_cast<uintptr_t>(this))) {}

UrlFilterBridge::~UrlFilterBridge() {}

void UrlFilterBridge::Destroy(JNIEnv* env,
                              const JavaParamRef<jobject>& obj) {
  delete this;
}

bool UrlFilterBridge::MatchesUrl(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 std::string& url_spec) const {
  GURL url(url_spec);
  return url_filter_.Run(url);
}
