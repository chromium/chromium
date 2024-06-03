// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_H_

#include "android_webview/common/mojom/renderer.mojom.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace android_webview {

// Native handle for the renderer process. The native object owns the Java peer.
//
// Lifetime: Renderer
class AwRenderProcess : public content::RenderProcessHostObserver,
                        public base::SupportsUserData::Data {
 public:
  static AwRenderProcess* GetInstanceForRenderProcessHost(
      content::RenderProcessHost* host);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  bool TerminateChildProcess(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  bool IsProcessLockedToSiteForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  explicit AwRenderProcess(content::RenderProcessHost* render_process_host);

  AwRenderProcess(const AwRenderProcess&) = delete;
  AwRenderProcess& operator=(const AwRenderProcess&) = delete;

  ~AwRenderProcess() override;

  void ClearCache();
  void SetJsOnlineProperty(bool network_up);

  // Notifies that a render view has been created for this process. After this,
  // the process will no longer be considered "unused".
  static void SetRenderViewReady(content::RenderProcessHost* host);

  // Returns whether the RPH is considered "unused", which means a render view
  // has never been created and RPH::Unused() returns true.
  static bool IsUnused(content::RenderProcessHost* host);

 private:
  void Ready();
  void Cleanup();

  // content::RenderProcessHostObserver implementation
  void RenderProcessReady(content::RenderProcessHost* host) override;

  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  mojom::Renderer* GetRendererRemote();

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  raw_ptr<content::RenderProcessHost> render_process_host_;

  mojo::AssociatedRemote<mojom::Renderer> renderer_remote_;

  base::WeakPtrFactory<AwRenderProcess> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_H_
