// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_LOCATION_DIALOG_BRIDGE_IMPL_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_LOCATION_DIALOG_BRIDGE_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "chrome/browser/download/android/download_location_dialog_bridge.h"
#include "chrome/browser/download/download_location_dialog_type.h"
#include "ui/gfx/native_widget_types.h"

class DownloadLocationDialogBridgeImpl : public DownloadLocationDialogBridge {
 public:
  DownloadLocationDialogBridgeImpl();
  ~DownloadLocationDialogBridgeImpl() override;

  // DownloadLocationDialogBridge implementation.
  void ShowDialog(gfx::NativeWindow native_window,
                  int64_t total_bytes,
                  DownloadLocationDialogType dialog_type,
                  const base::FilePath& suggested_path,
                  LocationCallback location_callback) override;

  void OnComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& returned_path) override;

  void OnCanceled(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj) override;

 private:
  // Called when the download location is selected by the user.
  void CompleteLocationSelection(DownloadLocationDialogResult result,
                                 base::FilePath file_path);

  bool is_dialog_showing_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  LocationCallback location_callback_;

  DISALLOW_COPY_AND_ASSIGN(DownloadLocationDialogBridgeImpl);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_LOCATION_DIALOG_BRIDGE_IMPL_H_
