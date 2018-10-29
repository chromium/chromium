// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_gl_functor.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AwGLFunctor_jni.h"

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
  reinterpret_cast<android_webview::RenderThreadManager*>(view_context)
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
          this,
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})) {
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

void AwGLFunctor::Destroy(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  java_ref_.reset();
  delete this;
}

void AwGLFunctor::DeleteHardwareRenderer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_thread_manager_.DeleteHardwareRendererOnUI();
}

jlong AwGLFunctor::GetAwDrawGLViewContext(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(&render_thread_manager_);
}

static jint JNI_AwGLFunctor_GetNativeInstanceCount(
    JNIEnv* env,
    const JavaParamRef<jclass>&) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_instance_count;
}

static jlong JNI_AwGLFunctor_GetAwDrawGLFunction(JNIEnv* env,
                                                 const JavaParamRef<jclass>&) {
  return reinterpret_cast<intptr_t>(&DrawGLFunction);
}

static jlong JNI_AwGLFunctor_Create(
    JNIEnv* env,
    const JavaParamRef<jclass>&,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(
      new AwGLFunctor(JavaObjectWeakGlobalRef(env, obj)));
}

}  // namespace android_webview
