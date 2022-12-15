// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ContentUtils_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/common/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

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

static void JNI_ContentUtils_SetUserAgent(
        JNIEnv* env,
        const base::android::JavaParamRef<jobject>& jweb_contents,
        const base::android::JavaParamRef<jstring>& ua,
        jboolean is_mobile) {
    content::WebContents* web_contents =
            content::WebContents::FromJavaWebContents(jweb_contents);
    std::string userAgent = ConvertJavaStringToUTF8(env, ua);
    embedder_support::SetUserAgentOverride(web_contents, userAgent, is_mobile);
    web_contents->SetRendererInitiatedUserAgentOverrideOption(
            content::NavigationController::UA_OVERRIDE_INHERIT);

    content::NavigationEntry* entry =
            web_contents->GetController().GetLastCommittedEntry();
    if (!entry)
        return;

    entry->SetIsOverridingUserAgent(true);
    web_contents->NotifyPreferencesChanged();
    web_contents->GetController().Reload(
            content::ReloadType::ORIGINAL_REQUEST_URL, true);
}

// loads_images_automatically
static void JNI_ContentUtils_SetLoadsImagesAutomatically(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jboolean loads_images_automatically) {
    Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
    profile->GetPrefs()->SetBoolean(
        prefs::kWebKitLoadsImagesAutomatically, loads_images_automatically);
}

// images_enabled
static void JNI_ContentUtils_SetImagesEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jboolean enable) {
    Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
    profile->GetPrefs()->SetBoolean(prefs::kWebKitImagesEnabled, enable);
}
