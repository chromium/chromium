// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_
#define ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/task/cancelable_task_tracker.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"

namespace android_webview {

class JsSandboxIsolate {
 public:
  JsSandboxIsolate();
  ~JsSandboxIsolate();

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

  void TerminateAndDestroy();
  void DestroyWhenPossible();
  void NotifyInitComplete();
  void CreateCancelableTaskTracker();
  void PostEvaluationToIsolateThread(const std::string code,
                                     FinishedCallback success_callback,
                                     FinishedCallback error_callback);

  // Used as a control sequence to add ordering to binder threadpool requests.
  scoped_refptr<base::SequencedTaskRunner> control_task_runner_;
  // Should be used from control_task_runner_.
  bool isolate_init_complete = false;
  // Should be used from control_task_runner_.
  bool destroy_called_before_init = false;
  // Should be used from control_task_runner_.
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  // Used for interaction with the isolate.
  scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner_;
  // Should be used from isolate_task_runner_.
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  // Should be used from isolate_task_runner_.
  std::unique_ptr<gin::ContextHolder> context_holder_;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_JS_SANDBOX_SERVICE_JS_SANDBOX_ISOLATE_H_
