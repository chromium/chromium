// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_INSECURE_DOWNLOAD_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_INSECURE_DOWNLOAD_DIALOG_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/android/download_callback_validator.h"
#include "components/download/public/common/download_item.h"
#include "ui/gfx/native_widget_types.h"

// Class for showing dialogs to asks whether user wants to download an insecure
// URL.
class InsecureDownloadDialogBridge : public download::DownloadItem::Observer {
 public:
  using InsecureDownloadDialogCallback =
      base::OnceCallback<void(bool /* accept */)>;

  static InsecureDownloadDialogBridge* GetInstance();

  InsecureDownloadDialogBridge();
  InsecureDownloadDialogBridge(const InsecureDownloadDialogBridge&) = delete;
  InsecureDownloadDialogBridge& operator=(const InsecureDownloadDialogBridge&) =
      delete;

  ~InsecureDownloadDialogBridge() override;

  // Called to create and show a dialog for an insecure download.
  void CreateDialog(download::DownloadItem* download,
                    const base::FilePath& base_name,
                    ui::WindowAndroid* window_android,
                    InsecureDownloadDialogCallback callback);

  // Called from Java via JNI.
  void OnConfirmed(JNIEnv* env, jlong callback_id, jboolean accepted);

 private:
  // Download items that are requesting the dialog. Could get deleted while
  // the dialog is showing.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items_;

  // Validator for all JNI callbacks.
  DownloadCallbackValidator validator_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_INSECURE_DOWNLOAD_DIALOG_BRIDGE_H_
