// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SiteIsolator_jni.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/site_instance.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

void JNI_SiteIsolator_StartIsolatingSite(JNIEnv* env,
                                         const JavaParamRef<jobject>& j_profile,
                                         const JavaParamRef<jobject>& j_gurl) {
  std::unique_ptr<GURL> gurl = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  content::SiteInstance::StartIsolatingSite(
      content::BrowserContextFromJavaHandle(j_profile), *gurl,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED);
}
