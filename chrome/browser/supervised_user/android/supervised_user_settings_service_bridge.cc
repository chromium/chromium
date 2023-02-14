// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserSettingsBridge_jni.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

using base::android::JavaParamRef;

void JNI_SupervisedUserSettingsBridge_SetFilteringBehavior(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint setting) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(setting));
}
