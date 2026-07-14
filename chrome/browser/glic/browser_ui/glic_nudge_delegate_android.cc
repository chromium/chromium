// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/glic/android/jni_headers/GlicNudgeDelegateBridge_jni.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace glic {

// C++ implementation of GlicNudgeDelegate for Android.
// Acts as JNI bridge to forward C++ nudge trigger/hide calls to Java's
// GlicNudgeDelegateBridge.
class GlicNudgeDelegateAndroid : public GlicSplitButtonDelegate {
 public:
  GlicNudgeDelegateAndroid(
      base::WeakPtr<GlicNudgeController> controller,
      const base::android::ScopedJavaGlobalRef<JGlicNudgeDelegateBridge>&
          j_delegate)
      : controller_(controller), j_delegate_(j_delegate) {
    CHECK(controller_);
    controller_->SetHorizontalTabsDelegate(this);
  }

  GlicNudgeDelegateAndroid(const GlicNudgeDelegateAndroid&) = delete;
  GlicNudgeDelegateAndroid& operator=(const GlicNudgeDelegateAndroid&) = delete;
  ~GlicNudgeDelegateAndroid() override {
    if (controller_) {
      controller_->SetHorizontalTabsDelegate(nullptr);
    }
  }

  // GlicSplitButtonDelegate:
  void OnTriggerGlicNudgeUI(NudgeParams params) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicNudgeDelegateBridge_onTriggerGlicNudgeUi(
        env, j_delegate_,
        base::android::ConvertUTF8ToJavaString(env, params.label),
        base::android::ConvertUTF8ToJavaString(env,
                                               params.anchored_message_text),
        base::android::ConvertUTF8ToJavaString(
            env, params.prompt_suggestion.value_or("")));
  }

  void OnHideGlicNudgeUI() override {
    Java_GlicNudgeDelegateBridge_onHideGlicNudgeUi(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  bool GetIsShowingGlicNudge() override {
    return Java_GlicNudgeDelegateBridge_getIsShowingGlicNudge(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  // Java GlicNudgeDelegateBridge native methods:
  void OnNudgeActivity(GlicNudgeActivity activity) {
    if (controller_) {
      controller_->OnNudgeActivity(activity);
    }
  }

  void Destroy() { delete this; }

 private:
  base::WeakPtr<GlicNudgeController> controller_;
  base::android::ScopedJavaGlobalRef<JGlicNudgeDelegateBridge> j_delegate_;
};

static int64_t JNI_GlicNudgeDelegateBridge_Create(
    JNIEnv* env,
    int64_t j_native_browser_window_interface,
    const base::android::JavaRef<JGlicNudgeDelegateBridge>& j_delegate) {
  BrowserWindowInterface* browser = reinterpret_cast<BrowserWindowInterface*>(
      j_native_browser_window_interface);
  if (!browser) {
    return 0l;
  }
  GlicNudgeController* glic_nudge_controller =
      GlicNudgeController::From(browser);
  if (!glic_nudge_controller) {
    // TODO(crbug.com/484037810): CHECK instead.
    return 0l;
  }
  return reinterpret_cast<int64_t>(new GlicNudgeDelegateAndroid(
      glic_nudge_controller->GetWeakPtr(),
      base::android::ScopedJavaGlobalRef<JGlicNudgeDelegateBridge>(
          j_delegate)));
}

DEFINE_JNI(GlicNudgeDelegateBridge)

}  // namespace glic
