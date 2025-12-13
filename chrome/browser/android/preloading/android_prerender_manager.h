// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PRELOADING_ANDROID_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_ANDROID_PRELOADING_ANDROID_PRERENDER_MANAGER_H_

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "url/android/gurl_android.h"

class NewTabPagePreloadPipelineManager;
class TabAndroid;

// This object is owned through a Java-side singletone object, and the object
// can be shared by multiple tabs (WebContents).
class AndroidPrerenderManager {
 public:
  explicit AndroidPrerenderManager(JNIEnv* env);

  AndroidPrerenderManager(const AndroidPrerenderManager&) = delete;
  AndroidPrerenderManager& operator=(const AndroidPrerenderManager&) = delete;

  virtual ~AndroidPrerenderManager();

  void StartPrerendering(JNIEnv* env,
                         const GURL& prerender_url,
                         TabAndroid* tab);

  void StopPrerendering(JNIEnv* env, TabAndroid* tab);

 private:
  NewTabPagePreloadPipelineManager* GetNewTabPagePreloadPipelineManager(
      TabAndroid* tab);
};

#endif  // CHROME_BROWSER_ANDROID_PRELOADING_ANDROID_PRERENDER_MANAGER_H_
