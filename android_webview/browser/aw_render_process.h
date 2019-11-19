// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RENDER_PROCESS_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"

#include "content/public/browser/render_process_host_observer.h"

namespace android_webview {

class AwRenderProcess : public content::RenderProcessHostObserver,
                        public base::SupportsUserData::Data {
 public:
  static AwRenderProcess* GetInstanceForRenderProcessHost(
      content::RenderProcessHost* host);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  bool TerminateChildProcess(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  explicit AwRenderProcess(content::RenderProcessHost* render_process_host);
  ~AwRenderProcess() override;

 private:
  void Ready();
  void Cleanup();

  // content::RenderProcessHostObserver implementation
  void RenderProcessReady(content::RenderProcessHost* host) override;

  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  content::RenderProcessHost* render_process_host_;

  base::WeakPtrFactory<AwRenderProcess> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AwRenderProcess);
};

}  // namespace android_webview

#endif
