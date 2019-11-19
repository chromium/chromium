// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <string>

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "chrome/android/chrome_jni_headers/AboutSettingsBridge_jni.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#include "components/version_info/version_info.h"

const char kSeparator[] = " ";

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jstring>
JNI_AboutSettingsBridge_GetApplicationVersion(JNIEnv* env) {
  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();
  std::string application =
      base::JoinString({android_build_info->host_package_label(),
                        version_info::GetVersionNumber()},
                       kSeparator);

  return ConvertUTF8ToJavaString(env, application);
}

static ScopedJavaLocalRef<jstring> JNI_AboutSettingsBridge_GetOSVersion(
    JNIEnv* env) {
  std::string os_version = base::JoinString(
      {version_info::GetOSType(), AndroidAboutAppInfo::GetOsInfo()},
      kSeparator);

  return ConvertUTF8ToJavaString(env, os_version);
}
