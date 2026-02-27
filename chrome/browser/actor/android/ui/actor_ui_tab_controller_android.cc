// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/android/ui/actor_ui_tab_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_deref.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/android/jni_headers/ActorUiTabController_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace actor::ui {

DEFINE_USER_DATA(ActorUiTabControllerAndroid);

ActorUiTabControllerAndroid::ActorUiTabControllerAndroid(
    tabs::TabInterface& tab,
    ActorKeyedService* actor_keyed_service)
    : ActorUiTabControllerInterface(tab),
      tab_(tab),
      actor_keyed_service_(CHECK_DEREF(actor_keyed_service)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

ActorUiTabControllerAndroid::~ActorUiTabControllerAndroid() = default;

void ActorUiTabControllerAndroid::OnUiTabStateChange(
    const UiTabState& ui_tab_state,
    UiResultCallback callback) {
  if (current_ui_tab_state_ == ui_tab_state) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), true));
    return;
  }
  VLOG(4) << "Tab scoped UI components updated FROM -> TO:\n"
          << "ui_tab_state: " << current_ui_tab_state_ << " -> " << ui_tab_state
          << "\n";

  current_ui_tab_state_ = ui_tab_state;

  DCHECK(tab_->GetContents());
  TabAndroid* tab_android = TabAndroid::FromWebContents(tab_->GetContents());
  bool success = false;
  if (tab_android) {
    JNIEnv* env = base::android::AttachCurrentThread();
    success = Java_ActorUiTabController_onUiTabStateChange(
        env, tab_android->GetJavaObject(), ui_tab_state.actor_overlay.is_active,
        ui_tab_state.actor_overlay.border_glow_visible,
        ui_tab_state.actor_overlay.mouse_down,
        ui_tab_state.handoff_button.is_active,
        static_cast<int>(ui_tab_state.handoff_button.controller),
        static_cast<int>(ui_tab_state.tab_indicator),
        ui_tab_state.border_glow_visible);
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), success));
}

void ActorUiTabControllerAndroid::SetActorTaskPaused() {
  if (auto* task = actor_keyed_service_->GetTaskFromTab(*tab_)) {
    // This call originates from the Android UI, so it represents an
    // intentional user pause task. Therefore, `from_actor` is false.
    task->Pause(/*from_actor=*/false);
  }
}

void ActorUiTabControllerAndroid::SetActorTaskResume() {
  if (auto* task = actor_keyed_service_->GetTaskFromTab(*tab_)) {
    task->Resume();
  }
}

base::WeakPtr<ActorUiTabControllerInterface>
ActorUiTabControllerAndroid::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

UiTabState ActorUiTabControllerAndroid::GetCurrentUiTabState() const {
  return current_ui_tab_state_;
}

static void JNI_ActorUiTabController_SetActorTaskPaused(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jtab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  if (tab_android && tab_android->web_contents()) {
    auto* controller = ActorUiTabControllerAndroid::From(tab_android);
    if (controller) {
      controller->SetActorTaskPaused();
    }
  }
}

static void JNI_ActorUiTabController_SetActorTaskResume(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jtab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  if (tab_android && tab_android->web_contents()) {
    auto* controller = ActorUiTabControllerAndroid::From(tab_android);
    if (controller) {
      controller->SetActorTaskResume();
    }
  }
}

}  // namespace actor::ui

DEFINE_JNI(ActorUiTabController)
