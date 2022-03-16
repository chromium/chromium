// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_JS_SANDBOX_JS_SANDBOX_CONTEXT_H_
#define ANDROID_WEBVIEW_RENDERER_JS_SANDBOX_JS_SANDBOX_CONTEXT_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"

namespace {
class SandboxRunnerDelegate;
}

namespace android_webview {

class JsSandboxContext {
 public:
  JsSandboxContext();
  ~JsSandboxContext();

  using FinishedCallback = base::OnceCallback<void(const std::string&)>;

  void DestroyNative(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  jboolean EvaluateJavascript(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jcode,
      const base::android::JavaParamRef<jobject>& j_success_callback,
      const base::android::JavaParamRef<jobject>& j_failure_callback);

 private:
  void DeleteSelf();
  void InitializeIsolateOnThread();
  void EvaluateJavascriptOnThread(const std::string code,
                                  FinishedCallback success_callback,
                                  FinishedCallback failure_callback);

  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<SandboxRunnerDelegate> delegate_;
  std::unique_ptr<gin::ShellRunner> runner_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_JS_SANDBOX_JS_SANDBOX_CONTEXT_H_
