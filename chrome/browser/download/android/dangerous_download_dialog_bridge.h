// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DANGEROUS_DOWNLOAD_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DANGEROUS_DOWNLOAD_DIALOG_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_item.h"
#include "ui/gfx/native_widget_types.h"

// Class for showing dialogs to asks whether user wants to download a dangerous
// file.
class DangerousDownloadDialogBridge : public download::DownloadItem::Observer {
 public:
  DangerousDownloadDialogBridge();
  DangerousDownloadDialogBridge(const DangerousDownloadDialogBridge&) = delete;
  DangerousDownloadDialogBridge& operator=(
      const DangerousDownloadDialogBridge&) = delete;

  ~DangerousDownloadDialogBridge() override;

  // Called to create and show a dialog for a dangerous download.
  void Show(download::DownloadItem* download_item,
            ui::WindowAndroid* window_android);

  // Called from Java via JNI.
  void Accepted(JNIEnv* env,
                const base::android::JavaParamRef<jstring>& jdownload_guid);

  // Called from Java via JNI.
  void Cancelled(JNIEnv* env,
                 const base::android::JavaParamRef<jstring>& jdownload_guid);

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download_item) override;

 private:
  // Download items that are requesting the dialog. Could get deleted while
  // the dialog is showing.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DANGEROUS_DOWNLOAD_DIALOG_BRIDGE_H_
