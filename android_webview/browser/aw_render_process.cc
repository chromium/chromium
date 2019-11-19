// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

#include "android_webview/browser_jni_headers/AwRenderProcess_jni.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;
using content::ChildProcessTerminationInfo;
using content::RenderProcessHost;

namespace android_webview {

const void* const kAwRenderProcessKey = &kAwRenderProcessKey;

// static
AwRenderProcess* AwRenderProcess::GetInstanceForRenderProcessHost(
    RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AwRenderProcess* render_process =
      static_cast<AwRenderProcess*>(host->GetUserData(kAwRenderProcessKey));
  if (!render_process) {
    std::unique_ptr<AwRenderProcess> created_render_process =
        std::make_unique<AwRenderProcess>(host);
    render_process = created_render_process.get();
    host->SetUserData(kAwRenderProcessKey, std::move(created_render_process));
  }
  return render_process;
}

AwRenderProcess::AwRenderProcess(RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  java_obj_.Reset(Java_AwRenderProcess_create(AttachCurrentThread()));
  CHECK(!java_obj_.is_null());
  if (render_process_host_->IsReady()) {
    Ready();
  }
  render_process_host->AddObserver(this);
}

AwRenderProcess::~AwRenderProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Java_AwRenderProcess_setNative(AttachCurrentThread(), java_obj_, 0);
  java_obj_.Reset();
}

void AwRenderProcess::Ready() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Java_AwRenderProcess_setNative(AttachCurrentThread(), java_obj_,
                                 reinterpret_cast<jlong>(this));
}

void AwRenderProcess::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  render_process_host_->RemoveObserver(this);
  render_process_host_->RemoveUserData(kAwRenderProcessKey);
  // |this| is now deleted.
}

bool AwRenderProcess::TerminateChildProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return render_process_host_->Shutdown(0);
}

base::android::ScopedJavaLocalRef<jobject> AwRenderProcess::GetJavaObject() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void AwRenderProcess::RenderProcessReady(RenderProcessHost* host) {
  DCHECK(host == render_process_host_);

  Ready();
}

void AwRenderProcess::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK(host == render_process_host_);

  Cleanup();
}

}  // namespace android_webview
