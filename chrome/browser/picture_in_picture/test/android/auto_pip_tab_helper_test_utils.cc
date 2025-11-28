// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/android/jni_android.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/picture_in_picture/test/jni_headers/AutoPictureInPictureTabHelperTestUtils_jni.h"

namespace picture_in_picture {

static void JNI_AutoPictureInPictureTabHelperTestUtils_InitializeForTesting(
    JNIEnv* env,
    content::WebContents* web_contents) {
  AutoPictureInPictureTabHelper::GetOrCreateForWebContents(web_contents);
}

static jboolean
JNI_AutoPictureInPictureTabHelperTestUtils_IsInAutoPictureInPicture(
    JNIEnv* env,
    content::WebContents* web_contents) {
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  return tab_helper->IsInAutoPictureInPicture();
}

static jboolean
JNI_AutoPictureInPictureTabHelperTestUtils_HasAutoPictureInPictureBeenRegistered(
    JNIEnv* env,
    content::WebContents* web_contents) {
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  return tab_helper->HasAutoPictureInPictureBeenRegistered();
}

static jboolean
JNI_AutoPictureInPictureTabHelperTestUtils_HasPictureInPictureVideo(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return web_contents->HasPictureInPictureVideo();
}

static void
JNI_AutoPictureInPictureTabHelperTestUtils_SetHasHighMediaEngagement(
    JNIEnv* env,
    content::WebContents* web_contents,
    jboolean has_high_engagement) {
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  tab_helper->set_has_high_engagement_for_testing(has_high_engagement);
}

static jint
JNI_AutoPictureInPictureTabHelperTestUtils_GetDismissCountForTesting(
    JNIEnv* env,
    content::WebContents* web_contents,
    GURL& url) {
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  return tab_helper->GetDismissCountForTesting(url);
}

static void
JNI_AutoPictureInPictureTabHelperTestUtils_SetIsUsingCameraOrMicrophone(
    JNIEnv* env,
    content::WebContents* web_contents,
    jboolean is_using_camera_or_microphone) {
  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  tab_helper->set_is_using_camera_or_microphone_for_testing(
      is_using_camera_or_microphone);
}

}  // namespace picture_in_picture

DEFINE_JNI(AutoPictureInPictureTabHelperTestUtils)
