// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/android_about_app_info.h"

#include <jni.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/system/sys_info.h"
#include "components/embedder_support/user_agent_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PlayServicesVersionInfo_jni.h"

std::string AndroidAboutAppInfo::GetGmsInfo() {
  return Java_PlayServicesVersionInfo_getGmsInfo(
      base::android::AttachCurrentThread());
}

std::string AndroidAboutAppInfo::GetOsInfo() {
  return base::SysInfo::OperatingSystemVersion() +
         embedder_support::GetAndroidOSInfo(
             embedder_support::IncludeAndroidBuildNumber::Include,
             embedder_support::IncludeAndroidModel::Include);
}
