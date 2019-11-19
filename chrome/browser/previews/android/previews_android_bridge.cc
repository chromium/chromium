// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/android/previews_android_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/PreviewsAndroidBridge_jni.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/web_contents.h"

static jlong JNI_PreviewsAndroidBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new PreviewsAndroidBridge(env, obj));
}

PreviewsAndroidBridge::PreviewsAndroidBridge(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {}

PreviewsAndroidBridge::~PreviewsAndroidBridge() {}

jboolean PreviewsAndroidBridge::ShouldShowPreviewUI(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return false;

  // Do not show the lite page chip between navigation start and navigation
  // finish.
  if (web_contents->GetController().GetPendingEntry())
    return false;

  PreviewsUITabHelper* tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return false;

  return tab_helper->should_display_android_omnibox_badge();
}

base::android::ScopedJavaLocalRef<jstring>
PreviewsAndroidBridge::GetLitePageRedirectOriginalURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_visible_url) {
  GURL visible_url(base::android::ConvertJavaStringToUTF16(env, j_visible_url));
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(visible_url,
                                                          &original_url)) {
    return base::android::ScopedJavaLocalRef<jstring>(
        base::android::ConvertUTF8ToJavaString(env, original_url));
  }

  return nullptr;
}

base::android::ScopedJavaLocalRef<jstring>
PreviewsAndroidBridge::GetStalePreviewTimestamp(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return base::android::ScopedJavaLocalRef<jstring>();

  PreviewsUITabHelper* tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return base::android::ScopedJavaLocalRef<jstring>();

  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF16ToJavaString(
          env, tab_helper->GetStalePreviewTimestampText()));
}

void PreviewsAndroidBridge::LoadOriginal(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return;

  PreviewsUITabHelper* tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;

  tab_helper->ReloadWithoutPreviews();
}

base::android::ScopedJavaLocalRef<jstring>
PreviewsAndroidBridge::GetPreviewsType(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  if (!web_contents)
    return base::android::ScopedJavaLocalRef<jstring>();

  PreviewsUITabHelper* tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return base::android::ScopedJavaLocalRef<jstring>();

  previews::PreviewsUserData* data = tab_helper->previews_user_data();
  if (!data || !data->HasCommittedPreviewsType())
    return base::android::ScopedJavaLocalRef<jstring>();

  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(
          env, previews::GetStringNameForType(data->CommittedPreviewsType())));
}
