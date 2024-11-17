// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_context_menu_helper.h"

#include "android_webview/browser_jni_headers/AwContextMenuHelper_jni.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "content/public/browser/render_process_host.h"
#include "ui/android/view_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android_webview {

AwContextMenuHelper::AwContextMenuHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<AwContextMenuHelper>(*web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_AwContextMenuHelper_create(
                           env, web_contents->GetJavaWebContents())
                           .obj());
  DCHECK(!java_obj_.is_null());
}

AwContextMenuHelper::~AwContextMenuHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwContextMenuHelper_destroy(env, java_obj_);
}

void AwContextMenuHelper::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::NativeView view = GetWebContents().GetNativeView();
  Java_AwContextMenuHelper_showContextMenu(
      env, java_obj_,
      context_menu::BuildJavaContextMenuParams(
          params, render_frame_host.GetProcess()->GetID(),
          render_frame_host.GetFrameToken().value()),
      view->GetContainerView());
}

void AwContextMenuHelper::DismissContextMenu() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwContextMenuHelper_dismissContextMenu(env, java_obj_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwContextMenuHelper);

}  // namespace android_webview
