// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/open_download_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/download/android/open_download_dialog_bridge_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/OpenDownloadDialogBridge_jni.h"

using base::android::JavaParamRef;

OpenDownloadDialogBridge::OpenDownloadDialogBridge(
    OpenDownloadDialogBridgeDelegate* delegate)
    : delegate_(delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_OpenDownloadDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

OpenDownloadDialogBridge::~OpenDownloadDialogBridge() {
  Java_OpenDownloadDialogBridge_destroy(base::android::AttachCurrentThread(),
                                        java_object_);
}

void OpenDownloadDialogBridge::Show(Profile* profile,
                                    const std::string& download_guid) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OpenDownloadDialogBridge_showDialog(
      env, java_object_, profile->GetJavaObject(), download_guid);
}

void OpenDownloadDialogBridge::OnConfirmed(JNIEnv* env,
                                           std::string& guid,
                                           jboolean accepted) {
  delegate_->OnConfirmed(guid, accepted);
}
