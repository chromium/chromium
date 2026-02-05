// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
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

bool JNI_AutoPictureInPicturePermissionController_IsAutoPictureInPictureInUse(
    JNIEnv* env,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return false;
  }

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    return false;
  }

  return tab_helper->AreAutoPictureInPicturePreconditionsMet() ||
         tab_helper->IsInAutoPictureInPicture();
}

int32_t JNI_AutoPictureInPicturePermissionController_GetPermissionStatus(
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
    int32_t status) {
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

void JNI_AutoPictureInPicturePermissionController_OnPictureInPictureDismissed(
    JNIEnv* env,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return;
  }

  auto* tab_helper =
      AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  if (tab_helper) {
    tab_helper->OnPictureInPictureDismissed();
  }
}

}  // namespace picture_in_picture

DEFINE_JNI(AutoPictureInPicturePermissionController)
