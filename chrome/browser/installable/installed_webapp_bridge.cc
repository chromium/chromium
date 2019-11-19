// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installed_webapp_bridge.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "chrome/android/chrome_jni_headers/InstalledWebappBridge_jni.h"
#include "components/content_settings/core/common/content_settings.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

static void JNI_InstalledWebappBridge_NotifyPermissionsChange(JNIEnv* env,
    jlong j_provider) {
  InstalledWebappProvider* provider =
    reinterpret_cast<InstalledWebappProvider*>(j_provider);
  provider->Notify();
}

InstalledWebappProvider::RuleList
InstalledWebappBridge::GetInstalledWebappNotificationPermissions() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_permissions =
      Java_InstalledWebappBridge_getNotificationPermissions(env);

  InstalledWebappProvider::RuleList rules;
  for (auto j_permission : j_permissions.ReadElements<jobject>()) {
    GURL origin(ConvertJavaStringToUTF8(
        Java_InstalledWebappBridge_getOriginFromPermission(env, j_permission)));
    ContentSetting setting = IntToContentSetting(
        Java_InstalledWebappBridge_getSettingFromPermission(env, j_permission));
    rules.push_back(std::make_pair(origin, setting));
  }

  return rules;
}

void InstalledWebappBridge::SetProviderInstance(
    InstalledWebappProvider *provider) {
  Java_InstalledWebappBridge_setInstalledWebappProvider(
      base::android::AttachCurrentThread(), (jlong) provider);
}
