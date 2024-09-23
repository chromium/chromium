// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/password_manager/no_passkeys/android/no_passkeys_bottom_sheet_bridge.h"

#include "base/android/jni_string.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/touch_to_fill/password_manager/no_passkeys/internal/android/jni/NoPasskeysBottomSheetBridge_jni.h"

namespace {

using JniDelegate = NoPasskeysBottomSheetBridge::JniDelegate;

class JniDelegateImpl : public JniDelegate {
 public:
  explicit JniDelegateImpl(NoPasskeysBottomSheetBridge* bridge)
      : bridge_(bridge) {}
  JniDelegateImpl(const JniDelegateImpl&) = delete;
  JniDelegateImpl& operator=(const JniDelegateImpl&) = delete;
  ~JniDelegateImpl() override = default;

  void Create(ui::WindowAndroid* window_android) override {
    java_object_.Reset(Java_NoPasskeysBottomSheetBridge_Constructor(
        jni_zero::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(bridge_.get()),
        window_android->GetJavaObject()));
  }

  void Show(const std::string& origin) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_NoPasskeysBottomSheetBridge_show(
        env, java_object_, base::android::ConvertUTF8ToJavaString(env, origin));
  }

  void Dismiss() override {
    Java_NoPasskeysBottomSheetBridge_dismiss(jni_zero::AttachCurrentThread(),
                                             java_object_);
  }

 private:
  // The corresponding Java NoPasskeysBottomSheetBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  // The owning native NoPasskeysBottomSheetBridge.
  raw_ptr<NoPasskeysBottomSheetBridge> bridge_;
};

}  // namespace

NoPasskeysBottomSheetBridge::JniDelegate::JniDelegate() = default;
NoPasskeysBottomSheetBridge::JniDelegate::~JniDelegate() = default;

NoPasskeysBottomSheetBridge::NoPasskeysBottomSheetBridge()
    : jni_delegate_(std::make_unique<JniDelegateImpl>(this)) {}

NoPasskeysBottomSheetBridge::NoPasskeysBottomSheetBridge(
    base::PassKey<class NoPasskeysBottomSheetBridgeTest>,
    std::unique_ptr<JniDelegate> jni_delegate)
    : jni_delegate_(std::move(jni_delegate)) {}

NoPasskeysBottomSheetBridge::NoPasskeysBottomSheetBridge(
    base::PassKey<class TouchToFillControllerWebAuthnTest>,
    std::unique_ptr<JniDelegate> jni_delegate)
    : jni_delegate_(std::move(jni_delegate)) {}

NoPasskeysBottomSheetBridge::~NoPasskeysBottomSheetBridge() {
  Dismiss();
}

void NoPasskeysBottomSheetBridge::Show(
    ui::WindowAndroid* window_android,
    const std::string& origin,
    base::OnceClosure on_dismissed_callback,
    base::OnceClosure on_click_use_another_device_callback) {
  CHECK(window_android) << "The bridge needs a window to attach to!";
  CHECK(on_dismissed_callback) << "The bridge needs a clean up callback!";
  CHECK(!on_dismissed_callback_)
      << "Show was already called. Use each bridge only once.";

  on_dismissed_callback_ = std::move(on_dismissed_callback);
  on_click_use_another_device_callback_ =
      std::move(on_click_use_another_device_callback);
  jni_delegate_->Create(window_android);
  jni_delegate_->Show(origin);
}

void NoPasskeysBottomSheetBridge::Dismiss() {
  if (on_dismissed_callback_) {  // Not dismissed yet.
    jni_delegate_->Dismiss();
  }
}

void NoPasskeysBottomSheetBridge::OnDismissed(JNIEnv* env) {
  CHECK(on_dismissed_callback_);
  std::move(on_dismissed_callback_).Run();
}

void NoPasskeysBottomSheetBridge::OnClickUseAnotherDevice(JNIEnv* env) {
  CHECK(on_click_use_another_device_callback_);
  std::move(on_click_use_another_device_callback_).Run();
}
