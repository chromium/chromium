// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/recently_closed_tabs_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/RecentlyClosedBridge_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

namespace {

void JNI_RecentlyClosedBridge_AddTabToList(
    JNIEnv* env,
    const sessions::TabRestoreService::Tab& tab,
    const JavaRef<jobject>& jtabs_list) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  Java_RecentlyClosedBridge_pushTab(
      env, jtabs_list, tab.id.id(),
      ConvertUTF16ToJavaString(env, current_navigation.title()),
      ConvertUTF8ToJavaString(env, current_navigation.virtual_url().spec()));
}

void JNI_RecentlyClosedBridge_AddTabsToList(
    JNIEnv* env,
    const sessions::TabRestoreService::Entries& entries,
    const JavaRef<jobject>& jtabs_list,
    int max_tab_count) {
  int added_count = 0;
  for (const auto& entry : entries) {
    DCHECK_EQ(entry->type, sessions::TabRestoreService::TAB);
    if (entry->type == sessions::TabRestoreService::TAB) {
      auto& tab = static_cast<const sessions::TabRestoreService::Tab&>(*entry);
      JNI_RecentlyClosedBridge_AddTabToList(env, tab, jtabs_list);
      if (++added_count == max_tab_count)
        break;
    }
  }
}

}  // namespace

RecentlyClosedTabsBridge::RecentlyClosedTabsBridge(
    ScopedJavaGlobalRef<jobject> jbridge,
    Profile* profile)
    : bridge_(std::move(jbridge)),
      profile_(profile),
      tab_restore_service_(nullptr) {}

RecentlyClosedTabsBridge::~RecentlyClosedTabsBridge() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void RecentlyClosedTabsBridge::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean RecentlyClosedTabsBridge::GetRecentlyClosedTabs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jtabs_list,
    jint max_tab_count) {
  EnsureTabRestoreService();
  if (!tab_restore_service_)
    return false;

  JNI_RecentlyClosedBridge_AddTabsToList(env, tab_restore_service_->entries(),
                                         jtabs_list, max_tab_count);
  return true;
}

jboolean RecentlyClosedTabsBridge::OpenRecentlyClosedTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jtab,
    jint recent_tab_id,
    jint j_disposition) {
  if (!tab_restore_service_)
    return false;

  // Find and remove the corresponding tab entry from TabRestoreService.
  // We take ownership of the returned tab.
  std::unique_ptr<sessions::TabRestoreService::Tab> tab_entry(
      tab_restore_service_->RemoveTabEntryById(
          SessionID::FromSerializedValue(recent_tab_id)));
  if (!tab_entry)
    return false;

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  if (!tab_android)
    return false;
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents)
    return false;

  // RestoreForeignSessionTab needs a SessionTab.
  sessions::SessionTab session_tab;
  session_tab.current_navigation_index = tab_entry->current_navigation_index;
  session_tab.navigations = tab_entry->navigations;

  WindowOpenDisposition disposition =
      static_cast<WindowOpenDisposition>(j_disposition);
  SessionRestore::RestoreForeignSessionTab(web_contents,
                                           session_tab,
                                           disposition);
  return true;
}

jboolean RecentlyClosedTabsBridge::OpenMostRecentlyClosedTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) {
  EnsureTabRestoreService();
  if (!tab_restore_service_ || TabModelList::empty() ||
      tab_restore_service_->entries().empty())
    return false;

  // Passing nullptr here because LiveTabContext will be determined later by
  // a call to AndroidLiveTabContext::FindLiveTabContextWithID in
  // ChromeTabRestoreServiceClient.
  std::vector<sessions::LiveTab*> restored_tab =
      tab_restore_service_->RestoreMostRecentEntry(nullptr);

  return !restored_tab.empty();
}

void RecentlyClosedTabsBridge::ClearRecentlyClosedTabs(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  EnsureTabRestoreService();
  if (tab_restore_service_)
    tab_restore_service_->ClearEntries();
}

void RecentlyClosedTabsBridge::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  Java_RecentlyClosedBridge_onUpdated(AttachCurrentThread(), bridge_);
}

void RecentlyClosedTabsBridge::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

void RecentlyClosedTabsBridge::EnsureTabRestoreService() {
  if (tab_restore_service_)
    return;

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);

  // TabRestoreServiceFactory::GetForProfile() can return NULL (e.g. in
  // incognito mode).
  if (tab_restore_service_) {
    // This does nothing if the tabs have already been loaded or they
    // shouldn't be loaded.
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}

static jlong JNI_RecentlyClosedBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbridge,
    const JavaParamRef<jobject>& jprofile) {
  RecentlyClosedTabsBridge* bridge = new RecentlyClosedTabsBridge(
      ScopedJavaGlobalRef<jobject>(env, jbridge.obj()),
      ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(bridge);
}
