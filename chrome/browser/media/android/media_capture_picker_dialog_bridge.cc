// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/media_capture_picker_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/MediaCapturePickerDialogBridge_jni.h"

using base::android::JavaParamRef;

MediaCapturePickerDialogBridge::MediaCapturePickerDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_MediaCapturePickerDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

MediaCapturePickerDialogBridge::~MediaCapturePickerDialogBridge() {
  // We should always have responded to any outstanding callback.
  CHECK(callback_.is_null());
  Java_MediaCapturePickerDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void MediaCapturePickerDialogBridge::Show(
    content::WebContents* web_contents,
    const std::u16string& app_name,
    bool request_audio,
    MediaCapturePickerDialogCallback callback) {
  CHECK(web_contents);
  CHECK(callback_.is_null());
  callback_ = std::move(callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::WindowAndroid* window_android = web_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), content::DesktopMediaID()));
    return;
  }

  Java_MediaCapturePickerDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(), app_name,
      request_audio);
}

void MediaCapturePickerDialogBridge::OnPickTab(
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

void MediaCapturePickerDialogBridge::OnPickWindow(JNIEnv* env) {
  auto desktop_media_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WINDOW, content::DesktopMediaID::kNullId);
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerDialogBridge::OnPickScreen(JNIEnv* env) {
  auto desktop_media_id = content::DesktopMediaID(
      content::DesktopMediaID::TYPE_SCREEN, content::DesktopMediaID::kNullId);
  std::move(callback_).Run(desktop_media_id);
}

void MediaCapturePickerDialogBridge::OnCancel(JNIEnv* env) {
  std::move(callback_).Run({});
}
