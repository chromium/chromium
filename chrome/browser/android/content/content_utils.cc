// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ContentUtils_jni.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"

static base::android::ScopedJavaLocalRef<jstring>
JNI_ContentUtils_GetBrowserUserAgent(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, GetUserAgent());
}

static void JNI_ContentUtils_SetUserAgentOverride(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = version_info::GetProductNameAndVersionForUserAgent();
  std::string spoofed_ua =
      content::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  web_contents->SetUserAgentOverride(spoofed_ua, false);
}
