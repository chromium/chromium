// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"

#include <functional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MediaCaptureDevicesDispatcherAndroid_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

class MediaCaptureDevicesDispatcherObserverAndroid
    : public MediaStreamCaptureIndicator::Observer {
 public:
  static MediaCaptureDevicesDispatcherObserverAndroid* GetInstance() {
    static base::NoDestructor<MediaCaptureDevicesDispatcherObserverAndroid>
        instance;
    return instance.get();
  }

  MediaCaptureDevicesDispatcherObserverAndroid() {
    MediaCaptureDevicesDispatcher::GetInstance()
        ->GetMediaStreamCaptureIndicator()
        ->AddObserver(this);
  }

  MediaCaptureDevicesDispatcherObserverAndroid(
      const MediaCaptureDevicesDispatcherObserverAndroid&) = delete;
  MediaCaptureDevicesDispatcherObserverAndroid& operator=(
      const MediaCaptureDevicesDispatcherObserverAndroid&) = delete;

  ~MediaCaptureDevicesDispatcherObserverAndroid() override = default;

  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingTabChanged(content::WebContents* web_contents,
                               bool is_capturing_tab) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MediaCaptureDevicesDispatcherAndroid_onIsCapturingTabChanged(
        env, web_contents, is_capturing_tab);
  }
};

void EnsureObserverCreated() {
  MediaCaptureDevicesDispatcherObserverAndroid::GetInstance();
}

bool CallIndicator(content::WebContents* web_contents,
                   bool (MediaStreamCaptureIndicator::*predicate)(
                       content::WebContents*) const) {
  EnsureObserverCreated();
  const auto& indicator = MediaCaptureDevicesDispatcher::GetInstance()
                              ->GetMediaStreamCaptureIndicator();
  return std::invoke(predicate, indicator.get(), web_contents);
}

}  // namespace

static bool JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingAudio(
    content::WebContents* web_contents) {
  return CallIndicator(web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingAudio);
}

static bool JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingVideo(
    content::WebContents* web_contents) {
  return CallIndicator(web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingVideo);
}

static bool JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingTab(
    content::WebContents* web_contents) {
  return CallIndicator(web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingTab);
}

static bool JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingWindow(
    content::WebContents* web_contents) {
  return CallIndicator(web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingWindow);
}

static bool JNI_MediaCaptureDevicesDispatcherAndroid_IsCapturingScreen(
    content::WebContents* web_contents) {
  return CallIndicator(web_contents,
                       &MediaStreamCaptureIndicator::IsCapturingDisplay);
}

static void JNI_MediaCaptureDevicesDispatcherAndroid_NotifyStopped(
    content::WebContents* web_contents) {
  EnsureObserverCreated();
  const auto& indicator = MediaCaptureDevicesDispatcher::GetInstance()
                              ->GetMediaStreamCaptureIndicator();
  indicator->StopMediaCapturing(
      web_contents, MediaStreamCaptureIndicator::MediaType::kUserMedia |
                        MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
}

static void JNI_MediaCaptureDevicesDispatcherAndroid_NotifyDisplayMediaStopped(
    content::WebContents* web_contents) {
  EnsureObserverCreated();
  const auto& indicator = MediaCaptureDevicesDispatcher::GetInstance()
                              ->GetMediaStreamCaptureIndicator();
  indicator->StopMediaCapturing(
      web_contents, MediaStreamCaptureIndicator::MediaType::kDisplayMedia);
}

DEFINE_JNI(MediaCaptureDevicesDispatcherAndroid)
