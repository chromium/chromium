// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preloading/android_prerender_manager.h"

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_features.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/ui/android/preloading/jni_headers/AndroidPrerenderManager_jni.h"

using base::android::JavaParamRef;

AndroidPrerenderManager::AndroidPrerenderManager(JNIEnv* env) {}

AndroidPrerenderManager::~AndroidPrerenderManager() = default;

// static
static jlong JNI_AndroidPrerenderManager_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new AndroidPrerenderManager(env));
}

void AndroidPrerenderManager::StartPrerendering(JNIEnv* env,
                                                const GURL& prerender_url,
                                                TabAndroid* tab) {
  auto* const preload_manager = GetNewTabPagePreloadPipelineManager(tab);
  if (!preload_manager) {
    return;
  }
  preload_manager->StartPrerender(
      prerender_url, chrome_preloading_predictor::kTouchOnNewTabPage);
}

void AndroidPrerenderManager::StopPrerendering(JNIEnv* env, TabAndroid* tab) {
  auto* const preload_manager = GetNewTabPagePreloadPipelineManager(tab);
  if (!preload_manager) {
    return;
  }
  preload_manager->ResetPrerender();
}

NewTabPagePreloadPipelineManager*
AndroidPrerenderManager::GetNewTabPagePreloadPipelineManager(TabAndroid* tab) {
  return tab ? tab->GetTabFeatures()->new_tab_page_preload_pipeline_manager()
             : nullptr;
}

DEFINE_JNI(AndroidPrerenderManager)
