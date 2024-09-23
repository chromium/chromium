// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <string>

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#include "components/version_info/version_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AboutSettingsBridge_jni.h"

const char kSeparator[] = " ";

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

static std::string JNI_AboutSettingsBridge_GetApplicationVersion(JNIEnv* env) {
  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();
  return base::JoinString({android_build_info->host_package_label(),
                           version_info::GetVersionNumber()},
                          kSeparator);
}

static std::string JNI_AboutSettingsBridge_GetOSVersion(JNIEnv* env) {
  return base::JoinString(
      {version_info::GetOSType(), AndroidAboutAppInfo::GetOsInfo()},
      kSeparator);
}
