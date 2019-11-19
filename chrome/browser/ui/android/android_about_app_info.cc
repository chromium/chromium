// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/android_about_app_info.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/system/sys_info.h"
#include "chrome/android/chrome_jni_headers/ChromeVersionInfo_jni.h"
#include "content/public/common/user_agent.h"

std::string AndroidAboutAppInfo::GetGmsInfo() {
  JNIEnv* env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jstring> info =
      Java_ChromeVersionInfo_getGmsInfo(env);
  return base::android::ConvertJavaStringToUTF8(env, info);
}

std::string AndroidAboutAppInfo::GetOsInfo() {
  return base::SysInfo::OperatingSystemVersion() +
         content::GetAndroidOSInfo(/*include_android_build_number=*/true);
}
