// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"

#include <functional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/web_contents.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MediaCaptureDevicesDispatcherAndroid_jni.h"

using base::android::JavaParamRef;

namespace {

jboolean CallIndicator(const JavaParamRef<jobject>& java_web_contents,
                       bool (MediaStreamCaptureIndicator::*predicate)(
                           content::WebContents*) const) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  const auto& indicator = MediaCaptureDevicesDispatcher::GetInstance()
                              ->GetMediaStreamCaptureIndicator();
  return std::invoke(predicate, indicator.get(), web_contents);
}

}  // namespace

static jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingAudio(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  return CallIndicator(java_web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingAudio);
}

static jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingVideo(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  return CallIndicator(java_web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingVideo);
}

static jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  return CallIndicator(java_web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingTab);
}

static jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  return CallIndicator(java_web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingWindow);
}

static jboolean JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingScreen(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  return CallIndicator(java_web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingDisplay);
}

static void JNI_MediaCaptureDevicesDispatcherAndroid_NotifyStopped(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  const auto& indicator = MediaCaptureDevicesDispatcher::GetInstance()
                              ->GetMediaStreamCaptureIndicator();
  indicator->StopMediaCapturing(
      web_contents, MediaStreamCaptureIndicator::MediaType::kUserMedia |
                        MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
}

DEFINE_JNI(MediaCaptureDevicesDispatcherAndroid)
