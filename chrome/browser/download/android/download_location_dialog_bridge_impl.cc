// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_location_dialog_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/android/chrome_jni_headers/DownloadLocationDialogBridge_jni.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/android/window_android.h"

DownloadLocationDialogBridgeImpl::DownloadLocationDialogBridgeImpl()
    : is_dialog_showing_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_DownloadLocationDialogBridge_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
  DCHECK(!java_obj_.is_null());
}

DownloadLocationDialogBridgeImpl::~DownloadLocationDialogBridgeImpl() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadLocationDialogBridge_destroy(env, java_obj_);
}

void DownloadLocationDialogBridgeImpl::ShowDialog(
    gfx::NativeWindow native_window,
    int64_t total_bytes,
    DownloadLocationDialogType dialog_type,
    const base::FilePath& suggested_path,
    LocationCallback location_callback) {
  if (!native_window)
    return;

  UMA_HISTOGRAM_ENUMERATION("MobileDownload.Location.Dialog.Type", dialog_type);

  location_callback_ = std::move(location_callback);

  // This shouldn't happen, but if it does, cancel download.
  if (dialog_type == DownloadLocationDialogType::NO_DIALOG) {
    NOTREACHED();
    CompleteLocationSelection(DownloadLocationDialogResult::USER_CANCELED,
                              base::FilePath());
    return;
  }

  // If dialog is showing, run the callback to continue without confirmation.
  if (is_dialog_showing_) {
    CompleteLocationSelection(DownloadLocationDialogResult::DUPLICATE_DIALOG,
                              std::move(suggested_path));
    return;
  }

  is_dialog_showing_ = true;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadLocationDialogBridge_showDialog(
      env, java_obj_, native_window->GetJavaObject(),
      static_cast<long>(total_bytes), static_cast<int>(dialog_type),
      base::android::ConvertUTF8ToJavaString(env,
                                             suggested_path.AsUTF8Unsafe()));
}

void DownloadLocationDialogBridgeImpl::OnComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& returned_path) {
  base::FilePath path(
      base::android::ConvertJavaStringToUTF8(env, returned_path));

  CompleteLocationSelection(DownloadLocationDialogResult::USER_CONFIRMED, path);

  is_dialog_showing_ = false;
}

void DownloadLocationDialogBridgeImpl::OnCanceled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (location_callback_) {
    CompleteLocationSelection(DownloadLocationDialogResult::USER_CANCELED,
                              base::FilePath());
  }

  is_dialog_showing_ = false;
}

void DownloadLocationDialogBridgeImpl::CompleteLocationSelection(
    DownloadLocationDialogResult result,
    base::FilePath file_path) {
  if (location_callback_) {
    UMA_HISTOGRAM_ENUMERATION("MobileDownload.Location.Dialog.Result", result);
    std::move(location_callback_).Run(result, std::move(file_path));
  }
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_DownloadLocationDialogBridge_GetDownloadDefaultDirectory(JNIEnv* env) {
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();

  return base::android::ConvertUTF8ToJavaString(
      env, pref_service->GetString(prefs::kDownloadDefaultDirectory));
}

static void
JNI_DownloadLocationDialogBridge_SetDownloadAndSaveFileDefaultDirectory(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& directory) {
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();

  base::FilePath path(base::android::ConvertJavaStringToUTF8(env, directory));
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  pref_service->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
}
