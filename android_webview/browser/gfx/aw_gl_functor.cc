// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/aw_gl_functor.h"

#include "android_webview/browser_jni_headers/AwGLFunctor_jni.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

extern "C" {
static AwDrawGLFunction DrawGLFunction;
static void DrawGLFunction(long view_context,
                           AwDrawGLInfo* draw_info,
                           void* spare) {
  // |view_context| is the value that was returned from the java
  // AwContents.onPrepareDrawGL; this cast must match the code there.
  reinterpret_cast<android_webview::AwGLFunctor*>(view_context)
      ->DrawGL(draw_info);
}
}

namespace android_webview {

namespace {
int g_instance_count = 0;
}

AwGLFunctor::AwGLFunctor(const JavaObjectWeakGlobalRef& java_ref)
    : java_ref_(java_ref),
      render_thread_manager_(
          base::CreateSingleThreadTaskRunner({BrowserThread::UI})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ++g_instance_count;
}

AwGLFunctor::~AwGLFunctor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  --g_instance_count;
}

bool AwGLFunctor::RequestInvokeGL(bool wait_for_completion) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_AwGLFunctor_requestInvokeGL(env, obj, wait_for_completion);
}

void AwGLFunctor::DetachFunctorFromView() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_AwGLFunctor_detachFunctorFromView(env, obj);
}

void AwGLFunctor::Destroy(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  java_ref_.reset();
  delete this;
}

void AwGLFunctor::DeleteHardwareRenderer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderThreadManager::InsideHardwareReleaseReset release_reset(
      &render_thread_manager_);
  DetachFunctorFromView();

  // Receiving at least one frame is a precondition for
  // initialization (such as looing up GL bindings and constructing
  // hardware_renderer_).
  bool draw_functor_succeeded = RequestInvokeGL(true);
  if (!draw_functor_succeeded) {
    LOG(ERROR) << "Unable to free GL resources. Has the Window leaked?";
    // Calling release on wrong thread intentionally.
    render_thread_manager_.DestroyHardwareRendererOnRT(true /* save_restore */);
  }
}

void AwGLFunctor::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview,toplevel", "DrawFunctor");
  bool save_restore = draw_info->version < 3;
  switch (draw_info->mode) {
    case AwDrawGLInfo::kModeSync:
      TRACE_EVENT_INSTANT0("android_webview", "kModeSync",
                           TRACE_EVENT_SCOPE_THREAD);
      render_thread_manager_.CommitFrameOnRT();
      break;
    case AwDrawGLInfo::kModeProcessNoContext:
      LOG(ERROR) << "Received unexpected kModeProcessNoContext";
      FALLTHROUGH;
    case AwDrawGLInfo::kModeProcess:
      render_thread_manager_.DestroyHardwareRendererOnRT(save_restore);
      break;
    case AwDrawGLInfo::kModeDraw: {
      HardwareRendererDrawParams params{
          draw_info->clip_left,   draw_info->clip_top, draw_info->clip_right,
          draw_info->clip_bottom, draw_info->width,    draw_info->height,
      };
      static_assert(base::size(decltype(draw_info->transform){}) ==
                        base::size(params.transform),
                    "transform size mismatch");
      for (unsigned int i = 0; i < base::size(params.transform); ++i) {
        params.transform[i] = draw_info->transform[i];
      }
      render_thread_manager_.DrawOnRT(save_restore, &params);
      break;
    }
  }
}

void AwGLFunctor::RemoveFromCompositorFrameProducer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_thread_manager_.RemoveFromCompositorFrameProducerOnUI();
}

jlong AwGLFunctor::GetCompositorFrameConsumer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(GetCompositorFrameConsumer());
}

static jint JNI_AwGLFunctor_GetNativeInstanceCount(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_instance_count;
}

static jlong JNI_AwGLFunctor_GetAwDrawGLFunction(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(&DrawGLFunction);
}

static jlong JNI_AwGLFunctor_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(
      new AwGLFunctor(JavaObjectWeakGlobalRef(env, obj)));
}

}  // namespace android_webview
