// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/test_support_jni_headers/PrerenderTestHelper_jni.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"

// Test helper method required by Java layer.

static jboolean JNI_PrerenderTestHelper_HasPrerenderedUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jurl) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return false;

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return false;

  auto* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile);
  if (!prerender_manager)
    return false;

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  auto contents = prerender_manager->GetAllPrerenderingContents();
  for (auto* content : contents) {
    auto* prerender_contents = prerender_manager->GetPrerenderContents(content);
    if (prerender_contents->prerender_url() == url &&
        prerender_contents->has_finished_loading()) {
      return true;
    }
  }
  return false;
}
