// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/historical_tab_saver.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/uuid.h"
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

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/HistoricalTabSaverImpl_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace historical_tab_saver {

namespace {

// Defined in TabGroupModelFilter.java
constexpr int kInvalidRootId = -1;

std::vector<WebContentsStateByteBuffer> AllTabsWebContentsStateByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jbyte_buffers,
    std::vector<int> saved_state_versions) {
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

std::optional<tab_groups::TabGroupId> JavaTokenToTabGroupId(
    JNIEnv* env,
    const JavaRef<jobject>& jtab_group_id) {
  if (jtab_group_id.is_null()) {
    return std::nullopt;
  }
  return tab_groups::TabGroupId::FromRawToken(
      base::android::TokenAndroid::FromJavaToken(env, jtab_group_id));
}

std::vector<std::optional<tab_groups::TabGroupId>> JavaTokensToTabGroupIds(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jtab_group_ids) {
  std::vector<std::optional<tab_groups::TabGroupId>> tab_group_ids;
  size_t array_length = env->GetArrayLength(jtab_group_ids);
  tab_group_ids.reserve(array_length);
  for (size_t i = 0; i < array_length; ++i) {
    auto jtab_group_id = env->GetObjectArrayElement(jtab_group_ids, i);
    std::optional<tab_groups::TabGroupId> tab_group_id = JavaTokenToTabGroupId(
        env, ScopedJavaLocalRef<jobject>(env, jtab_group_id));
    tab_group_ids.push_back(tab_group_id);
  }
  return tab_group_ids;
}

std::optional<base::Uuid> StringToUuid(
    const std::u16string& serialized_saved_tab_group_id) {
  if (serialized_saved_tab_group_id.empty()) {
    return std::nullopt;
  }
  return base::Uuid::ParseLowercase(serialized_saved_tab_group_id);
}

std::vector<std::optional<base::Uuid>> StringsToUuids(
    const std::vector<std::u16string>& serialized_saved_tab_group_ids) {
  std::vector<std::optional<base::Uuid>> saved_tab_group_ids;
  saved_tab_group_ids.reserve(serialized_saved_tab_group_ids.size());
  for (const auto& serialized_saved_tab_group_id :
       serialized_saved_tab_group_ids) {
    saved_tab_group_ids.push_back(StringToUuid(serialized_saved_tab_group_id));
  }
  return saved_tab_group_ids;
}

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

  // TODO(crbug/41496693): We should update AndroidLiveTabContext to return
  // group data for single tabs when not closing an entire group to align with
  // desktop. Right now any individual tab closure is treated as not being in a
  // group.
  // Index is unimportant on Android.
  service->CreateHistoricalTab(sessions::ContentLiveTab::GetForWebContents(
                                   scoped_web_contents->web_contents()),
                               /*index=*/-1);
}

void CreateHistoricalGroup(
    TabModel* model,
    const std::optional<tab_groups::TabGroupId>& optional_tab_group_id,
    const std::optional<base::Uuid> saved_tab_group_id,
    const std::u16string& group_title,
    int group_color,
    std::vector<raw_ptr<TabAndroid, VectorExperimental>> tabs,
    std::vector<WebContentsStateByteBuffer> web_contents_state) {
  DCHECK(model);
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(model->GetProfile());
  if (!service) {
    return;
  }

  tab_groups::TabGroupId group_id = optional_tab_group_id
                                        ? *optional_tab_group_id
                                        : tab_groups::TabGroupId::GenerateNew();
  std::map<int, tab_groups::TabGroupId> tab_id_to_group_id;
  for (const TabAndroid* tab : tabs) {
    DCHECK(tab);
    tab_id_to_group_id.insert(std::make_pair(tab->GetAndroidId(), group_id));
  }

  // TODO(crbug/41496693): If we update AndroidLiveTabContext to return group
  // data for tabs it should be possible to eliminate the need for this wrapper
  // when closing an entire tab group.
  AndroidLiveTabContextCloseWrapper context(
      model, std::move(tabs), std::move(tab_id_to_group_id),
      std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>(
          {{group_id, tab_groups::TabGroupVisualData(
                          group_title, /*color_int=*/(
                              tab_groups::TabGroupColorId)group_color)}}),
      std::map<tab_groups::TabGroupId, std::optional<base::Uuid>>(
          {{group_id, saved_tab_group_id}}),
      std::move(web_contents_state));

  service->CreateHistoricalGroup(&context, group_id);
  service->GroupClosed(group_id);
}

void CreateHistoricalBulkClosure(
    TabModel* model,
    std::vector<int> root_ids,
    std::vector<std::optional<tab_groups::TabGroupId>> optional_tab_group_ids,
    std::vector<std::optional<base::Uuid>> saved_tab_group_ids,
    std::vector<std::u16string> group_titles,
    std::vector<int> group_colors,
    std::vector<int> per_tab_root_id,
    std::vector<raw_ptr<TabAndroid, VectorExperimental>> tabs,
    std::vector<WebContentsStateByteBuffer> web_contents_state) {
  DCHECK(model);
  DCHECK_EQ(root_ids.size(), group_titles.size());
  DCHECK_EQ(root_ids.size(), group_colors.size());
  DCHECK_EQ(root_ids.size(), optional_tab_group_ids.size());
  DCHECK_EQ(root_ids.size(), saved_tab_group_ids.size());
  DCHECK_EQ(per_tab_root_id.size(), tabs.size());

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(model->GetProfile());
  if (!service) {
    return;
  }

  // Map each Android Group IDs to a stand-in tab_group::TabGroupId for storage
  // in TabRestoreService.
  std::map<int, tab_groups::TabGroupId> group_id_mapping;

  // Map each tab_group::TabGroupId to corresponding data for consumption
  // downstream.
  std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>
      tab_group_visual_data;
  std::map<tab_groups::TabGroupId, std::optional<base::Uuid>>
      saved_tab_group_ids_map;

  for (size_t i = 0; i < root_ids.size(); ++i) {
    auto group_id = tab_groups::TabGroupId::CreateEmpty();
    auto optional_tab_group_id = optional_tab_group_ids[i];
    if (optional_tab_group_id) {
      group_id = *optional_tab_group_id;
    } else {
      group_id = tab_groups::TabGroupId::GenerateNew();
      // Avoid collision - highly unlikely for 128 bit int.
      while (tab_group_visual_data.count(group_id)) {
        group_id = tab_groups::TabGroupId::GenerateNew();
      }
    }

    int root_id = root_ids[i];
    group_id_mapping.insert({root_id, group_id});

    auto saved_tab_group_id = saved_tab_group_ids[i];
    if (saved_tab_group_id) {
      saved_tab_group_ids_map.insert({group_id, saved_tab_group_id});
    }

    const std::u16string title = group_titles[i];
    int color = group_colors[i];
    tab_group_visual_data[group_id] = tab_groups::TabGroupVisualData(
        title, /*color=*/(tab_groups::TabGroupColorId)color);
  }

  // Map Android Tabs by ID to their new or existing native
  // tab_group::TabGroupId.
  std::map<int, tab_groups::TabGroupId> tab_id_to_group_id;
  for (size_t i = 0; i < tabs.size(); ++i) {
    TabAndroid* tab = tabs[i];
    if (per_tab_root_id[i] != kInvalidRootId) {
      int root_id = per_tab_root_id[i];
      auto it = group_id_mapping.find(root_id);
      CHECK(it != group_id_mapping.end(), base::NotFatalUntil::M130);
      tab_id_to_group_id.insert(
          std::make_pair(tab->GetAndroidId(), it->second));
    }
  }

  // This wrapper is necessary for bulk closures that don't close all tabs via
  // the bulk tab editor.
  AndroidLiveTabContextCloseWrapper context(
      model, std::move(tabs), std::move(tab_id_to_group_id),
      std::move(tab_group_visual_data), std::move(saved_tab_group_ids_map),
      std::move(web_contents_state));
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

// Static JNI methods.

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

static void JNI_HistoricalTabSaverImpl_CreateHistoricalGroup(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    const JavaParamRef<jobject>& jtab_group_id,
    std::u16string& serialized_saved_tab_group_id,
    std::u16string& title,
    jint jcolor,
    const JavaParamRef<jobjectArray>& jtabs_android,
    const JavaParamRef<jobjectArray>& jbyte_buffers,
    std::vector<int32_t>& saved_state_versions) {
  std::optional<tab_groups::TabGroupId> tab_group_id =
      JavaTokenToTabGroupId(env, jtab_group_id);
  std::optional<base::Uuid> saved_tab_group_id =
      StringToUuid(serialized_saved_tab_group_id);
  auto tabs_android = TabAndroid::GetAllNativeTabs(
      env, base::android::ScopedJavaLocalRef<jobjectArray>(jtabs_android));
  int tabs_android_count = env->GetArrayLength(jtabs_android);
  DCHECK_EQ(tabs_android_count, env->GetArrayLength(jbyte_buffers));
  DCHECK_EQ(tabs_android_count, static_cast<int>(saved_state_versions.size()));

  std::vector<WebContentsStateByteBuffer> web_contents_states =
      AllTabsWebContentsStateByteBuffer(env, jbyte_buffers,
                                        std::move(saved_state_versions));
  CreateHistoricalGroup(TabModelList::FindNativeTabModelForJavaObject(
                            ScopedJavaLocalRef<jobject>(env, jtab_model.obj())),
                        tab_group_id, saved_tab_group_id, title, (int)jcolor,
                        std::move(tabs_android),
                        std::move(web_contents_states));
}

static void JNI_HistoricalTabSaverImpl_CreateHistoricalBulkClosure(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    std::vector<int32_t>& root_ids,
    const JavaParamRef<jobjectArray>& jtab_group_ids,
    std::vector<std::u16string>& serialized_saved_tab_group_ids,
    std::vector<std::u16string>& group_titles,
    std::vector<int32_t>& group_colors,
    std::vector<int32_t>& per_tab_root_id,
    const JavaParamRef<jobjectArray>& jtabs_android,
    const JavaParamRef<jobjectArray>& jbyte_buffers,
    std::vector<int32_t>& saved_state_versions) {
  std::vector<std::optional<tab_groups::TabGroupId>> tab_group_ids =
      JavaTokensToTabGroupIds(env, jtab_group_ids);
  std::vector<std::optional<base::Uuid>> saved_tab_group_ids =
      StringsToUuids(serialized_saved_tab_group_ids);
  int tabs_android_count = env->GetArrayLength(jtabs_android);
  DCHECK_EQ(tabs_android_count, env->GetArrayLength(jbyte_buffers));
  DCHECK_EQ(tabs_android_count, static_cast<int>(saved_state_versions.size()));

  std::vector<WebContentsStateByteBuffer> web_contents_states =
      AllTabsWebContentsStateByteBuffer(env, jbyte_buffers,
                                        std::move(saved_state_versions));
  CreateHistoricalBulkClosure(
      TabModelList::FindNativeTabModelForJavaObject(
          ScopedJavaLocalRef<jobject>(env, jtab_model.obj())),
      std::move(root_ids), std::move(tab_group_ids),
      std::move(saved_tab_group_ids), std::move(group_titles),
      std::move(group_colors), std::move(per_tab_root_id),
      TabAndroid::GetAllNativeTabs(
          env, base::android::ScopedJavaLocalRef<jobjectArray>(jtabs_android)),
      std::move(web_contents_states));
}

}  // namespace historical_tab_saver
