// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_DIALOG_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {
class WebContents;
}  // namespace content

class MediaCapturePickerDialogBridge {
 public:
  // Callback for when media to capture is picked. The `DesktopMediaID` will
  // be null if nothing was picked.
  using MediaCapturePickerDialogCallback =
      base::OnceCallback<void(const content::DesktopMediaID&)>;

  MediaCapturePickerDialogBridge();
  MediaCapturePickerDialogBridge(const MediaCapturePickerDialogBridge&) =
      delete;
  MediaCapturePickerDialogBridge& operator=(
      const MediaCapturePickerDialogBridge&) = delete;
  MediaCapturePickerDialogBridge(MediaCapturePickerDialogBridge&&) = delete;
  MediaCapturePickerDialogBridge& operator=(MediaCapturePickerDialogBridge&&) =
      delete;
  ~MediaCapturePickerDialogBridge();

  // Shows a dialog to select a media source to capture. The initiator (not
  // necessarily target) of the capture request is `web_contents`.
  void Show(content::WebContents* web_contents,
            const std::u16string& app_name,
            MediaCapturePickerDialogCallback callback);

  // Called from Java via JNI when the dialog resolves.
  void OnResult(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& java_web_contents);

 private:
  MediaCapturePickerDialogCallback callback_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_DIALOG_BRIDGE_H_
