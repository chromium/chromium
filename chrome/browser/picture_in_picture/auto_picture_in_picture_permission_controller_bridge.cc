// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_permission_controller_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutoPictureInPicturePermissionController_jni.h"

using base::android::JavaRef;

namespace picture_in_picture {

// Sets a flag to indicate that auto-pip has been triggered.
void AutoPictureInPicturePermissionControllerBridge::SetAutoPipTriggered(
    content::WebContents& web_contents) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_AutoPictureInPicturePermissionController_setAutoPipTriggered(
      env, web_contents.GetJavaWebContents());
}

// Clears the flag that indicates that auto-pip has been triggered.
void AutoPictureInPicturePermissionControllerBridge::ClearAutoPipTriggered(
    content::WebContents& web_contents) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_AutoPictureInPicturePermissionController_clearAutoPipTriggered(
      env, web_contents.GetJavaWebContents());
}

void AutoPictureInPicturePermissionControllerBridge::ClearAllowOnceState(
    content::WebContents& web_contents) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_AutoPictureInPicturePermissionController_clearAllowOnceState(
      env, web_contents.GetJavaWebContents());
}

jint JNI_AutoPictureInPicturePermissionController_GetPermissionStatus(
    JNIEnv* env,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return CONTENT_SETTING_BLOCK;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  return host_content_settings_map->GetContentSetting(
      web_contents->GetLastCommittedURL(), web_contents->GetLastCommittedURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
}

void JNI_AutoPictureInPicturePermissionController_SetPermissionStatus(
    JNIEnv* env,
    content::WebContents* web_contents,
    jint status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    // If the WebContents is gone, we cannot get the Profile to save the
    // setting. This might happen if the tab is closed immediately after the
    // user makes a choice. In this case, we just drop the update.
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  host_content_settings_map->SetContentSettingDefaultScope(
      web_contents->GetLastCommittedURL(), web_contents->GetLastCommittedURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
      static_cast<ContentSetting>(status));
}

}  // namespace picture_in_picture

DEFINE_JNI(AutoPictureInPicturePermissionController)
