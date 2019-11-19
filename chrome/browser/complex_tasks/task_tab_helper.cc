// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/complex_tasks/task_tab_helper.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"

#if defined(OS_ANDROID)
#include "chrome/android/chrome_jni_headers/TaskTabHelper_jni.h"
#include "chrome/browser/android/tab_android.h"

using base::android::JavaParamRef;
#endif  // defined(OS_ANDROID)

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
      last_pruned_navigation_entry_index_(-1) {}

TaskTabHelper::~TaskTabHelper() {}

TaskTabHelper::HubType TaskTabHelper::GetSpokeEntryHubType() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();

  DCHECK(entry);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  if (url_service && url_service->IsSearchResultsPageFromDefaultSearchProvider(
                         entry->GetURL())) {
    return HubType::DEFAULT_SEARCH_ENGINE;
  } else if (ui::PageTransitionCoreTypeIs(
                 entry->GetTransitionType(),
                 ui::PageTransition::PAGE_TRANSITION_FORM_SUBMIT)) {
    return HubType::FORM_SUBMIT;
  } else {
    return HubType::OTHER;
  }
}

void TaskTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  int current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();

  if (current_entry_index > last_pruned_navigation_entry_index_)
    entry_index_to_spoke_count_map_[current_entry_index] = 1;

  UpdateAndRecordTaskIds(load_details);
}

void TaskTabHelper::NavigationListPruned(
    const content::PrunedDetails& pruned_details) {
  int current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();

  if (entry_index_to_spoke_count_map_.count(current_entry_index) == 0)
    entry_index_to_spoke_count_map_[current_entry_index] = 1;

  entry_index_to_spoke_count_map_[current_entry_index]++;
  last_pruned_navigation_entry_index_ = current_entry_index;

  RecordHubAndSpokeNavigationUsage(
      entry_index_to_spoke_count_map_[current_entry_index]);
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
#if defined(OS_ANDROID)
      else {
        // Cross-tab navigation - only supported in Android. In this case
        // the parent and parent root Task IDs are passed from the Java layer
        if (this->GetParentTaskId() != -1) {
          navigation_task_id->set_parent_id(this->GetParentTaskId());
          navigation_task_id->set_root_id(this->GetParentRootTaskId());
        }
      }
#endif  // defined(OS_ANDROID)
    }
  } else {
    navigation_task_id->set_root_id(navigation_task_id->id());
  }
  local_navigation_task_id_map_.emplace(load_details.entry->GetUniqueID(),
                                        *navigation_task_id);
}

void TaskTabHelper::RecordHubAndSpokeNavigationUsage(int spokes) {
  DCHECK_GT(spokes, 1);

  std::string histogram_name;
  switch (GetSpokeEntryHubType()) {
    case HubType::DEFAULT_SEARCH_ENGINE: {
      histogram_name =
          "Tabs.Tasks.HubAndSpokeNavigationUsage.FromDefaultSearchEngine";
      break;
    }
    case HubType::FORM_SUBMIT: {
      histogram_name = "Tabs.Tasks.HubAndSpokeNavigationUsage.FromFormSubmit";
      break;
    }
    case HubType::OTHER: {
      histogram_name = "Tabs.Tasks.HubAndSpokeNavigationUsage.FromOther";
      break;
    }
    default: {
      NOTREACHED() << "Unknown HubType";
    }
  }

  base::UmaHistogramExactLinear(histogram_name, spokes, 100);
}

#if defined(OS_ANDROID)
int64_t TaskTabHelper::GetParentTaskId() {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  return Java_TaskTabHelper_getParentTaskId(
      base::android::AttachCurrentThread(), tab_android->GetJavaObject());
}

int64_t TaskTabHelper::GetParentRootTaskId() {
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents());
  return Java_TaskTabHelper_getParentRootTaskId(
      base::android::AttachCurrentThread(), tab_android->GetJavaObject());
}

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
#endif  // defined(OS_ANDROID)

WEB_CONTENTS_USER_DATA_KEY_IMPL(TaskTabHelper)

}  // namespace tasks
