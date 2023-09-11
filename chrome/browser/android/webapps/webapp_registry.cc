// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapp_registry.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/android/chrome_jni_headers/WebappRegistry_jni.h"
#include "chrome/browser/android/browsing_data/url_filter_bridge.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

void WebappRegistry::UnregisterWebappsForUrls(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter) {
  // |filter_bridge| is destroyed from its Java counterpart.
  UrlFilterBridge* filter_bridge = new UrlFilterBridge(url_filter);

  Java_WebappRegistry_unregisterWebappsForUrls(
      base::android::AttachCurrentThread(), filter_bridge->j_bridge());
}

void WebappRegistry::ClearWebappHistoryForUrls(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter) {
  // |filter_bridge| is destroyed from its Java counterpart.
  UrlFilterBridge* filter_bridge = new UrlFilterBridge(url_filter);

  Java_WebappRegistry_clearWebappHistoryForUrls(
      base::android::AttachCurrentThread(), filter_bridge->j_bridge());
}

std::vector<std::string> WebappRegistry::GetOriginsWithWebApk() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> java_result =
      Java_WebappRegistry_getOriginsWithWebApkAsArray(env);

  std::vector<std::string> origins;
  base::android::AppendJavaStringArrayToStringVector(env, java_result,
                                                     &origins);
  return origins;
}

std::vector<std::string> WebappRegistry::GetOriginsWithInstalledApp() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> java_result =
      Java_WebappRegistry_getOriginsWithInstalledAppAsArray(env);

  std::vector<std::string> origins;
  base::android::AppendJavaStringArrayToStringVector(env, java_result,
                                                     &origins);
  return origins;
}
