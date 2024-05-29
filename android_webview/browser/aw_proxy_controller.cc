// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/current_thread.h"
#include "content/public/browser/browser_thread.h"
#include "net/proxy_resolution/proxy_config_service_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwProxyController_jni.h"

using base::android::AttachCurrentThread;
using base::android::HasException;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void ProxyOverrideChanged(const JavaRef<jobject>& obj,
                          const JavaRef<jobject>& listener,
                          const JavaRef<jobject>& executor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!listener)
    return;
  JNIEnv* env = AttachCurrentThread();
  Java_AwProxyController_proxyOverrideChanged(env, obj, listener, executor);
  if (HasException(env)) {
    // Tell the chromium message loop to not perform any tasks after the current
    // one - we want to make sure we return to Java cleanly without first making
    // any new JNI calls.
    base::CurrentUIThread::Get()->Abort();
  }
}

}  // namespace

ScopedJavaLocalRef<jstring> JNI_AwProxyController_SetProxyOverride(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobjectArray>& jurl_schemes,
    const base::android::JavaParamRef<jobjectArray>& jproxy_urls,
    const base::android::JavaParamRef<jobjectArray>& jbypass_rules,
    const JavaParamRef<jobject>& listener,
    const JavaParamRef<jobject>& executor,
    const jboolean reverse_bypass) {
  std::vector<std::string> url_schemes;
  base::android::AppendJavaStringArrayToStringVector(env, jurl_schemes,
                                                     &url_schemes);
  std::vector<std::string> proxy_urls;
  base::android::AppendJavaStringArrayToStringVector(env, jproxy_urls,
                                                     &proxy_urls);
  std::vector<net::ProxyConfigServiceAndroid::ProxyOverrideRule> proxy_rules;
  int size = url_schemes.size();
  DCHECK(url_schemes.size() == proxy_urls.size());
  proxy_rules.reserve(size);
  for (int i = 0; i < size; i++) {
    proxy_rules.emplace_back(url_schemes[i], proxy_urls[i]);
  }
  std::vector<std::string> bypass_rules;
  base::android::AppendJavaStringArrayToStringVector(env, jbypass_rules,
                                                     &bypass_rules);
  std::string result;
  result = AwProxyConfigMonitor::GetInstance()->SetProxyOverride(
      proxy_rules, bypass_rules, reverse_bypass,
      base::BindOnce(&ProxyOverrideChanged,
                     ScopedJavaGlobalRef<jobject>(env, obj),
                     ScopedJavaGlobalRef<jobject>(env, listener),
                     ScopedJavaGlobalRef<jobject>(env, executor)));
  return base::android::ConvertUTF8ToJavaString(env, result);
}

void JNI_AwProxyController_ClearProxyOverride(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& listener,
    const JavaParamRef<jobject>& executor) {
  AwProxyConfigMonitor::GetInstance()->ClearProxyOverride(base::BindOnce(
      &ProxyOverrideChanged, ScopedJavaGlobalRef<jobject>(env, obj),
      ScopedJavaGlobalRef<jobject>(env, listener),
      ScopedJavaGlobalRef<jobject>(env, executor)));
}

}  // namespace android_webview
