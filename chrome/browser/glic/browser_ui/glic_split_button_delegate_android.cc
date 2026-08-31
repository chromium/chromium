// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/android/jni_headers/GlicSplitButtonDelegateBridge_jni.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/actor/core/task_id.h"

namespace glic {

// C++ implementation of GlicSplitButtonDelegate for Android.
// Acts as JNI bridge to forward C++ split button requests to Java's
// GlicSplitButtonDelegateBridge.
class GlicSplitButtonDelegateAndroid : public GlicSplitButtonDelegate {
 public:
  GlicSplitButtonDelegateAndroid(
      BrowserWindowInterface* browser,
      base::WeakPtr<GlicNudgeController> controller,
      const base::android::ScopedJavaGlobalRef<JGlicSplitButtonDelegateBridge>&
          j_delegate)
      : browser_(browser), controller_(controller), j_delegate_(j_delegate) {
    CHECK(controller_);
    controller_->SetHorizontalTabsDelegate(this);
  }

  GlicSplitButtonDelegateAndroid(const GlicSplitButtonDelegateAndroid&) =
      delete;
  GlicSplitButtonDelegateAndroid& operator=(
      const GlicSplitButtonDelegateAndroid&) = delete;
  ~GlicSplitButtonDelegateAndroid() override {
    if (controller_) {
      controller_->SetHorizontalTabsDelegate(nullptr);
    }
  }

  // GlicSplitButtonDelegate:
  void OnTriggerGlicNudgeUI(NudgeParams params) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicSplitButtonDelegateBridge_onTriggerGlicNudgeUi(
        env, j_delegate_,
        base::android::ConvertUTF8ToJavaString(env, params.label),
        base::android::ConvertUTF8ToJavaString(env,
                                               params.anchored_message_text),
        base::android::ConvertUTF8ToJavaString(
            env, params.prompt_suggestion.value_or("")));
  }

  void OnHideGlicNudgeUI() override {
    Java_GlicSplitButtonDelegateBridge_onHideGlicNudgeUi(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  bool GetIsShowingGlicNudge() override {
    return Java_GlicSplitButtonDelegateBridge_getIsShowingGlicNudge(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  void ShowGlicActorTaskIcon() override {
    Java_GlicSplitButtonDelegateBridge_showGlicActorTaskIcon(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  void HideGlicActorTaskIcon() override {
    Java_GlicSplitButtonDelegateBridge_hideGlicActorTaskIcon(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  bool GetIsShowingGlicActorTaskIconNudge() override {
    return Java_GlicSplitButtonDelegateBridge_getIsShowingGlicActorTaskIconNudge(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  void SetGlicActorNudgeLabel(const std::u16string& nudge_label) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicSplitButtonDelegateBridge_setGlicActorNudgeLabel(
        env, j_delegate_,
        base::android::ConvertUTF16ToJavaString(env, nudge_label));
  }

  void TriggerGlicActorNudge(const std::u16string& nudge_text) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicSplitButtonDelegateBridge_triggerGlicActorNudge(
        env, j_delegate_,
        base::android::ConvertUTF16ToJavaString(env, nudge_text));
  }

  void SetGlicActorNudgePressedState(bool pressed) override {
    Java_GlicSplitButtonDelegateBridge_setGlicActorNudgePressedState(
        base::android::AttachCurrentThread(), j_delegate_, pressed);
  }

  void ShowActorTaskListBubble() override {
    Java_GlicSplitButtonDelegateBridge_showActorTaskListBubble(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  void CloseActorTaskListBubble() override {
    Java_GlicSplitButtonDelegateBridge_closeActorTaskListBubble(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  bool IsActorTaskListBubbleShowing() override {
    return Java_GlicSplitButtonDelegateBridge_isActorTaskListBubbleShowing(
        base::android::AttachCurrentThread(), j_delegate_);
  }

  void SetGlicShowState(bool show) override {
    Java_GlicSplitButtonDelegateBridge_setGlicShowState(
        base::android::AttachCurrentThread(), j_delegate_, show);
  }

  void SetGlicPanelIsOpen(bool open) override {
    Java_GlicSplitButtonDelegateBridge_setGlicPanelIsOpen(
        base::android::AttachCurrentThread(), j_delegate_, open);
  }

  // Methods invoked from Java GlicSplitButtonDelegateBridge via JNI:
  void OnNudgeActivity(GlicNudgeActivity activity) {
    if (controller_) {
      controller_->OnNudgeActivity(activity);
    }
  }

  void OnTaskRowClicked(int task_id) {
    if (browser_) {
      if (auto* bubble_controller =
              ActorTaskListBubbleController::From(browser_)) {
        bubble_controller->OnTaskRowClicked(actor::TaskId(task_id));
      }
    }
  }

  void OnGlicActorButtonClicked() {
    if (browser_) {
      if (auto* bubble_controller =
              ActorTaskListBubbleController::From(browser_)) {
        bubble_controller->ShowBubble();
      }
    }
  }

  void Destroy() { delete this; }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
  base::WeakPtr<GlicNudgeController> controller_;
  base::android::ScopedJavaGlobalRef<JGlicSplitButtonDelegateBridge>
      j_delegate_;
};

static int64_t JNI_GlicSplitButtonDelegateBridge_Create(
    JNIEnv* env,
    int64_t j_native_browser_window_interface,
    const base::android::JavaRef<JGlicSplitButtonDelegateBridge>& j_delegate) {
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
  return reinterpret_cast<int64_t>(new GlicSplitButtonDelegateAndroid(
      browser, glic_nudge_controller->GetWeakPtr(),
      base::android::ScopedJavaGlobalRef<JGlicSplitButtonDelegateBridge>(
          j_delegate)));
}

DEFINE_JNI(GlicSplitButtonDelegateBridge)

}  // namespace glic
