// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_dialog_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/download/android/jni_headers/DownloadDialogBridge_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_features.h"
#include "components/prefs/pref_service.h"
#include "ui/android/window_android.h"

// Default minimum file size in kilobyte to trigger download later feature.
const int64_t kDownloadLaterDefaultMinFileSizeKb = 204800;

// -----------------------------------------------------------------------------
// DownloadDialogResult
DownloadDialogResult::DownloadDialogResult() = default;

DownloadDialogResult::DownloadDialogResult(const DownloadDialogResult&) =
    default;

DownloadDialogResult::~DownloadDialogResult() = default;

// -----------------------------------------------------------------------------
// DownloadDialogBridge.
DownloadDialogBridge::DownloadDialogBridge() : is_dialog_showing_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_DownloadDialogBridge_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
  DCHECK(!java_obj_.is_null());
}

DownloadDialogBridge::~DownloadDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadDialogBridge_destroy(env, java_obj_);
}

void DownloadDialogBridge::ShowDialog(
    gfx::NativeWindow native_window,
    int64_t total_bytes,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    DownloadLocationDialogType dialog_type,
    const base::FilePath& suggested_path,
    bool is_incognito,
    DialogCallback dialog_callback) {
  if (!native_window)
    return;

  UMA_HISTOGRAM_ENUMERATION("MobileDownload.Location.Dialog.Type", dialog_type);

  dialog_callback_ = std::move(dialog_callback);

  // This shouldn't happen, but if it does, cancel download.
  if (dialog_type == DownloadLocationDialogType::NO_DIALOG) {
    NOTREACHED();
    DownloadDialogResult dialog_result;
    dialog_result.location_result = DownloadLocationDialogResult::USER_CANCELED;
    CompleteSelection(std::move(dialog_result));
    return;
  }

  // If dialog is showing, run the callback to continue without confirmation.
  if (is_dialog_showing_) {
    DownloadDialogResult dialog_result;
    dialog_result.location_result =
        DownloadLocationDialogResult::DUPLICATE_DIALOG;
    dialog_result.file_path = suggested_path;
    CompleteSelection(std::move(dialog_result));
    return;
  }

  is_dialog_showing_ = true;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadDialogBridge_showDialog(
      env, java_obj_, native_window->GetJavaObject(),
      static_cast<long>(total_bytes), static_cast<int>(connection_type),
      static_cast<int>(dialog_type),
      base::android::ConvertUTF8ToJavaString(env,
                                             suggested_path.AsUTF8Unsafe()),
      false /*supports_later_dialog*/, is_incognito);
}

void DownloadDialogBridge::OnComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& returned_path,
    jboolean on_wifi,
    jlong start_time) {
  DownloadDialogResult dialog_result;
  dialog_result.location_result = DownloadLocationDialogResult::USER_CONFIRMED;
  dialog_result.file_path = base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, returned_path));

  if (on_wifi) {
    dialog_result.download_schedule =
        download::DownloadSchedule(true /*only_on_wifi*/, absl::nullopt);
  }
  if (start_time > 0) {
    dialog_result.download_schedule = download::DownloadSchedule(
        false /*only_on_wifi*/, base::Time::FromJavaTime(start_time));
  }

  CompleteSelection(std::move(dialog_result));
  is_dialog_showing_ = false;
}

void DownloadDialogBridge::OnCanceled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (dialog_callback_) {
    DownloadDialogResult dialog_result;
    dialog_result.location_result = DownloadLocationDialogResult::USER_CANCELED;
    CompleteSelection(std::move(dialog_result));
  }

  is_dialog_showing_ = false;
}

void DownloadDialogBridge::CompleteSelection(DownloadDialogResult result) {
  if (!dialog_callback_)
    return;

  UMA_HISTOGRAM_ENUMERATION("MobileDownload.Location.Dialog.Result",
                            result.location_result);
  std::move(dialog_callback_).Run(std::move(result));
}

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_DownloadDialogBridge_GetDownloadDefaultDirectory(JNIEnv* env) {
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();

  return base::android::ConvertUTF8ToJavaString(
      env, pref_service->GetString(prefs::kDownloadDefaultDirectory));
}

// static
void JNI_DownloadDialogBridge_SetDownloadAndSaveFileDefaultDirectory(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& directory) {
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();

  base::FilePath path(base::android::ConvertJavaStringToUTF8(env, directory));
  pref_service->SetFilePath(prefs::kDownloadDefaultDirectory, path);
  pref_service->SetFilePath(prefs::kSaveFileDefaultDirectory, path);
}

// static
jlong JNI_DownloadDialogBridge_GetDownloadLaterMinFileSize(JNIEnv* env) {
  return DownloadDialogBridge::GetDownloadLaterMinFileSize();
}

// static
jboolean JNI_DownloadDialogBridge_ShouldShowDateTimePicker(JNIEnv* env) {
  return DownloadDialogBridge::ShouldShowDateTimePicker();
}

jboolean JNI_DownloadDialogBridge_IsLocationDialogManaged(JNIEnv* env) {
  PrefService* pref_service =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();

  return pref_service->IsManagedPreference(prefs::kPromptForDownload);
}

// static
long DownloadDialogBridge::GetDownloadLaterMinFileSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      download::features::kDownloadLater,
      download::features::kDownloadLaterMinFileSizeKb,
      kDownloadLaterDefaultMinFileSizeKb);
}

// static
bool DownloadDialogBridge::ShouldShowDateTimePicker() {
  return base::GetFieldTrialParamByFeatureAsBool(
      download::features::kDownloadLater,
      download::features::kDownloadLaterShowDateTimePicker,
      /*default_value=*/true);
}
