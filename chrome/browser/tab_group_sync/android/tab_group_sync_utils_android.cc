// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_group_sync/jni_headers/TabGroupSyncUtils_jni.h"

static void JNI_TabGroupSyncUtils_OnDidFinishNavigation(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_group_id,
    jint j_tab_id,
    jlong navigation_handle_ptr) {
  CHECK(profile);
  tab_groups::TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  CHECK(service);

  auto group_id = tab_groups::TabGroupSyncConversionsBridge::FromJavaTabGroupId(
      env, j_group_id);
  auto tab_group = service->GetGroup(group_id);
  if (!tab_group) {
    return;
  }
  auto tab_id = tab_groups::FromJavaTabId(j_tab_id);

  auto* navigation_handle =
      reinterpret_cast<content::NavigationHandle*>(navigation_handle_ptr);
  tab_groups::SavedTabGroupTabBuilder tab_builder;
  tab_builder.SetRedirectURLChain(navigation_handle->GetRedirectChain());
  service->UpdateTab(group_id, tab_id, tab_builder);

  tab_groups::TabGroupSyncUtils::RecordSavedTabGroupNavigationUkmMetrics(
      tab_id,
      tab_group->collaboration_id() ? tab_groups::SavedTabGroupType::SHARED
                                    : tab_groups::SavedTabGroupType::SYNCED,
      navigation_handle, service);
}

static jboolean JNI_TabGroupSyncUtils_IsUrlInTabRedirectChain(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_group_id,
    jint j_tab_id,
    GURL& url) {
  CHECK(profile);
  tab_groups::TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  CHECK(service);

  auto group_id = tab_groups::TabGroupSyncConversionsBridge::FromJavaTabGroupId(
      env, j_group_id);
  auto tab_group = service->GetGroup(group_id);
  if (!tab_group) {
    return false;
  }

  auto tab_id = tab_groups::FromJavaTabId(j_tab_id);
  auto* tab = tab_group->GetTab(tab_id);
  if (!tab) {
    return false;
  }

  return tab->IsURLInRedirectChain(url);
}
