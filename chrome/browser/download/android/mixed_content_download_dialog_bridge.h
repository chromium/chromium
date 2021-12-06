// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_DIALOG_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "chrome/browser/download/android/download_callback_validator.h"
#include "components/download/public/common/download_item.h"
#include "ui/gfx/native_widget_types.h"

// Class for showing dialogs to asks whether user wants to download a mixed
// content URL.
class MixedContentDownloadDialogBridge
    : public download::DownloadItem::Observer {
 public:
  using MixedContentDialogCallback =
      base::OnceCallback<void(bool /* accept */)>;

  static MixedContentDownloadDialogBridge* GetInstance();

  MixedContentDownloadDialogBridge();
  MixedContentDownloadDialogBridge(const MixedContentDownloadDialogBridge&) =
      delete;
  MixedContentDownloadDialogBridge& operator=(
      const MixedContentDownloadDialogBridge&) = delete;

  ~MixedContentDownloadDialogBridge() override;

  // Called to create and show a dialog for a mixed-content download.
  void CreateDialog(download::DownloadItem* download,
                    const base::FilePath& base_name,
                    ui::WindowAndroid* window_android,
                    MixedContentDialogCallback callback);

  // Called from Java via JNI.
  void OnConfirmed(JNIEnv* env, jlong callback_id, jboolean accepted);

 private:
  // Download items that are requesting the dialog. Could get deleted while
  // the dialog is showing.
  std::vector<download::DownloadItem*> download_items_;

  // Validator for all JNI callbacks.
  DownloadCallbackValidator validator_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_DIALOG_BRIDGE_H_
