// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/complex_tasks/task_tab_helper.h"

#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/android/chrome_jni_headers/TaskTabHelper_jni.h"
#include "chrome/browser/android/tab_android.h"

using base::android::JavaParamRef;
#endif  // BUILDFLAG(IS_ANDROID)

namespace {
bool DoesTransitionContinueTask(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_SUBFRAME) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_FORM_SUBMIT) ||
         transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
}
}  // namespace

namespace tasks {

TaskTabHelper::TaskTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TaskTabHelper>(*web_contents) {}

TaskTabHelper::~TaskTabHelper() {}

void TaskTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  UpdateAndRecordTaskIds(load_details);
}

sessions::NavigationTaskId* TaskTabHelper::GetCurrentTaskId(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  content::NavigationEntry* navigation_entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!navigation_entry)
    return nullptr;
  sessions::NavigationTaskId* navigation_task_id =
      sessions::NavigationTaskId::Get(navigation_entry);
  if (!navigation_task_id)
    return nullptr;
  return navigation_task_id;
}

void TaskTabHelper::UpdateAndRecordTaskIds(
    const content::LoadCommittedDetails& load_details) {
  sessions::NavigationTaskId* navigation_task_id =
      sessions::NavigationTaskId::Get(load_details.entry);

  // The Task ID is the Global ID of the first navigation. The first
  // navigation is detected if the Task ID hasn't been set yet.
  if (navigation_task_id->id() == -1) {
    navigation_task_id->set_id(
        load_details.entry->GetTimestamp().since_origin().InMicroseconds());
  }

  if (DoesTransitionContinueTask(load_details.entry->GetTransitionType())) {
    if (load_details.previous_entry_index != -1) {
      content::NavigationEntry* prev_nav_entry =
          web_contents()->GetController().GetEntryAtIndex(
              load_details.previous_entry_index);

      if (prev_nav_entry != nullptr) {
        sessions::NavigationTaskId* prev_navigation_task_id =
            sessions::NavigationTaskId::Get(prev_nav_entry);
        navigation_task_id->set_parent_id(prev_navigation_task_id->id());
        navigation_task_id->set_root_id(prev_navigation_task_id->root_id());
      }
    } else if (GetParentTaskId() != -1) {
      // If the previous_entry_index is -1, this is a new tab. Use the parent
      // task-id for the case of cross tab navigations (such as window.open).
      navigation_task_id->set_parent_id(GetParentTaskId());
      navigation_task_id->set_root_id(GetParentRootTaskId());
    }
  } else {
    navigation_task_id->set_root_id(navigation_task_id->id());
  }
  local_navigation_task_id_map_.emplace(load_details.entry->GetUniqueID(),
                                        *navigation_task_id);
}

int64_t TaskTabHelper::GetParentTaskId() {
#if BUILDFLAG(IS_ANDROID)
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  return tab_android && Java_TaskTabHelper_getParentTaskId(
                            base::android::AttachCurrentThread(),
                            tab_android->GetJavaObject());
#else
  return -1;
#endif
}

int64_t TaskTabHelper::GetParentRootTaskId() {
#if BUILDFLAG(IS_ANDROID)
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  return tab_android && Java_TaskTabHelper_getParentRootTaskId(
                            base::android::AttachCurrentThread(),
                            tab_android->GetJavaObject());
#else
  return -1;
#endif
}

#if BUILDFLAG(IS_ANDROID)
jlong JNI_TaskTabHelper_GetTaskId(JNIEnv* env,
                                  const JavaParamRef<jobject>& jweb_contents) {
  sessions::NavigationTaskId* navigation_task_id =
      TaskTabHelper::GetCurrentTaskId(
          content::WebContents::FromJavaWebContents(jweb_contents));
  if (navigation_task_id) {
    return navigation_task_id->id();
  }
  return -1;
}

jlong JNI_TaskTabHelper_GetRootTaskId(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  sessions::NavigationTaskId* navigation_task_id =
      TaskTabHelper::GetCurrentTaskId(
          content::WebContents::FromJavaWebContents(jweb_contents));
  if (navigation_task_id) {
    return navigation_task_id->root_id();
  }
  return -1;
}
#endif  // BUILDFLAG(IS_ANDROID)

WEB_CONTENTS_USER_DATA_KEY_IMPL(TaskTabHelper);

}  // namespace tasks
