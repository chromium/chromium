// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/policy_warning_download_dialog_bridge.h"

#include <algorithm>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/PolicyWarningDownloadDialogBridge_jni.h"

PolicyWarningDownloadDialogBridge::PolicyWarningDownloadDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_PolicyWarningDownloadDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

PolicyWarningDownloadDialogBridge::~PolicyWarningDownloadDialogBridge() {
  for (download::DownloadItem* download_item : download_items_) {
    download_item->RemoveObserver(this);
  }
  Java_PolicyWarningDownloadDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void PolicyWarningDownloadDialogBridge::Show(
    download::DownloadItem* download_item,
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
  Java_PolicyWarningDownloadDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(),
      download_item->GetGuid(),
      base::UTF8ToUTF16(download_item->GetFileNameToReportUser().value()));
}

void PolicyWarningDownloadDialogBridge::OnDownloadDestroyed(
    download::DownloadItem* download_item) {
  auto iter = std::ranges::find(download_items_, download_item);
  if (iter != download_items_.end()) {
    (*iter)->RemoveObserver(this);
    download_items_.erase(iter);
  }
}

void PolicyWarningDownloadDialogBridge::Accepted(JNIEnv* env,
                                                 std::string& download_guid) {
  download::DownloadItem* download = DownloadDialogUtils::FindAndRemoveDownload(
      &download_items_, download_guid);
  if (download) {
    download->RemoveObserver(this);
    download->ValidateDangerousDownload();
  }
}

void PolicyWarningDownloadDialogBridge::Cancelled(JNIEnv* env,
                                                  std::string& download_guid) {
  download::DownloadItem* download = DownloadDialogUtils::FindAndRemoveDownload(
      &download_items_, download_guid);
  if (download) {
    download->RemoveObserver(this);
    download->Remove();
  }
}

DEFINE_JNI(PolicyWarningDownloadDialogBridge)
