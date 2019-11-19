// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/vr/service/arcore_consent_prompt_interface.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

class VR_EXPORT ArCoreConsentPrompt : public ArCoreConsentPromptInterface {
 public:
  ArCoreConsentPrompt();
  ~ArCoreConsentPrompt();

  // ArCoreConsentPromptInterface:
  void ShowConsentPrompt(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> response_callback) override;

  // Called from Java end.
  void OnUserConsentResult(JNIEnv* env,
                           jboolean is_granted);
  void OnRequestInstallArModuleResult(
      JNIEnv* env,
      bool success);
  void OnRequestInstallSupportedArCoreResult(
      JNIEnv* env,
      bool success);

 private:
  // Returns true if AR module installation is supported, false otherwise.
  bool CanRequestInstallArModule();
  // Returns true if AR module is not installed, false otherwise.
  bool ShouldRequestInstallArModule();
  void RequestInstallArModule();
  bool ShouldRequestInstallSupportedArCore();
  void RequestInstallSupportedArCore();

  void RequestArModule();
  void OnRequestArModuleResult(bool success);
  void RequestArCoreInstallOrUpdate();
  void OnRequestArCoreInstallOrUpdateResult(bool success);

  void CallDeferredUserConsentCallback(bool is_permission_granted);

  base::WeakPtr<ArCoreConsentPrompt> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::OnceCallback<void(bool)> on_user_consent_callback_;

  base::OnceCallback<void(bool)> on_request_ar_module_result_callback_;
  base::OnceCallback<void(bool)>
      on_request_arcore_install_or_update_result_callback_;

  base::android::ScopedJavaGlobalRef<jobject> jdelegate_;
  int render_process_id_;
  int render_frame_id_;

  base::android::ScopedJavaGlobalRef<jobject> java_install_utils_;
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ArCoreConsentPrompt> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreConsentPrompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_CONSENT_PROMPT_H_
