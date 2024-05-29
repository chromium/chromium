// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/content/jni_headers/ContentUtils_jni.h"

static base::android::ScopedJavaLocalRef<jstring>
JNI_ContentUtils_GetBrowserUserAgent(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, embedder_support::GetUserAgent());
}

static void JNI_ContentUtils_SetUserAgentOverride(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jboolean j_override_in_new_tabs) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  embedder_support::SetDesktopUserAgentOverride(
      web_contents, embedder_support::GetUserAgentMetadata(),
      j_override_in_new_tabs);
}
