// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/PermissionSettingsBridge_jni.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_manager.h"

using base::android::JavaParamRef;

static jboolean JNI_PermissionSettingsBridge_ShouldShowNotificationsPromo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& jweb_contents) {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(
          content::WebContents::FromJavaWebContents(jweb_contents));
  return manager->IsRequestInProgress() &&
         manager->Requests()[0]->GetContentSettingsType() ==
             ContentSettingsType::NOTIFICATIONS &&
         QuietNotificationPermissionUiState::ShouldShowPromo(
             ProfileAndroid::FromProfileAndroid(jprofile));
}

static void JNI_PermissionSettingsBridge_DidShowNotificationsPromo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  QuietNotificationPermissionUiState::PromoWasShown(
      ProfileAndroid::FromProfileAndroid(jprofile));
}
