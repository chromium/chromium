// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"

#include <jni.h>

#include <algorithm>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback_forward.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/grouped_affiliations/jni_headers/AcknowledgeGroupedCredentialSheetBridge_jni.h"

namespace {

using JniDelegate = AcknowledgeGroupedCredentialSheetBridge::JniDelegate;

class JniDelegateImpl : public JniDelegate {
 public:
  JniDelegateImpl() = default;
  JniDelegateImpl(const JniDelegateImpl&) = delete;
  JniDelegateImpl& operator=(const JniDelegateImpl&) = delete;
  ~JniDelegateImpl() override = default;

  void Create(const gfx::NativeWindow window_android,
              AcknowledgeGroupedCredentialSheetBridge* bridge) override {
    java_bridge_.Reset(Java_AcknowledgeGroupedCredentialSheetBridge_Constructor(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(bridge), window_android->GetJavaObject()));
  }

  void Show(std::string current_origin,
            std::string credential_origin) override {
    Java_AcknowledgeGroupedCredentialSheetBridge_show(
        base::android::AttachCurrentThread(), java_bridge_, current_origin,
        credential_origin);
  }

  void Dismiss() override {
    Java_AcknowledgeGroupedCredentialSheetBridge_dismiss(
        base::android::AttachCurrentThread(), java_bridge_);
  }

 private:
  // The corresponding Java GroupedCredentialAcknowledgeSheetBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};
}  // namespace

AcknowledgeGroupedCredentialSheetBridge::JniDelegate::JniDelegate() = default;
AcknowledgeGroupedCredentialSheetBridge::JniDelegate::~JniDelegate() = default;

AcknowledgeGroupedCredentialSheetBridge::
    AcknowledgeGroupedCredentialSheetBridge()
    : jni_delegate_(std::make_unique<JniDelegateImpl>()) {}

AcknowledgeGroupedCredentialSheetBridge::
    AcknowledgeGroupedCredentialSheetBridge(
        base::PassKey<
            class AcknowledgeGroupedCredentialSheetControllerTestHelper>,
        std::unique_ptr<JniDelegate> jni_delegate)
    : jni_delegate_(std::move(jni_delegate)) {}

AcknowledgeGroupedCredentialSheetBridge::
    AcknowledgeGroupedCredentialSheetBridge(
        base::PassKey<class AcknowledgeGroupedCredentialSheetControllerTest>,
        std::unique_ptr<JniDelegate> jni_delegate)
    : jni_delegate_(std::move(jni_delegate)) {}

AcknowledgeGroupedCredentialSheetBridge::
    ~AcknowledgeGroupedCredentialSheetBridge() {
  // If `Show` was never called, just return.
  if (!closure_callback_) {
    return;
  }
  jni_delegate_->Dismiss();
}

void AcknowledgeGroupedCredentialSheetBridge::Show(
    std::string current_origin,
    std::string credential_origin,
    gfx::NativeWindow window,
    base::OnceCallback<void(bool)> closure_callback) {
  if (!window) {
    return;
  }
  closure_callback_ = std::move(closure_callback);
  jni_delegate_->Create(window, this);
  jni_delegate_->Show(std::move(current_origin), std::move(credential_origin));
}

void AcknowledgeGroupedCredentialSheetBridge::OnDismissed(JNIEnv* env,
                                                          bool accepted) {
  std::move(closure_callback_).Run(accepted);
}
