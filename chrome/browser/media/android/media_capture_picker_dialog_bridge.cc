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
      env, java_object_, window_android->GetJavaObject(),
      base::android::ConvertUTF16ToJavaString(env, app_name));
}

void MediaCapturePickerDialogBridge::OnResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::DesktopMediaID desktop_media_id;

  // If no web contents was selected to capture, return a null DesktopMediaID.
  // TODO(crbug.com/352186941): Implement for window and screen sharing.
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (web_contents) {
    desktop_media_id = content::DesktopMediaID(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(
            web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
            web_contents->GetPrimaryMainFrame()->GetRoutingID()));
  }
  std::move(callback_).Run(desktop_media_id);
}
