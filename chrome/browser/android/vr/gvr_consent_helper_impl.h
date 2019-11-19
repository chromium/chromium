// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_CONSENT_HELPER_IMPL_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_CONSENT_HELPER_IMPL_H_

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/vr/vr_module_provider.h"
#include "chrome/browser/vr/service/gvr_consent_helper.h"

namespace vr {
class GvrConsentHelperImpl : public GvrConsentHelper {
 public:
  GvrConsentHelperImpl();

  ~GvrConsentHelperImpl() override;

  // Caller must ensure not to call this a second time before the first dialog
  // is dismissed.
  void PromptUserAndGetConsent(int render_process_id,
                               int render_frame_id,
                               XrConsentPromptLevel consent_level,
                               OnUserConsentCallback) override;
  void OnUserConsentResult(JNIEnv* env,
                           jboolean is_granted);

 private:
  void OnModuleInstalled(bool success);

  std::unique_ptr<VrModuleProvider> module_delegate_;
  int render_process_id_;
  int render_frame_id_;
  XrConsentPromptLevel consent_level_;

  OnUserConsentCallback on_user_consent_callback_;
  base::android::ScopedJavaGlobalRef<jobject> jdelegate_;

  base::WeakPtrFactory<GvrConsentHelperImpl> weak_ptr_{this};

  DISALLOW_COPY_AND_ASSIGN(GvrConsentHelperImpl);
};
}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_CONSENT_HELPER_IMPL_H_
