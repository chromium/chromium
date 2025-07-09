// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "chrome/browser/picture_in_picture/test/jni_headers/AutoPictureInPictureTabHelperTestUtils_jni.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;

namespace picture_in_picture {

void JNI_AutoPictureInPictureTabHelperTestUtils_InitializeForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  AutoPictureInPictureTabHelper::GetOrCreateForWebContents(web_contents);
}

jboolean JNI_AutoPictureInPictureTabHelperTestUtils_IsInAutoPictureInPicture(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  return tab_helper->IsInAutoPictureInPicture();
}

jboolean
JNI_AutoPictureInPictureTabHelperTestUtils_HasAutoPictureInPictureBeenRegistered(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  return tab_helper->HasAutoPictureInPictureBeenRegistered();
}

void JNI_AutoPictureInPictureTabHelperTestUtils_SetHasHighMediaEngagement(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    jboolean has_high_engagement) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  tab_helper->set_has_high_engagement_for_testing(has_high_engagement);
}

void JNI_AutoPictureInPictureTabHelperTestUtils_SetHasAudioFocusForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
    jboolean has_focus) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  tab_helper->set_has_audio_focus_for_testing(has_focus);
}

}  // namespace picture_in_picture
