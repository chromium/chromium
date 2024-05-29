// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process.h"

#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwRenderProcess_jni.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;
using content::ChildProcessTerminationInfo;
using content::RenderProcessHost;

namespace android_webview {

const void* const kAwRenderProcessKey = &kAwRenderProcessKey;

// A user data key to keep track of whether a render view has been created in
// this RPH. This can't be stored in AwRenderProcess since that object may be
// deleted if the OS process dies.
const void* const kAwRenderViewReadyKey = &kAwRenderViewReadyKey;

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
  CHECK(java_obj_);
  if (render_process_host_->IsReady()) {
    Ready();
  }
  GetRendererRemote();
  render_process_host->AddObserver(this);
}

AwRenderProcess::~AwRenderProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Java_AwRenderProcess_setNative(AttachCurrentThread(), java_obj_, 0);
  java_obj_.Reset();
}

void AwRenderProcess::ClearCache() {
  GetRendererRemote()->ClearCache();
}

void AwRenderProcess::SetJsOnlineProperty(bool network_up) {
  GetRendererRemote()->SetJsOnlineProperty(network_up);
}

// static
void AwRenderProcess::SetRenderViewReady(content::RenderProcessHost* host) {
  host->SetUserData(kAwRenderViewReadyKey,
                    std::make_unique<base::SupportsUserData::Data>());
}

// static
bool AwRenderProcess::IsUnused(content::RenderProcessHost* host) {
  return host->IsUnused() && !host->GetUserData(kAwRenderViewReadyKey);
}

void AwRenderProcess::Ready() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Java_AwRenderProcess_setNative(AttachCurrentThread(), java_obj_,
                                 reinterpret_cast<jlong>(this));
}

void AwRenderProcess::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the process was never used, keep the same Java object to satisfy CTS
  // tests.
  if (base::FeatureList::IsEnabled(
          features::kCreateSpareRendererOnBrowserContextCreation) &&
      IsUnused(render_process_host_)) {
    renderer_remote_.reset();
    return;
  }

  render_process_host_->RemoveObserver(this);
  render_process_host_->RemoveUserData(kAwRenderProcessKey);
  // |this| is now deleted.
}

bool AwRenderProcess::TerminateChildProcess(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool result = render_process_host_->Shutdown(0);

  // If the process has never been used, this is the spare render process.
  // Treat this as if it never existed since it's an internal performance
  // optimization.
  if (base::FeatureList::IsEnabled(
          features::kCreateSpareRendererOnBrowserContextCreation) &&
      result && IsUnused(render_process_host_)) {
    // Use fast shutdown for the unused process to allow loadUrl() calls to work
    // immediately after the terminate call.
    render_process_host_->FastShutdownIfPossible();
    return false;
  }

  return result;
}

bool AwRenderProcess::IsProcessLockedToSiteForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return render_process_host_->IsProcessLockedToSiteForTesting();  // IN-TEST
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

mojom::Renderer* AwRenderProcess::GetRendererRemote() {
  if (!renderer_remote_) {
    render_process_host_->GetChannel()->GetRemoteAssociatedInterface(
        &renderer_remote_);
    renderer_remote_.reset_on_disconnect();
  }
  return renderer_remote_.get();
}

}  // namespace android_webview
