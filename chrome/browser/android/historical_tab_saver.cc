// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/historical_tab_saver.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/HistoricalTabSaverImpl_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context_wrapper.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace historical_tab_saver {

namespace {

// Defined in TabGroupModelFilter.java
constexpr int kInvalidGroupId = -1;

void CreateHistoricalTab(
    TabAndroid* tab_android,
    WebContentsStateByteBuffer web_contents_state_byte_buffer) {
  if (!tab_android) {
    return;
  }

  auto scoped_web_contents = ScopedWebContents::CreateForTab(
      tab_android, &web_contents_state_byte_buffer);
  if (!scoped_web_contents->web_contents()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(Profile::FromBrowserContext(
          scoped_web_contents->web_contents()->GetBrowserContext()));
  if (!service) {
    return;
  }

  // Index is unimportant on Android.
  service->CreateHistoricalTab(sessions::ContentLiveTab::GetForWebContents(
                                   scoped_web_contents->web_contents()),
                               /*index=*/-1);
}

void CreateHistoricalGroup(
    TabModel* model,
    const std::u16string& group_title,
    std::vector<TabAndroid*> tabs,
    std::vector<WebContentsStateByteBuffer> web_contents_state) {
  DCHECK(model);
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(model->GetProfile());
  if (!service) {
    return;
  }

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  std::map<int, tab_groups::TabGroupId> tab_id_to_group_id;
  for (const auto* tab : tabs) {
    DCHECK(tab);
    tab_id_to_group_id.insert(std::make_pair(tab->GetAndroidId(), group_id));
  }

  AndroidLiveTabContextCloseWrapper context(
      model, std::move(tabs), std::move(tab_id_to_group_id),
      std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>(
          {{group_id,
            tab_groups::TabGroupVisualData(group_title, /*color_int=*/0)}}),
      std::move(web_contents_state));

  service->CreateHistoricalGroup(&context, group_id);
  service->GroupClosed(group_id);
}

void CreateHistoricalBulkClosure(
    TabModel* model,
    std::vector<int> android_group_ids,
    std::vector<std::u16string> group_titles,
    std::vector<int> per_tab_android_group_id,
    std::vector<TabAndroid*> tabs,
    std::vector<WebContentsStateByteBuffer> web_contents_state) {
  DCHECK(model);
  DCHECK_EQ(android_group_ids.size(), group_titles.size());
  DCHECK_EQ(per_tab_android_group_id.size(), tabs.size());

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(model->GetProfile());
  if (!service) {
    return;
  }

  // Map each Android Group IDs to a stand-in tab_group::TabGroupId for storage
  // in TabRestoreService.
  std::map<int, tab_groups::TabGroupId> group_id_mapping;

  // Map each stand-in tab_group::TabGroupId to a TabGroupVisualData containing
  // the title of the group from Android.
  std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>
      native_groups;

  for (size_t i = 0; i < android_group_ids.size(); ++i) {
    auto new_id = tab_groups::TabGroupId::GenerateNew();
    // Avoid collision - highly unlikely for 128 bit int.
    while (native_groups.count(new_id)) {
      new_id = tab_groups::TabGroupId::GenerateNew();
    }

    int android_group_id = android_group_ids[i];
    group_id_mapping.insert({android_group_id, new_id});

    const std::u16string title = group_titles[i];
    native_groups[new_id] =
        tab_groups::TabGroupVisualData(title, /*color_int=*/0);
  }

  // Map Android Tabs by ID to their new native tab_group::TabGroupId.
  std::map<int, tab_groups::TabGroupId> tab_id_to_group_id;
  for (size_t i = 0; i < tabs.size(); ++i) {
    TabAndroid* tab = tabs[i];
    if (per_tab_android_group_id[i] != kInvalidGroupId) {
      int android_group_id = per_tab_android_group_id[i];
      auto it = group_id_mapping.find(android_group_id);
      DCHECK(it != group_id_mapping.end());
      tab_id_to_group_id.insert(
          std::make_pair(tab->GetAndroidId(), it->second));
    }
  }

  AndroidLiveTabContextCloseWrapper context(
      model, std::move(tabs), std::move(tab_id_to_group_id),
      std::move(native_groups), std::move(web_contents_state));
  service->BrowserClosing(&context);
  service->BrowserClosed(&context);
}

}  // namespace

ScopedWebContents::ScopedWebContents(content::WebContents* unowned_web_contents)
    : unowned_web_contents_(unowned_web_contents),
      owned_web_contents_(nullptr) {}
ScopedWebContents::ScopedWebContents(
    std::unique_ptr<content::WebContents> owned_web_contents)
    : unowned_web_contents_(nullptr),
      owned_web_contents_(std::move(owned_web_contents)) {
  if (owned_web_contents_) {
    owned_web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  }
}

ScopedWebContents::~ScopedWebContents() = default;

content::WebContents* ScopedWebContents::web_contents() const {
  if (!unowned_web_contents_) {
    return owned_web_contents_.get();
  } else {
    return unowned_web_contents_;
  }
}

// static
std::unique_ptr<ScopedWebContents> ScopedWebContents::CreateForTab(
    TabAndroid* tab,
    const WebContentsStateByteBuffer* web_contents_state) {
  if (tab->web_contents()) {
    return std::make_unique<ScopedWebContents>(tab->web_contents());
  }
  if (web_contents_state->state_version != -1) {
    auto native_contents = WebContentsState::RestoreContentsFromByteBuffer(
        web_contents_state, /*initially_hidden=*/true, /*no_renderer=*/true);
    if (native_contents) {
      return std::make_unique<ScopedWebContents>(std::move(native_contents));
    }
  }
  // Fallback to an empty web contents in the event state restoration
  // fails. This will just not be added to the TabRestoreService.
  // This is only called on non-incognito pathways.
  CHECK(!tab->IsIncognito());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  content::WebContents::CreateParams params(profile);
  params.initially_hidden = true;
  params.desired_renderer_state =
      content::WebContents::CreateParams::kNoRendererProcess;
  return std::make_unique<ScopedWebContents>(
      content::WebContents::Create(params));
}

// static
static std::vector<WebContentsStateByteBuffer>
AllTabsWebContentsStateByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jbyte_buffers,
    const JavaParamRef<jintArray>& jsaved_state_versions) {
  std::vector<int> saved_state_versions;
  base::android::JavaIntArrayToIntVector(env, jsaved_state_versions,
                                         &saved_state_versions);
  int jbyte_buffers_count = env->GetArrayLength(jbyte_buffers);
  std::vector<WebContentsStateByteBuffer> web_contents_states;
  web_contents_states.reserve(jbyte_buffers_count);

  for (int i = 0; i < jbyte_buffers_count; ++i) {
    web_contents_states.emplace_back(
        ScopedJavaLocalRef<jobject>(
            env, env->GetObjectArrayElement(jbyte_buffers, i)),
        saved_state_versions[i]);
  }
  return web_contents_states;
}

// Static JNI methods.

// static
static void JNI_HistoricalTabSaverImpl_CreateHistoricalTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_android,
    const JavaParamRef<jobject>& state,
    jint saved_state_version) {
  WebContentsStateByteBuffer web_contents_state = WebContentsStateByteBuffer(
      ScopedJavaLocalRef<jobject>(state), (int)saved_state_version);
  CreateHistoricalTab(TabAndroid::GetNativeTab(env, jtab_android),
                      std::move(web_contents_state));
}

// static
static void JNI_HistoricalTabSaverImpl_CreateHistoricalGroup(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    const JavaParamRef<jstring>& jtitle,
    const JavaParamRef<jobjectArray>& jtabs_android,
    const JavaParamRef<jobjectArray>& jbyte_buffers,
    const JavaParamRef<jintArray>& jsaved_state_versions) {
  std::u16string title = base::android::ConvertJavaStringToUTF16(env, jtitle);
  auto tabs_android = TabAndroid::GetAllNativeTabs(
      env, base::android::ScopedJavaLocalRef(jtabs_android));
  int tabs_android_count = env->GetArrayLength(jtabs_android);
  DCHECK_EQ(tabs_android_count, env->GetArrayLength(jbyte_buffers));
  DCHECK_EQ(tabs_android_count, env->GetArrayLength(jsaved_state_versions));

  std::vector<WebContentsStateByteBuffer> web_contents_states =
      AllTabsWebContentsStateByteBuffer(env, jbyte_buffers,
                                        jsaved_state_versions);
  CreateHistoricalGroup(TabModelList::FindNativeTabModelForJavaObject(
                            ScopedJavaLocalRef<jobject>(env, jtab_model.obj())),
                        title, std::move(tabs_android),
                        std::move(web_contents_states));
}

// static
static void JNI_HistoricalTabSaverImpl_CreateHistoricalBulkClosure(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    const JavaParamRef<jintArray>& jandroid_group_ids,
    const JavaParamRef<jobjectArray>& jgroup_titles,
    const JavaParamRef<jintArray>& jper_tab_android_group_id,
    const JavaParamRef<jobjectArray>& jtabs_android,
    const JavaParamRef<jobjectArray>& jbyte_buffers,
    const JavaParamRef<jintArray>& jsaved_state_versions) {
  std::vector<int> android_group_ids;
  base::android::JavaIntArrayToIntVector(env, jandroid_group_ids,
                                         &android_group_ids);
  std::vector<std::u16string> group_titles;
  base::android::AppendJavaStringArrayToStringVector(env, jgroup_titles,
                                                     &group_titles);
  std::vector<int> per_tab_android_group_id;
  base::android::JavaIntArrayToIntVector(env, jper_tab_android_group_id,
                                         &per_tab_android_group_id);
  int tabs_android_count = env->GetArrayLength(jtabs_android);
  DCHECK_EQ(tabs_android_count, env->GetArrayLength(jbyte_buffers));
  DCHECK_EQ(tabs_android_count, env->GetArrayLength(jsaved_state_versions));

  std::vector<WebContentsStateByteBuffer> web_contents_states =
      AllTabsWebContentsStateByteBuffer(env, jbyte_buffers,
                                        jsaved_state_versions);
  CreateHistoricalBulkClosure(
      TabModelList::FindNativeTabModelForJavaObject(
          ScopedJavaLocalRef<jobject>(env, jtab_model.obj())),
      std::move(android_group_ids), std::move(group_titles),
      std::move(per_tab_android_group_id),
      TabAndroid::GetAllNativeTabs(
          env, base::android::ScopedJavaLocalRef(jtabs_android)),
      std::move(web_contents_states));
}

}  // namespace historical_tab_saver
