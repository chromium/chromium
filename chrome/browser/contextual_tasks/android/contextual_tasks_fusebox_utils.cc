// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>

#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksFuseboxManagerImpl_jni.h"

namespace contextual_tasks {

static std::string JNI_ContextualTasksFuseboxManagerImpl_GetTaskIdForTab(
    JNIEnv* env,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::string();
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile);
  if (!ui_service) {
    return std::string();
  }

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
  if (!task) {
    return std::string();
  }

  return task->GetTaskId().AsLowercaseString();
}

static bool JNI_ContextualTasksFuseboxManagerImpl_IsContextualTasksUrl(
    JNIEnv* env,
    const GURL& url) {
  return url.spec().starts_with(chrome::kChromeUIContextualTasksURL);
}

}  // namespace contextual_tasks

DEFINE_JNI(ContextualTasksFuseboxManagerImpl)
