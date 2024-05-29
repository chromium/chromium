// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/add_username_dialog/add_username_dialog_bridge.h"

#include <jni.h>

#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/add_username_dialog/jni_headers/AddUsernameDialogBridge_jni.h"

namespace {

using JniDelegate = AddUsernameDialogBridge::JniDelegate;

class JniDelegateImpl : public JniDelegate {
 public:
  JniDelegateImpl() = default;
  JniDelegateImpl(const JniDelegateImpl&) = delete;
  JniDelegateImpl& operator=(const JniDelegateImpl&) = delete;
  ~JniDelegateImpl() override = default;

  void Create(const gfx::NativeWindow window_android,
              AddUsernameDialogBridge* bridge) override {
    if (!window_android) {
      return;
    }

    java_bridge_.Reset(Java_AddUsernameDialogBridge_Constructor(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(bridge), window_android->GetJavaObject()));
  }

  void ShowAddUsernameDialog(const std::u16string& password) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AddUsernameDialogBridge_showAddUsernameDialog(
        base::android::AttachCurrentThread(), java_bridge_,
        base::android::ConvertUTF16ToJavaString(env, password));
  }

  void Dismiss() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AddUsernameDialogBridge_dismiss(env, java_bridge_);
  }

 private:
  // The corresponding Java AddUsernameDialogBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace

AddUsernameDialogBridge::JniDelegate::JniDelegate() = default;
AddUsernameDialogBridge::JniDelegate::~JniDelegate() = default;

AddUsernameDialogBridge::AddUsernameDialogBridge()
    : jni_delegate_(std::make_unique<JniDelegateImpl>()) {}
AddUsernameDialogBridge::~AddUsernameDialogBridge() {
  if (!dialog_dismissed_callback_) {
    return;
  }
  jni_delegate_->Dismiss();
}

AddUsernameDialogBridge::AddUsernameDialogBridge(
    base::PassKey<class GeneratedPasswordSavedMessageDelegateTest>,
    std::unique_ptr<JniDelegate> jni_delegate)
    : jni_delegate_(std::move(jni_delegate)) {}

void AddUsernameDialogBridge::ShowAddUsernameDialog(
    const gfx::NativeWindow window_android,
    const std::u16string& password,
    DialogAcceptedCallback dialog_accepted_callback,
    base::OnceClosure dialog_dismissed_callback) {
  dialog_accepted_callback_ = std::move(dialog_accepted_callback);
  dialog_dismissed_callback_ = std::move(dialog_dismissed_callback);

  jni_delegate_->Create(window_android, this);
  jni_delegate_->ShowAddUsernameDialog(password);
}

void AddUsernameDialogBridge::OnDialogAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& username) {
  std::move(dialog_accepted_callback_)
      .Run(base::android::ConvertJavaStringToUTF16(env, username));
}

void AddUsernameDialogBridge::OnDialogDismissed(JNIEnv* env) {
  std::move(dialog_dismissed_callback_).Run();
}
