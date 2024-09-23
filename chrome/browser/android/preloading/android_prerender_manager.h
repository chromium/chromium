// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PRELOADING_ANDROID_PRERENDER_MANAGER_H_
#define CHROME_BROWSER_ANDROID_PRELOADING_ANDROID_PRERENDER_MANAGER_H_

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "url/android/gurl_android.h"

class AndroidPrerenderManager {
 public:
  AndroidPrerenderManager(JNIEnv* env, jobject obj);

  AndroidPrerenderManager(const AndroidPrerenderManager&) = delete;
  AndroidPrerenderManager& operator=(const AndroidPrerenderManager&) = delete;

  virtual ~AndroidPrerenderManager();

  bool StartPrerendering(
      JNIEnv* env,
      const GURL& prerender_url,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  void StopPrerendering(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_web_contents);

 private:
  base::WeakPtr<content::PrerenderHandle> prerender_handle_;
};

#endif  // CHROME_BROWSER_ANDROID_PRELOADING_ANDROID_PRERENDER_MANAGER_H_
