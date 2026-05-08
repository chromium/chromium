// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksBridge_jni.h"

namespace contextual_tasks {

DEFINE_USER_DATA(ContextualTasksBridge);

static int64_t JNI_ContextualTasksBridge_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& caller,
    int64_t browser_window_ptr,
    Profile* profile) {
  auto* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(browser_window_ptr);
  CHECK(browser_window);
  return reinterpret_cast<intptr_t>(
      new ContextualTasksBridge(env, caller, browser_window, profile));
}

// static
ContextualTasksBridge* ContextualTasksBridge::From(
    BrowserWindowInterface* window) {
  if (!window) {
    return nullptr;
  }
  return Get(window->GetUnownedUserDataHost());
}

ContextualTasksBridge::ContextualTasksBridge(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    BrowserWindowInterface* browser_window,
    Profile* profile)
    : profile_(profile),
      java_obj_(obj),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
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

void ContextualTasksBridge::UndoClose(JNIEnv* env) {
  if (controller_) {
    controller_->Show();
  }
}

void ContextualTasksBridge::NotifyWebUIReady(
    const base::Uuid& task_id,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_onWebUIReady(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, task_id.AsLowercaseString()),
      web_contents->GetJavaWebContents());
}

void ContextualTasksBridge::NotifyWebUIDestroyed(
    const std::optional<base::Uuid>& task_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_onWebUIDestroyed(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(
          env,
          task_id.has_value() ? task_id->AsLowercaseString() : std::string()));
}

void ContextualTasksBridge::NotifyTaskChanged(
    const std::optional<base::Uuid>& old_task_id,
    const std::optional<base::Uuid>& new_task_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_onTaskChanged(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(
          env, old_task_id.has_value() ? old_task_id->AsLowercaseString()
                                       : std::string()),
      base::android::ConvertUTF8ToJavaString(
          env, new_task_id.has_value() ? new_task_id->AsLowercaseString()
                                       : std::string()));
}

void ContextualTasksBridge::NotifyShowUndoSnackbar() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_showUndoSnackbar(env, java_obj_);
}

void ContextualTasksBridge::NotifyOpenFeedbackUi(const GURL& page_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_openFeedbackUi(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, page_url.spec()));
}

static std::string JNI_ContextualTasksBridge_GetTaskIdForTab(
    JNIEnv* env,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::string();
  }

  // 1. Check the association map (for regular browsing tabs).
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile);
  if (!contextual_tasks_service) {
    return std::string();
  }

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  if (!tab_id.is_valid()) {
    return std::string();
  }

  std::optional<ContextualTask> task =
      contextual_tasks_service->GetContextualTaskForTab(tab_id);
  if (task) {
    return task->GetTaskId().AsLowercaseString();
  }

  // 2. If no task in map, check the URL (for the AIM tab itself).
  const GURL& url = web_contents->GetLastCommittedURL();
  if (contextual_tasks::IsContextualTasksUrl(url)) {
    base::Uuid task_id =
        contextual_tasks::ContextualTasksUiService::GetTaskIdFromUrl(url);
    if (task_id.is_valid()) {
      return task_id.AsLowercaseString();
    }
  }

  return std::string();
}

static bool JNI_ContextualTasksBridge_IsContextualTasksUrl(JNIEnv* env,
                                                           const GURL& url) {
  return url.spec().starts_with(chrome::kChromeUIContextualTasksURL);
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
