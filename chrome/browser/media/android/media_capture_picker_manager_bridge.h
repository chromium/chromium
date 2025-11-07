// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_MANAGER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {
class WebContents;
}  // namespace content

class MediaCapturePickerManagerBridge {
 public:
  MediaCapturePickerManagerBridge();
  MediaCapturePickerManagerBridge(const MediaCapturePickerManagerBridge&) =
      delete;
  MediaCapturePickerManagerBridge& operator=(
      const MediaCapturePickerManagerBridge&) = delete;
  MediaCapturePickerManagerBridge(MediaCapturePickerManagerBridge&&) = delete;
  MediaCapturePickerManagerBridge& operator=(
      MediaCapturePickerManagerBridge&&) = delete;
  ~MediaCapturePickerManagerBridge();

  // Shows a dialog to select a media source to capture. The initiator (not
  // necessarily target) of the capture request is `web_contents`.
  void Show(content::WebContents* web_contents,
            const std::u16string& app_name,
            bool request_audio,
            DesktopMediaPicker::DoneCallback callback);

  // Called from Java via JNI when the dialog resolves.
  void OnPickTab(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& java_web_contents,
                 bool audio_share);

  // Called from Java via JNI when the dialog resolves.
  void OnPickWindow(JNIEnv* env);

  // Called from Java via JNI when the dialog resolves.
  void OnPickScreen(JNIEnv* env);

  // Called from Java via JNI when the dialog resolves.
  void OnCancel(JNIEnv* env);

 private:
  DesktopMediaPicker::DoneCallback callback_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_MANAGER_BRIDGE_H_
