// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_surface.h"

#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "cc/input/overscroll_behavior.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwWebSurface_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using content::BrowserThread;

namespace android_webview {

static void JNI_AwWebSurface_SetForWebContents(
    JNIEnv* env,
    content::WebContents* web_contents,
    const JavaRef<jobject>& jsurface) {
  web_contents->RemoveUserData(AwWebSurface::UserDataKey());
  if (jsurface) {
    AwWebSurface::CreateForWebContents(web_contents, jsurface);
  }
}

AwWebSurface::AwWebSurface(content::WebContents* web_contents,
                           const JavaRef<jobject>& obj)
    : content::WebContentsUserData<AwWebSurface>(*web_contents),
      java_surface_(obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserViewRenderer* bvr = BrowserViewRenderer::FromWebContents(web_contents);
  CHECK(bvr);
  bvr->SetOverscrollCallback(base::BindRepeating(
      &AwWebSurface::DidOverscroll, weak_ptr_factory_.GetWeakPtr()));
}

AwWebSurface::~AwWebSurface() = default;

void AwWebSurface::DidOverscroll(
    const gfx::Vector2dF& physical_overscroll_delta,
    const gfx::Vector2dF& physical_fling_velocity,
    const cc::OverscrollBehavior& overscroll_behavior) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Respect CSS overscroll-behavior so pages can stop overscroll from chaining
  // out of WebSurface (unlike WebView's local View overscroll path).
  float dx = overscroll_behavior.PropagatesXScroll()
                 ? physical_overscroll_delta.x()
                 : 0.f;
  float dy = overscroll_behavior.PropagatesYScroll()
                 ? physical_overscroll_delta.y()
                 : 0.f;
  if (dx == 0.f && dy == 0.f) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_AwWebSurface_onOverScroll(env, java_surface_, dx, dy,
                                 dx != 0.f ? physical_fling_velocity.x() : 0.f,
                                 dy != 0.f ? physical_fling_velocity.y() : 0.f);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwWebSurface);

}  // namespace android_webview

DEFINE_JNI(AwWebSurface)
