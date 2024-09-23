// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapp_registry.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "chrome/browser/android/browsing_data/url_filter_bridge.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebappRegistry_jni.h"

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

std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>
WebappRegistry::GetWebApkSpecifics() const {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobjectArray> java_result =
      Java_WebappRegistry_getWebApkSpecifics(env);

  std::vector<std::string> webapk_specifics_bytes;
  base::android::JavaArrayOfByteArrayToStringVector(env, java_result,
                                                    &webapk_specifics_bytes);

  std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> webapk_specifics;
  for (const auto& specifics_bytes : webapk_specifics_bytes) {
    std::unique_ptr<sync_pb::WebApkSpecifics> specifics =
        std::make_unique<sync_pb::WebApkSpecifics>();
    if (!specifics->ParseFromString(specifics_bytes)) {
      LOG(ERROR) << "failed to parse WebApkSpecifics proto";
      continue;
    }

    webapk_specifics.push_back(std::move(specifics));
  }

  return webapk_specifics;
}

// static
void WebappRegistry::SetNeedsPwaRestore(bool needs) {
  Java_WebappRegistry_setNeedsPwaRestore(base::android::AttachCurrentThread(),
                                         needs);
}

// static
bool WebappRegistry::GetNeedsPwaRestore() {
  return Java_WebappRegistry_getNeedsPwaRestore(
      base::android::AttachCurrentThread());
}
