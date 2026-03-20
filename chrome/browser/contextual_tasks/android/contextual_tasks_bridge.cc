// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"

#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksBridge_jni.h"

namespace contextual_tasks {

static int64_t JNI_ContextualTasksBridge_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& caller,
    int64_t browser_window_ptr,
    Profile* profile) {
  auto* browser_window =
      browser_window_ptr != 0
          ? reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr)
          : nullptr;
  return reinterpret_cast<intptr_t>(
      new ContextualTasksBridge(env, caller, browser_window, profile));
}

ContextualTasksBridge::ContextualTasksBridge(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    BrowserWindowInterface* browser_window,
    Profile* profile)
    : profile_(profile), java_obj_(obj) {
  if (!browser_window) {
    return;
  }
  entry_point_eligibility_manager_ =
      GetUserDataFactory().CreateInstance<EntryPointEligibilityManager>(
          *browser_window, browser_window);
  active_task_context_provider_ =
      GetUserDataFactory().CreateInstance<ActiveTaskContextProviderImpl>(
          *browser_window, browser_window,
          ContextualTasksServiceFactory::GetForProfile(profile));
  controller_ =
      GetUserDataFactory().CreateInstance<ContextualTasksSidePanelCoordinator>(
          *browser_window, browser_window, active_task_context_provider_.get(),
          entry_point_eligibility_manager_.get());
}

ContextualTasksBridge::~ContextualTasksBridge() = default;

void ContextualTasksBridge::Destroy(JNIEnv* env) {
  delete this;
}

// static
ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
ContextualTasksBridge::GetUserDataFactory() {
  static base::NoDestructor<
      ui::UserDataFactoryWithOwner<BrowserWindowInterface>>
      factory;
  return *factory;
}

}  // namespace contextual_tasks

DEFINE_JNI(ContextualTasksBridge)
