// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/android/previews_android_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/PreviewsAndroidBridge_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_observer.h"
#include "content/public/browser/web_contents.h"

static jlong JNI_PreviewsAndroidBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new PreviewsAndroidBridge(env, obj));
}

// static
bool PreviewsAndroidBridge::CreateHttpsImageCompressionInfoBar(
    content::WebContents* web_contents) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);

  base::android::ScopedJavaLocalRef<jobject> j_tab_android =
      tab_android->GetJavaObject();
  DCHECK(!j_tab_android.is_null());

  return Java_PreviewsAndroidBridge_createHttpsImageCompressionInfoBar(
      base::android::AttachCurrentThread(), j_tab_android);
}

PreviewsAndroidBridge::PreviewsAndroidBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {}

PreviewsAndroidBridge::~PreviewsAndroidBridge() {}

jboolean PreviewsAndroidBridge::IsHttpsImageCompressionApplied(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;
  return subresource_redirect::SubresourceRedirectObserver::
      IsHttpsImageCompressionApplied(web_contents);
}
