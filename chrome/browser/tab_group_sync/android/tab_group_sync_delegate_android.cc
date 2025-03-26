// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/android/tab_group_sync_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/public/types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_group_sync/delegate_jni_headers/TabGroupSyncDelegate_jni.h"

namespace tab_groups {

TabGroupSyncDelegateAndroid::TabGroupSyncDelegateAndroid(
    TabGroupSyncService* service,
    ScopedJavaLocalRef<jobject> j_delegate_deps)
    : tab_group_sync_service_(service) {
  DCHECK(tab_group_sync_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env,
                  Java_TabGroupSyncDelegate_create(
                      env, reinterpret_cast<int64_t>(this), j_delegate_deps)
                      .obj());
}

TabGroupSyncDelegateAndroid::~TabGroupSyncDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupSyncDelegate_destroy(env, java_obj_);
}

std::optional<LocalTabGroupID>
TabGroupSyncDelegateAndroid::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  return std::nullopt;
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncDelegateAndroid::CreateScopedLocalObserverPauser() {
  return nullptr;
}

void TabGroupSyncDelegateAndroid::CreateLocalTabGroup(
    const SavedTabGroup& tab_group) {}

void TabGroupSyncDelegateAndroid::CloseLocalTabGroup(
    const LocalTabGroupID& local_id) {}

void TabGroupSyncDelegateAndroid::ConnectLocalTabGroup(
    const SavedTabGroup& group) {}

void TabGroupSyncDelegateAndroid::DisconnectLocalTabGroup(
    const LocalTabGroupID& local_id) {}

void TabGroupSyncDelegateAndroid::UpdateLocalTabGroup(
    const SavedTabGroup& group) {}

std::vector<LocalTabGroupID>
TabGroupSyncDelegateAndroid::GetLocalTabGroupIds() {
  return std::vector<LocalTabGroupID>();
}

std::vector<LocalTabID> TabGroupSyncDelegateAndroid::GetLocalTabIdsForTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  return std::vector<LocalTabID>();
}

std::set<LocalTabID> TabGroupSyncDelegateAndroid::GetSelectedTabs() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_selected_tabs_array =
      Java_TabGroupSyncDelegate_getSelectedTabs(env, java_obj_);
  std::vector<int> selected_tabs_vector;
  base::android::JavaIntArrayToIntVector(env, j_selected_tabs_array,
                                         &selected_tabs_vector);
  return std::set<LocalTabID>(selected_tabs_vector.begin(),
                              selected_tabs_vector.end());
}

std::u16string TabGroupSyncDelegateAndroid::GetTabTitle(
    const LocalTabID& local_tab_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_tab_id = ToJavaTabId(local_tab_id);
  auto j_title =
      Java_TabGroupSyncDelegate_getTabTitle(env, java_obj_, j_tab_id);
  return base::android::ConvertJavaStringToUTF16(j_title);
}

std::unique_ptr<SavedTabGroup>
TabGroupSyncDelegateAndroid::CreateSavedTabGroupFromLocalGroup(
    const LocalTabGroupID& local_tab_group_id) {
  return nullptr;
}

}  // namespace tab_groups
