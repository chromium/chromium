// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/duplicate_download_dialog_bridge.h"

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
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DuplicateDownloadDialogBridge_jni.h"

using base::android::JavaParamRef;

// static
DuplicateDownloadDialogBridge* DuplicateDownloadDialogBridge::GetInstance() {
  return base::Singleton<DuplicateDownloadDialogBridge>::get();
}

DuplicateDownloadDialogBridge::DuplicateDownloadDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_DuplicateDownloadDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

DuplicateDownloadDialogBridge::~DuplicateDownloadDialogBridge() {
  Java_DuplicateDownloadDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void DuplicateDownloadDialogBridge::Show(
    const std::string& file_path,
    const std::string& page_url,
    int64_t total_bytes,
    bool duplicate_request_exists,
    content::WebContents* web_contents,
    DuplicateDownloadDialogCallback callback) {
  DCHECK(web_contents);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_otr_profile_id;
  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  // If belongs to an off-the-record profile, then the OTRProfileID should be
  // taken from the browser context to support multiple off-the-record profiles.
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (browser_context && browser_context->IsOffTheRecord()) {
    std::optional<Profile::OTRProfileID> otr_profile_id =
        Profile::FromBrowserContext(browser_context)->GetOTRProfileID();
    if (otr_profile_id) {
      j_otr_profile_id = otr_profile_id->ConvertToJavaOTRProfileID(env);
    }
  }
  // Copy |callback| on the heap to pass the pointer through JNI. This callback
  // will be deleted when it's run.
  CHECK(!callback.is_null());
  jlong callback_id = reinterpret_cast<jlong>(
      new DuplicateDownloadDialogCallback(std::move(callback)));
  validator_.AddJavaCallback(callback_id);
  Java_DuplicateDownloadDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(), file_path, page_url,
      total_bytes, duplicate_request_exists, j_otr_profile_id, callback_id);
}

void DuplicateDownloadDialogBridge::OnConfirmed(JNIEnv* env,
                                                jlong callback_id,
                                                jboolean accepted) {
  if (!validator_.ValidateAndClearJavaCallback(callback_id))
    return;
  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<DuplicateDownloadDialogCallback> cb(
      reinterpret_cast<DuplicateDownloadDialogCallback*>(callback_id));
  std::move(*cb).Run(accepted);
}
