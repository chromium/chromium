// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/android/offscreen_rendering_manager_android.h"

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/actor/android/jni_headers/OffscreenRenderingManager_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace actor {

OffscreenRenderingManagerAndroid::OffscreenRenderingManagerAndroid(
    ui::WindowAndroid* window,
    int width,
    int height) {
  CHECK(window);
  offscreen_compositor_.reset(
      content::Compositor::CreateOffscreen(this, window));

  // Initialize with the provided size. If multiple tabs with different sizes
  // are used, they will each resize themselves, but the compositor's window
  // bounds only need to be large enough to encompass them if we were doing
  // readback from the root layer (which we aren't).
  offscreen_compositor_->SetWindowBounds(gfx::Size(width, height));

  root_layer_ = cc::slim::Layer::Create();
  offscreen_compositor_->SetRootLayer(root_layer_);
}

OffscreenRenderingManagerAndroid::~OffscreenRenderingManagerAndroid() {
  offscreen_compositor_->SetRootLayer(nullptr);
  offscreen_compositor_.reset();
  root_layer_ = nullptr;
}

void OffscreenRenderingManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void OffscreenRenderingManagerAndroid::StartOffscreenRendering(
    JNIEnv* env,
    content::WebContents* web_contents,
    int width,
    int height) {
  CHECK(web_contents);

  ui::ViewAndroid* view = web_contents->GetNativeView();
  CHECK(view && view->GetLayer());

  gfx::Size size(width, height);
  web_contents->Resize(gfx::Rect(size));
  view->OnPhysicalBackingSizeChanged(size);

  root_layer_->AddChild(view->GetLayer());
  offscreen_compositor_->SetNeedsComposite();
}

void OffscreenRenderingManagerAndroid::StopOffscreenRendering(
    JNIEnv* env,
    content::WebContents* web_contents) {
  CHECK(web_contents);

  ui::ViewAndroid* view = web_contents->GetNativeView();
  CHECK(view && view->GetLayer());
  if (view->GetLayer()->parent() == root_layer_) {
    view->GetLayer()->RemoveFromParent();
  }
}

}  // namespace actor

// JNI Bridge Functions
static int64_t JNI_OffscreenRenderingManager_Init(JNIEnv* env,
                                                  ui::WindowAndroid* window,
                                                  int32_t width,
                                                  int32_t height) {
  return reinterpret_cast<intptr_t>(
      new actor::OffscreenRenderingManagerAndroid(window, width, height));
}

DEFINE_JNI(OffscreenRenderingManager)
