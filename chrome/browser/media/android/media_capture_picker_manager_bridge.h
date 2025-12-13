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
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "content/public/browser/desktop_media_id.h"

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
  void Show(const DesktopMediaPicker::Params& params,
            DesktopMediaPicker::DoneCallback callback);

  // Called from Java via JNI when the dialog resolves.
  void OnPickTab(JNIEnv* env,
                 content::WebContents* web_contents,
                 bool audio_share);

  // Called from Java via JNI when the dialog resolves.
  void OnPickWindow(JNIEnv* env);

  // Called from Java via JNI when the dialog resolves.
  void OnPickScreen(JNIEnv* env);

  // Called from Java via JNI when the dialog resolves.
  void OnCancel(JNIEnv* env);

  // Called from Java via JNI to check if a tab should be filtered.
  bool ShouldFilterWebContents(JNIEnv* env, content::WebContents* web_contents);

 private:
  DesktopMediaPicker::DoneCallback callback_;
  DesktopMediaList::WebContentsFilter web_contents_filter_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // The number of windows/ screens media source have been selected. This is
  // used to generate an unique media id.
  static int next_fake_id_;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_MEDIA_CAPTURE_PICKER_MANAGER_BRIDGE_H_
