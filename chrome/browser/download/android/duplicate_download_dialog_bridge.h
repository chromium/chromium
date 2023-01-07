// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_DIALOG_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/download/android/download_callback_validator.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_item.h"

namespace content {
class WebContents;
}  // namespace content

// Class for showing dialogs to asks whether user wants to download a file
// that already exists on disk
class DuplicateDownloadDialogBridge : public download::DownloadItem::Observer {
 public:
  using DuplicateDownloadDialogCallback =
      base::OnceCallback<void(bool /*accepted*/)>;
  // static
  static DuplicateDownloadDialogBridge* GetInstance();

  DuplicateDownloadDialogBridge();
  DuplicateDownloadDialogBridge(const DuplicateDownloadDialogBridge&) = delete;
  DuplicateDownloadDialogBridge& operator=(
      const DuplicateDownloadDialogBridge&) = delete;

  ~DuplicateDownloadDialogBridge() override;

  // Called to create and show a dialog for a duplicate download.
  void Show(const std::string& file_path,
            const std::string& page_url,
            int64_t total_bytes,
            bool duplicate_request_exists,
            content::WebContents* web_contents,
            DuplicateDownloadDialogCallback callback);

  // Called from Java via JNI.
  void OnConfirmed(JNIEnv* env, jlong callback_id, jboolean accepted);

 private:
  // Validator for all JNI callbacks.
  DownloadCallbackValidator validator_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DUPLICATE_DOWNLOAD_DIALOG_BRIDGE_H_
