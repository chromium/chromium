// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_view_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/actor/core/task_id.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/glic/android/jni_headers/ActorTaskRowData_jni.h"
#include "chrome/browser/glic/android/jni_headers/GlicSplitButtonDelegateBridge_jni.h"

namespace glic {

using jni_zero::AttachCurrentThread;

// C++ implementation of GlicSplitButtonViewDelegate for Android.
// Acts as JNI bridge to forward C++ split button requests to Java's
// GlicSplitButtonDelegateBridge.
// TODO(crbug.com/559190458): Rename to GlicSplitButtonViewDelegateAndroid.
class GlicSplitButtonDelegateAndroid : public GlicSplitButtonViewDelegate {
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

  // GlicSplitButtonViewDelegate:
  void OnTriggerGlicNudgeUI(NudgeParams params) override {
    Java_GlicSplitButtonDelegateBridge_onTriggerGlicNudgeUi(
        AttachCurrentThread(), j_delegate_, params.label,
        params.anchored_message_text, params.prompt_suggestion.value_or(""));
  }

  void OnHideGlicNudgeUI() override {
    Java_GlicSplitButtonDelegateBridge_onHideGlicNudgeUi(AttachCurrentThread(),
                                                         j_delegate_);
  }

  bool GetIsShowingGlicNudge() override {
    return Java_GlicSplitButtonDelegateBridge_getIsShowingGlicNudge(
        AttachCurrentThread(), j_delegate_);
  }

  void ShowGlicActorTaskIcon() override {
    Java_GlicSplitButtonDelegateBridge_showGlicActorTaskIcon(
        AttachCurrentThread(), j_delegate_);
  }

  void HideGlicActorTaskIcon() override {
    Java_GlicSplitButtonDelegateBridge_hideGlicActorTaskIcon(
        AttachCurrentThread(), j_delegate_);
  }

  bool GetIsShowingGlicActorTaskIconNudge() override {
    return Java_GlicSplitButtonDelegateBridge_getIsShowingGlicActorTaskIconNudge(
        AttachCurrentThread(), j_delegate_);
  }

  void SetGlicActorNudgeLabel(const std::u16string& nudge_label) override {
    Java_GlicSplitButtonDelegateBridge_setGlicActorNudgeLabel(
        AttachCurrentThread(), j_delegate_, nudge_label);
  }

  void TriggerGlicActorNudge(const std::u16string& nudge_label) override {
    Java_GlicSplitButtonDelegateBridge_triggerGlicActorNudge(
        AttachCurrentThread(), j_delegate_, nudge_label);
  }

  void SetGlicActorNudgePressedState(bool pressed) override {
    Java_GlicSplitButtonDelegateBridge_setGlicActorNudgePressedState(
        AttachCurrentThread(), j_delegate_, pressed);
  }

  void ShowActorTaskListBubble() override {
    Profile* profile = browser_->GetProfile();
    auto* manager =
        glic::GlicActorTaskIconManagerFactory::GetForProfile(profile);
    if (!manager) {
      return;
    }
    std::vector<actor::ui::ActorTaskRowData> rows =
        ActorTaskListBubbleController::GetActorTaskRowsForBubble(
            profile, manager->actor_task_list_bubble_rows());

    JNIEnv* env = AttachCurrentThread();
    std::vector<jni_zero::ScopedJavaLocalRef<jobject>> j_rows;
    j_rows.reserve(rows.size());
    for (const auto& row : rows) {
      j_rows.push_back(Java_ActorTaskRowData_Constructor(
          env, row.task_id.value(), row.title, row.subtitle, row.is_enabled,
          row.needs_review, row.tab_id));
    }
    Java_GlicSplitButtonDelegateBridge_showActorTaskListBubble(env, j_delegate_,
                                                               j_rows);
  }

  void CloseActorTaskListBubble() override {
    Java_GlicSplitButtonDelegateBridge_closeActorTaskListBubble(
        AttachCurrentThread(), j_delegate_);
  }

  bool IsActorTaskListBubbleShowing() override {
    return Java_GlicSplitButtonDelegateBridge_isActorTaskListBubbleShowing(
        AttachCurrentThread(), j_delegate_);
  }

  void SetGlicShowState(bool show) override {
    Java_GlicSplitButtonDelegateBridge_setGlicShowState(AttachCurrentThread(),
                                                        j_delegate_, show);
  }

  void SetGlicPanelIsOpen(bool open) override {
    Java_GlicSplitButtonDelegateBridge_setGlicPanelIsOpen(AttachCurrentThread(),
                                                          j_delegate_, open);
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

  void OnActorTaskListBubbleDismissed() {
    if (browser_) {
      if (auto* bubble_controller =
              ActorTaskListBubbleController::From(browser_)) {
        bubble_controller->OnBubbleDestroyed();
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
