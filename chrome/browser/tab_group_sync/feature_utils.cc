// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/feature_utils.h"

#include "build/build_config.h"
#include "components/saved_tab_groups/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/tab_group_sync/utils_jni_headers/TabGroupSyncFeatures_jni.h"
#endif

namespace tab_groups {

bool IsTabGroupSyncEnabled() {
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabGroupSyncFeatures_isTabGroupSyncEnabled(env);
#else
  return false;
#endif
}

}  // namespace tab_groups
