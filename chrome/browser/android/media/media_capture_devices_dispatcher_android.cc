// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MediaCaptureDevicesDispatcherAndroid_jni.h"

using base::android::JavaParamRef;

jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingAudio(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingAudio(web_contents);
}

jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingVideo(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingVideo(web_contents);
}

jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingScreen(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingWindow(web_contents) ||
         indicator->IsCapturingDisplay(web_contents);
}

void JNI_MediaCaptureDevicesDispatcherAndroid_NotifyStopped(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  indicator->StopMediaCapturing(
      web_contents, MediaStreamCaptureIndicator::MediaType::kUserMedia |
                        MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
}
