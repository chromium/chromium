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
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/ui/android/preloading/jni_headers/AndroidPrerenderManager_jni.h"

using base::android::JavaRef;

// static
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

// static
void AndroidPrerenderManager::StopPrerendering(JNIEnv* env, TabAndroid* tab) {
  auto* const preload_manager = GetNewTabPagePreloadPipelineManager(tab);
  if (!preload_manager) {
    return;
  }
  preload_manager->ResetPrerender();
}

// static
void AndroidPrerenderManager::StartPrewarm(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto* prerender_manager =
      PrerenderManager::GetOrCreateForWebContents(web_contents);
  CHECK(prerender_manager);
  prerender_manager->MaybeStartPrewarmSearchResult();
}

// static
NewTabPagePreloadPipelineManager*
AndroidPrerenderManager::GetNewTabPagePreloadPipelineManager(TabAndroid* tab) {
  return NewTabPagePreloadPipelineManager::From(tab);
}

static void JNI_AndroidPrerenderManager_StartPrerendering(
    JNIEnv* env,
    const GURL& prerender_url,
    TabAndroid* tab) {
  AndroidPrerenderManager::StartPrerendering(env, prerender_url, tab);
}

static void JNI_AndroidPrerenderManager_StopPrerendering(JNIEnv* env,
                                                         TabAndroid* tab) {
  AndroidPrerenderManager::StopPrerendering(env, tab);
}

static void JNI_AndroidPrerenderManager_StartPrewarm(
    content::WebContents* web_contents) {
  AndroidPrerenderManager::StartPrewarm(web_contents);
}

DEFINE_JNI(AndroidPrerenderManager)
