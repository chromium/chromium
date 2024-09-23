// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preloading/android_prerender_manager.h"

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/ui/android/preloading/jni_headers/AndroidPrerenderManager_jni.h"

using base::android::JavaParamRef;

AndroidPrerenderManager::AndroidPrerenderManager(JNIEnv* env, jobject obj) {}

AndroidPrerenderManager::~AndroidPrerenderManager() = default;

// static
jlong JNI_AndroidPrerenderManager_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new AndroidPrerenderManager(env, caller));
}

bool AndroidPrerenderManager::StartPrerendering(
    JNIEnv* env,
    const GURL& prerender_url,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* const web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  PrerenderManager::CreateForWebContents(web_contents);
  auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
  CHECK(prerender_manager);
  prerender_handle_ = prerender_manager->StartPrerenderNewTabPage(
      prerender_url, chrome_preloading_predictor::kTouchOnNewTabPage);
  return prerender_handle_ != nullptr;
}

void AndroidPrerenderManager::StopPrerendering(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  if (prerender_handle_) {
    content::WebContents* const web_contents =
        content::WebContents::FromJavaWebContents(j_web_contents);
    auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
    CHECK(prerender_manager);
    prerender_manager->StopPrerenderNewTabPage(prerender_handle_);
    prerender_handle_ = nullptr;
  }
}
