// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_utils.h"

#include "base/android/jni_string.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_utils.h"
#include "jni/DownloadUtils_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jstring> JNI_DownloadUtils_GetFailStateMessage(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint fail_state) {
  base::string16 message = OfflineItemUtils::GetFailStateMessage(
      static_cast<offline_items_collection::FailState>(fail_state));
  l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED, message);
  return ConvertUTF16ToJavaString(env, message);
}

static jint JNI_DownloadUtils_GetResumeMode(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jint failState) {
  auto reason = OfflineItemUtils::ConvertFailStateToDownloadInterruptReason(
      static_cast<offline_items_collection::FailState>(failState));
  return static_cast<jint>(download::GetDownloadResumeMode(
      reason, false /* restart_required */, true /* user_action_required */));
}

// static
base::FilePath DownloadUtils::GetUriStringForPath(
    const base::FilePath& file_path) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto uri_jstring = Java_DownloadUtils_getUriStringForPath(
      env,
      base::android::ConvertUTF8ToJavaString(env, file_path.AsUTF8Unsafe()));
  return base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, uri_jstring));
}
