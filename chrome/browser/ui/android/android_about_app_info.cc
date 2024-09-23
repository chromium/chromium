// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/android_about_app_info.h"

#include <jni.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/system/sys_info.h"
#include "content/public/common/user_agent.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PlayServicesVersionInfo_jni.h"

std::string AndroidAboutAppInfo::GetGmsInfo() {
  JNIEnv* env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jstring> info =
      Java_PlayServicesVersionInfo_getGmsInfo(env);
  return base::android::ConvertJavaStringToUTF8(env, info);
}

std::string AndroidAboutAppInfo::GetOsInfo() {
  return base::SysInfo::OperatingSystemVersion() +
         content::GetAndroidOSInfo(content::IncludeAndroidBuildNumber::Include,
                                   content::IncludeAndroidModel::Include);
}

std::string AndroidAboutAppInfo::GetTargetsUInfo() {
  std::string targets_u_info =
      base::android::BuildInfo::GetInstance()->is_at_least_u() ? "true"
                                                               : "false";
  targets_u_info += "/";
  targets_u_info +=
      base::android::BuildInfo::GetInstance()->targets_at_least_u() ? "true"
                                                                    : "false";
  return targets_u_info;
}
