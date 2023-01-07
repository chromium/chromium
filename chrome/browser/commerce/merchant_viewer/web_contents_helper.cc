// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/chrome_content_client.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"

#include "chrome/browser/commerce/merchant_viewer/android/jni_headers/WebContentsHelpers_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> JNI_WebContentsHelpers_CreateWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean initially_hidden,
    jboolean initialize_renderer) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content::WebContents::CreateParams params(profile);
  params.initially_hidden = static_cast<bool>(initially_hidden);
  params.desired_renderer_state =
      static_cast<bool>(initialize_renderer)
          ? content::WebContents::CreateParams::
                kInitializeAndWarmupRendererProcess
          : content::WebContents::CreateParams::kOkayToHaveRendererProcess;

  return content::WebContents::Create(params).release()->GetJavaWebContents();
}

static void JNI_WebContentsHelpers_SetUserAgentOverride(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jboolean j_override_in_new_tabs) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  embedder_support::SetDesktopUserAgentOverride(
      web_contents, embedder_support::GetUserAgentMetadata(),
      j_override_in_new_tabs);
}
