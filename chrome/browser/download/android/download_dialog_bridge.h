// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_DIALOG_BRIDGE_H_

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/download/download_dialog_types.h"
#include "net/base/network_change_notifier.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

// Contains all the user selection from download dialogs.
struct DownloadDialogResult {
  DownloadDialogResult();
  DownloadDialogResult(const DownloadDialogResult&);
  ~DownloadDialogResult();

  // Results from download location dialog.
  DownloadLocationDialogResult location_result =
      DownloadLocationDialogResult::USER_CONFIRMED;
  base::FilePath file_path;
};

// Used to show a dialog for the user to select download details, such as file
// location, file name. and download start time.
// TODO(xingliu): Move logic out of the bridge, and write a test.
class DownloadDialogBridge {
 public:
  using DialogCallback = base::OnceCallback<void(DownloadDialogResult)>;

  DownloadDialogBridge();
  DownloadDialogBridge(const DownloadDialogBridge&) = delete;
  DownloadDialogBridge& operator=(const DownloadDialogBridge&) = delete;

  virtual ~DownloadDialogBridge();

  // Shows the download dialog.
  virtual void ShowDialog(
      gfx::NativeWindow native_window,
      int64_t total_bytes,
      net::NetworkChangeNotifier::ConnectionType connection_type,
      DownloadLocationDialogType dialog_type,
      const base::FilePath& suggested_path,
      Profile* profile,
      DialogCallback dialog_callback);

  void OnComplete(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jstring>& returned_path);

  void OnCanceled(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // Called when the user finished the selections from download dialog.
  void CompleteSelection(DownloadDialogResult result);

  bool is_dialog_showing_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  DialogCallback dialog_callback_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_DIALOG_BRIDGE_H_
