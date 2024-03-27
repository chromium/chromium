// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_OPEN_DOWNLOAD_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_OPEN_DOWNLOAD_DIALOG_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/android/download_callback_validator.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_item.h"

class OpenDownloadDialogBridgeDelegate;
class Profile;

// Class for showing dialogs to asks whether user wants to open a downloaded
// file from an external app.
class OpenDownloadDialogBridge : public download::DownloadItem::Observer {
 public:
  using OpenDownloadDialogCallback =
      base::OnceCallback<void(bool /*accepted*/)>;

  explicit OpenDownloadDialogBridge(OpenDownloadDialogBridgeDelegate* delegate);
  OpenDownloadDialogBridge(const OpenDownloadDialogBridge&) = delete;
  OpenDownloadDialogBridge& operator=(const OpenDownloadDialogBridge&) = delete;

  ~OpenDownloadDialogBridge() override;

  // Called to create and show a dialog for a download.
  void Show(Profile* profile, const std::string& download_guid);

  // Called from Java via JNI.
  void OnConfirmed(JNIEnv* env, std::string& j_guid, jboolean accepted);

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  raw_ptr<OpenDownloadDialogBridgeDelegate> delegate_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_OPEN_DOWNLOAD_DIALOG_BRIDGE_H_
