// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process.h"
#include "android_webview/browser_jni_headers/AwDebug_jni.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"

using content::RenderProcessHost;

namespace android_webview {

class AwDebugCpuAffinity : public content::RenderProcessHostCreationObserver {
 public:
  AwDebugCpuAffinity() {
    for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      RenderProcessHost* process_host = i.GetCurrentValue();
      if (process_host) {
        SetAffinityForProcessHost(process_host);
      }
    }
  }

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(RenderProcessHost* process_host) override {
    SetAffinityForProcessHost(process_host);
  }

 private:
  void SetAffinityForProcessHost(RenderProcessHost* process_host) {
    process_host->PostTaskWhenProcessIsReady(base::BindOnce(
        &AwRenderProcess::SetCpuAffinityToLittleCores,
        base::Unretained(
            AwRenderProcess::GetInstanceForRenderProcessHost(process_host))));
  }
};

static void JNI_AwDebug_SetCpuAffinityToLittleCores(JNIEnv* env) {
  static base::NoDestructor<AwDebugCpuAffinity> aw_debug_cpu_affinity;
}

static void JNI_AwDebug_SetSupportLibraryWebkitVersionCrashKey(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& version) {
  static ::crash_reporter::CrashKeyString<32> crash_key(
      crash_keys::kSupportLibraryWebkitVersion);
  crash_key.Set(ConvertJavaStringToUTF8(env, version));
}

}  // namespace android_webview
