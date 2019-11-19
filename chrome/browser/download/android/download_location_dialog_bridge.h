// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_LOCATION_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_LOCATION_DIALOG_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/download/download_location_dialog_result.h"
#include "chrome/browser/download/download_location_dialog_type.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class FilePath;
}  // namespace base

class DownloadLocationDialogBridge {
 public:
  using LocationCallback = base::OnceCallback<void(DownloadLocationDialogResult,
                                                   const base::FilePath&)>;

  virtual ~DownloadLocationDialogBridge() = default;

  // Show a download location picker dialog to determine the download path.
  // The path selected by the user will be returned in |location_callback|.
  virtual void ShowDialog(gfx::NativeWindow native_window,
                          int64_t total_bytes,
                          DownloadLocationDialogType dialog_type,
                          const base::FilePath& suggested_path,
                          LocationCallback location_callback) = 0;

  virtual void OnComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& returned_path) = 0;

  virtual void OnCanceled(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj) = 0;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_LOCATION_DIALOG_BRIDGE_H_
