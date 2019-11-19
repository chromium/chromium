// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/nqe/network_quality_provider.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/NetworkQualityProvider_jni.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_quality_observer_factory.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

NetworkQualityProvider::NetworkQualityProvider(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj)
    : j_obj_(env, obj) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(g_browser_process);
  g_browser_process->network_quality_tracker()
      ->AddRTTAndThroughputEstimatesObserver(this);
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
}

NetworkQualityProvider::~NetworkQualityProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  g_browser_process->network_quality_tracker()
      ->RemoveRTTAndThroughputEstimatesObserver(this);
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
}

void NetworkQualityProvider::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkQualityProvider_onEffectiveConnectionTypeChanged(
      env, j_obj_, static_cast<int>(type));
}

void NetworkQualityProvider::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkQualityProvider_onRTTOrThroughputEstimatesComputed(
      env, j_obj_, http_rtt.InMilliseconds(), transport_rtt.InMilliseconds(),
      downstream_throughput_kbps);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jlong JNI_NetworkQualityProvider_Init(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new NetworkQualityProvider(env, obj));
}
