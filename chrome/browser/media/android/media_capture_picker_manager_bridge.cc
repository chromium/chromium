// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/media_capture_picker_manager_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MediaCapturePickerManagerBridge_jni.h"

using base::android::JavaParamRef;

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
    content::WebContents* web_contents,
    const std::u16string& app_name,
    bool request_audio,
    DesktopMediaPicker::DoneCallback callback) {
  CHECK(web_contents);
  CHECK(callback_.is_null());
  callback_ = std::move(callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaCapturePickerManagerBridge_showDialog(
      env, java_object_, web_contents->GetJavaWebContents(), app_name,
      request_audio);
}

void MediaCapturePickerManagerBridge::OnPickTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents,
    bool audio_share) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
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
      content::DesktopMediaID::TYPE_WINDOW, content::DesktopMediaID::kNullId);
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerManagerBridge::OnPickScreen(JNIEnv* env) {
  auto desktop_media_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_SCREEN, content::DesktopMediaID::kNullId);
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerManagerBridge::OnCancel(JNIEnv* env) {
  std::move(callback_).Run(base::unexpected(
      blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
}
