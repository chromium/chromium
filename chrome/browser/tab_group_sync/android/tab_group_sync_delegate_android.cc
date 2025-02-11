// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/android/tab_group_sync_delegate_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_group_sync/delegate_jni_headers/TabGroupSyncDelegate_jni.h"

namespace tab_groups {

TabGroupSyncDelegateAndroid::TabGroupSyncDelegateAndroid(
    TabGroupSyncService* service)
    : tab_group_sync_service_(service) {
  DCHECK(tab_group_sync_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_TabGroupSyncDelegate_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

TabGroupSyncDelegateAndroid::~TabGroupSyncDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupSyncDelegate_destroy(env, java_obj_);
}

void TabGroupSyncDelegateAndroid::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {}

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

std::unique_ptr<SavedTabGroup>
TabGroupSyncDelegateAndroid::CreateSavedTabGroupFromLocalGroup(
    const LocalTabGroupID& local_tab_group_id) {
  return nullptr;
}

}  // namespace tab_groups
