// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_surface.h"

#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
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
    const gfx::Vector2dF& physical_fling_velocity) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  // TODO(b/567900824): Respect the page's overscroll-behavior, which
  // SynchronousCompositorHost::DidOverscroll drops, so pages can stop the
  // chaining.
  Java_AwWebSurface_onOverScroll(
      env, java_surface_, physical_overscroll_delta.x(),
      physical_overscroll_delta.y(), physical_fling_velocity.x(),
      physical_fling_velocity.y());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwWebSurface);

}  // namespace android_webview

DEFINE_JNI(AwWebSurface)
