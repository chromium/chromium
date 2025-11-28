// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/media_capture_picker_manager_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MediaCapturePickerManagerBridge_jni.h"

using base::android::JavaParamRef;

int MediaCapturePickerManagerBridge::next_fake_id_ = 1;

MediaCapturePickerManagerBridge::MediaCapturePickerManagerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_MediaCapturePickerManagerBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

MediaCapturePickerManagerBridge::~MediaCapturePickerManagerBridge() {
  // We should always have responded to any outstanding callback.
  CHECK(callback_.is_null());
  Java_MediaCapturePickerManagerBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void MediaCapturePickerManagerBridge::Show(
    const DesktopMediaPicker::Params& params,
    DesktopMediaPicker::DoneCallback callback) {
  CHECK(params.web_contents);
  CHECK(callback_.is_null());
  callback_ = std::move(callback);
  web_contents_filter_ = params.includable_web_contents_filter;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaCapturePickerManagerBridge_showDialog(
      env, java_object_, params.web_contents->GetJavaWebContents(),
      params.app_name, params.target_name, params.request_audio,
      params.exclude_system_audio,
      static_cast<int>(params.window_audio_preference),
      static_cast<int>(params.preferred_display_surface),
      params.capture_this_tab, params.exclude_self_browser_surface,
      params.exclude_monitor_type_surfaces);
}

void MediaCapturePickerManagerBridge::OnPickTab(
    JNIEnv* env,
    content::WebContents* web_contents,
    bool audio_share) {
  CHECK(web_contents);
  auto desktop_media_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID(),
          web_contents->GetPrimaryMainFrame()->GetRoutingID()));
  desktop_media_id.audio_share = audio_share;
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerManagerBridge::OnPickWindow(JNIEnv* env) {
  auto desktop_media_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WINDOW, next_fake_id_++);
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerManagerBridge::OnPickScreen(JNIEnv* env) {
  auto desktop_media_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_SCREEN, next_fake_id_++);
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerManagerBridge::OnCancel(JNIEnv* env) {
  std::move(callback_).Run(base::unexpected(
      blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
}

bool MediaCapturePickerManagerBridge::ShouldFilterWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return true;
  }
  return !web_contents_filter_.Run(web_contents);
}

DEFINE_JNI(MediaCapturePickerManagerBridge)
