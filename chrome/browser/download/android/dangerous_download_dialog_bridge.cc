// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/dangerous_download_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/DangerousDownloadDialogBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

DangerousDownloadDialogBridge::DangerousDownloadDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_DangerousDownloadDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

DangerousDownloadDialogBridge::~DangerousDownloadDialogBridge() {
  for (download::DownloadItem* download_item : download_items_) {
    download_item->RemoveObserver(this);
  }
  Java_DangerousDownloadDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void DangerousDownloadDialogBridge::Show(download::DownloadItem* download_item,
                                         ui::WindowAndroid* window_android) {
  // Don't show dangerous download again if it is already showing.
  if (base::Contains(download_items_, download_item)) {
    return;
  }
  if (!window_android) {
    download_item->Remove();
    return;
  }
  download_item->AddObserver(this);
  download_items_.push_back(download_item);

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_DangerousDownloadDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, download_item->GetGuid()),
      base::android::ConvertUTF16ToJavaString(
          env,
          base::UTF8ToUTF16(download_item->GetFileNameToReportUser().value())),
      download_item->GetTotalBytes(),
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_WARNING));
}

void DangerousDownloadDialogBridge::OnDownloadDestroyed(
    download::DownloadItem* download_item) {
  auto iter = base::ranges::find(download_items_, download_item);
  if (iter != download_items_.end()) {
    (*iter)->RemoveObserver(this);
    download_items_.erase(iter);
  }
}

void DangerousDownloadDialogBridge::Accepted(
    JNIEnv* env,
    const JavaParamRef<jstring>& jdownload_guid) {
  download::DownloadItem* download = DownloadDialogUtils::FindAndRemoveDownload(
      &download_items_, ConvertJavaStringToUTF8(env, jdownload_guid));
  if (download) {
    download->RemoveObserver(this);
    download->ValidateDangerousDownload();
  }
}

void DangerousDownloadDialogBridge::Cancelled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jdownload_guid) {
  download::DownloadItem* download = DownloadDialogUtils::FindAndRemoveDownload(
      &download_items_, ConvertJavaStringToUTF8(env, jdownload_guid));
  if (download) {
    download->RemoveObserver(this);
    download->Remove();
  }
}
