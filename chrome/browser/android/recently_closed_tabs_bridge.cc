// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/recently_closed_tabs_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "chrome/android/chrome_jni_headers/RecentlyClosedBridge_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;
using base::android::ToJavaLongArray;

namespace recent_tabs {
namespace {

bool TabEntryWithIdExists(const sessions::TabRestoreService::Entries& entries,
                          SessionID session_id) {
  const auto end = TabIterator::end(entries);
  for (auto it = TabIterator::begin(entries); it != end; ++it) {
    if (it->id == session_id) {
      return true;
    }
  }
  return false;
}

// Helpers for GetRecentlyClosedEntries:

void PrepareTabs(
    JNIEnv* env,
    TabIterator& it,
    const sessions::TabRestoreService::Entries::const_iterator& current_entry,
    std::vector<int>& ids,
    std::vector<int64_t>& timestamps,
    std::vector<std::u16string>& titles,
    std::vector<ScopedJavaLocalRef<jobject>>& urls,
    std::vector<std::string>& group_ids) {
  while (it.CurrentEntry() == current_entry) {
    const sessions::TabRestoreService::Tab& tab = *it;
    const sessions::SerializedNavigationEntry& current_navigation =
        tab.navigations.at(tab.current_navigation_index);
    ids.push_back(tab.id.id());
    timestamps.push_back(tab.timestamp.InMillisecondsSinceUnixEpoch());
    titles.push_back(current_navigation.title());
    urls.push_back(url::GURLAndroid::FromNativeGURL(
        env, current_navigation.virtual_url()));
    group_ids.push_back(tab.group ? tab.group->ToString() : "");
    ++it;
  }
}

// Add a tab entry to the main entries list.
void JNI_RecentlyClosedBridge_AddTabToEntries(
    JNIEnv* env,
    const sessions::TabRestoreService::Tab& tab,
    const JavaRef<jobject>& jentries) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  Java_RecentlyClosedBridge_addTabToEntries(
      env, jentries, tab.id.id(), tab.timestamp.InMillisecondsSinceUnixEpoch(),
      ConvertUTF16ToJavaString(env, current_navigation.title()),
      url::GURLAndroid::FromNativeGURL(env, current_navigation.virtual_url()),
      tab.group ? ConvertUTF8ToJavaString(env, tab.group->ToString())
                : nullptr);
}

void JNI_RecentlyClosedBridge_AddGroupToEntries(
    JNIEnv* env,
    TabIterator& it,
    const sessions::TabRestoreService::Entries::const_iterator& current_entry,
    const sessions::TabRestoreService::Group& group,
    const JavaRef<jobject>& jentries) {
  std::vector<int> ids;
  std::vector<int64_t> timestamps;
  std::vector<std::u16string> titles;
  std::vector<ScopedJavaLocalRef<jobject>> urls;
  std::vector<std::string> group_ids;

  const size_t tab_count = group.tabs.size();
  ids.reserve(tab_count);
  timestamps.reserve(tab_count);
  titles.reserve(tab_count);
  urls.reserve(tab_count);
  group_ids.reserve(tab_count);
  PrepareTabs(env, it, current_entry, ids, timestamps, titles, urls, group_ids);

  Java_RecentlyClosedBridge_addGroupToEntries(
      env, jentries, group.id.id(),
      group.timestamp.InMillisecondsSinceUnixEpoch(),
      ConvertUTF16ToJavaString(env, group.visual_data.title()),
      ToJavaIntArray(env, ids), ToJavaLongArray(env, timestamps),
      ToJavaArrayOfStrings(env, titles),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, urls),
      ToJavaArrayOfStrings(env, group_ids));
}

void JNI_RecentlyClosedBridge_AddBulkEventToEntries(
    JNIEnv* env,
    TabIterator& it,
    const sessions::TabRestoreService::Entries::const_iterator& current_entry,
    const sessions::TabRestoreService::Window& window,
    const JavaRef<jobject>& jentries) {
  std::vector<int> ids;
  std::vector<int64_t> timestamps;
  std::vector<std::u16string> titles;
  std::vector<ScopedJavaLocalRef<jobject>> urls;
  std::vector<std::string> per_tab_group_ids;

  const size_t tab_count = window.tabs.size();
  ids.reserve(tab_count);
  timestamps.reserve(tab_count);
  titles.reserve(tab_count);
  urls.reserve(tab_count);
  per_tab_group_ids.reserve(tab_count);
  PrepareTabs(env, it, current_entry, ids, timestamps, titles, urls,
              per_tab_group_ids);

  std::vector<std::string> group_ids;
  std::vector<std::u16string> group_titles;

  const size_t group_count = window.tab_groups.size();
  group_ids.reserve(group_count);
  group_titles.reserve(group_count);
  for (const auto& tab_group : window.tab_groups) {
    group_ids.push_back(tab_group.first.ToString());
    group_titles.push_back(tab_group.second.title());
  }

  Java_RecentlyClosedBridge_addBulkEventToEntries(
      env, jentries, window.id.id(),
      window.timestamp.InMillisecondsSinceUnixEpoch(),
      ToJavaArrayOfStrings(env, group_ids),
      ToJavaArrayOfStrings(env, group_titles), ToJavaIntArray(env, ids),
      ToJavaLongArray(env, timestamps), ToJavaArrayOfStrings(env, titles),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, urls),
      ToJavaArrayOfStrings(env, per_tab_group_ids));
}

// Add `entries` to `jentries`.
void JNI_RecentlyClosedBridge_AddEntriesToList(
    JNIEnv* env,
    const sessions::TabRestoreService::Entries& entries,
    const JavaRef<jobject>& jentries,
    int max_entry_count) {
  int added_count = 0;
  for (auto it = TabIterator::begin(entries);
       it != TabIterator::end(entries) && added_count < max_entry_count;
       ++added_count) {
    if (it.IsCurrentEntryTab()) {
      JNI_RecentlyClosedBridge_AddTabToEntries(env, *it, jentries);
      ++it;
      continue;
    }

    auto entry = it.CurrentEntry();
    if ((*entry)->type == sessions::TabRestoreService::GROUP) {
      const auto& group =
          static_cast<const sessions::TabRestoreService::Group&>(**entry);
      JNI_RecentlyClosedBridge_AddGroupToEntries(env, it, entry, group,
                                                 jentries);
      continue;
    }
    if ((*entry)->type == sessions::TabRestoreService::WINDOW) {
      const auto& window =
          static_cast<const sessions::TabRestoreService::Window&>(**entry);
      JNI_RecentlyClosedBridge_AddBulkEventToEntries(env, it, entry, window,
                                                     jentries);
      continue;
    }
    NOTREACHED();
  }
}

}  // namespace

TabIterator::TabIterator(
    const sessions::TabRestoreService::Entries& entries,
    sessions::TabRestoreService::Entries::const_iterator it)
    : entries_(entries), current_entry_(it) {
  SetupInnerTabList();
}
TabIterator::~TabIterator() = default;

// static.
TabIterator TabIterator::begin(
    const sessions::TabRestoreService::Entries& entries) {
  return TabIterator(entries, entries.cbegin());
}

// static.
TabIterator TabIterator::end(
    const sessions::TabRestoreService::Entries& entries) {
  return TabIterator(entries, entries.cend());
}

bool TabIterator::IsCurrentEntryTab() const {
  return (*current_entry_)->type == sessions::TabRestoreService::TAB;
}

sessions::TabRestoreService::Entries::const_iterator TabIterator::CurrentEntry()
    const {
  return current_entry_;
}

TabIterator& TabIterator::operator++() {
  // Early out at end.
  if (current_entry_ == entries_->cend()) {
    return *this;
  }

  // Iterate backward over current set of tabs if possible.
  if (current_tab_ && tabs_ && current_tab_ != tabs_->crend()) {
    (*current_tab_)++;
    if (*current_tab_ != tabs_->crend()) {
      return *this;
    }
  }

  // At the end of an entry then go to the next entry.
  tabs_ = nullptr;
  current_tab_ = absl::nullopt;
  current_entry_++;
  if (current_entry_ == entries_->cend()) {
    return *this;
  }

  SetupInnerTabList();

  return *this;
}

TabIterator TabIterator::operator++(int) {
  TabIterator retval = *this;
  ++(*this);
  return retval;
}

bool TabIterator::operator==(TabIterator other) const {
  return current_entry_ == other.current_entry_ &&
         current_tab_ == other.current_tab_;
}

bool TabIterator::operator!=(TabIterator other) const {
  return !(*this == other);
}

const sessions::TabRestoreService::Tab& TabIterator::operator*() const {
  return current_tab_ ? ***current_tab_
                      : static_cast<const sessions::TabRestoreService::Tab&>(
                            **current_entry_);
}

const sessions::TabRestoreService::Tab* TabIterator::operator->() const {
  return current_tab_ ? (*current_tab_)->get()
                      : static_cast<const sessions::TabRestoreService::Tab*>(
                            current_entry_->get());
}

void TabIterator::SetupInnerTabList() {
  if (current_entry_ == entries_->cend()) {
    return;
  }

  if ((*current_entry_)->type == sessions::TabRestoreService::GROUP) {
    tabs_ = &static_cast<const sessions::TabRestoreService::Group*>(
                 current_entry_->get())
                 ->tabs;
  }
  if ((*current_entry_)->type == sessions::TabRestoreService::WINDOW) {
    tabs_ = &static_cast<const sessions::TabRestoreService::Window*>(
                 current_entry_->get())
                 ->tabs;
  }
  if (tabs_) {
    current_tab_ = tabs_->crbegin();
    if (current_tab_ == tabs_->crend()) {
      ++(*this);
    }
  }
}

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

void RecentlyClosedTabsBridge::Destroy(JNIEnv* env) {
  delete this;
}

jboolean RecentlyClosedTabsBridge::GetRecentlyClosedEntries(
    JNIEnv* env,
    const JavaParamRef<jobject>& jentries_list,
    jint max_entry_count) {
  EnsureTabRestoreService();
  if (!tab_restore_service_)
    return false;

  JNI_RecentlyClosedBridge_AddEntriesToList(
      env, tab_restore_service_->entries(), jentries_list, max_entry_count);
  return true;
}

jboolean RecentlyClosedTabsBridge::OpenRecentlyClosedTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    jint tab_session_id,
    jint j_disposition) {
  if (!tab_restore_service_)
    return false;

  SessionID entry_id = SessionID::FromSerializedValue(tab_session_id);
  // Ensure the corresponding tab entry from TabRestoreService is a tab.
  if (!TabEntryWithIdExists(tab_restore_service_->entries(), entry_id)) {
    return false;
  }

  auto* model = TabModelList::FindNativeTabModelForJavaObject(
      ScopedJavaLocalRef<jobject>(env, jtab_model.obj()));
  if (model == nullptr) {
    return false;
  }

  AndroidLiveTabContextRestoreWrapper restore_context(model);
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service_->RestoreEntryById(
          &restore_context, entry_id,
          static_cast<WindowOpenDisposition>(j_disposition));
  return !restored_tabs.empty();
}

jboolean RecentlyClosedTabsBridge::OpenRecentlyClosedEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    jint entry_session_id) {
  // This should only be called when in bulk restore mode otherwise per-tab
  // restore should always be used.
  if (!tab_restore_service_)
    return false;

  auto* model = TabModelList::FindNativeTabModelForJavaObject(
      ScopedJavaLocalRef<jobject>(env, jtab_model.obj()));
  if (model == nullptr) {
    return false;
  }

  AndroidLiveTabContextRestoreWrapper restore_context(model);
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service_->RestoreEntryById(
          &restore_context, SessionID::FromSerializedValue(entry_session_id),
          WindowOpenDisposition::NEW_BACKGROUND_TAB);
  RestoreAndroidTabGroups(env, jtab_model, restore_context.GetTabGroups());
  return !restored_tabs.empty();
}

jboolean RecentlyClosedTabsBridge::OpenMostRecentlyClosedEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model) {
  EnsureTabRestoreService();
  if (!tab_restore_service_ || tab_restore_service_->entries().empty()) {
    return false;
  }

  auto* model = TabModelList::FindNativeTabModelForJavaObject(
      ScopedJavaLocalRef<jobject>(env, jtab_model.obj()));
  if (model == nullptr) {
    return false;
  }

  AndroidLiveTabContextRestoreWrapper restore_context(model);
  std::vector<sessions::LiveTab*> restored_tabs;
  // Do not use OpenMostRecentEntry as it uses WindowOpenDisposition::UNKNOWN.
  // WindowOpenDisposition::UNKNOWN looks for a desktop window to use (N/A on
  // Android) this ends up replacing `restore_context` with the base
  // AndroidLiveTabContext. `restore_context` is required to rebuild groups
  // information. To avoid this just use the first entry in entries when
  // restoring.
  restored_tabs = tab_restore_service_->RestoreEntryById(
      &restore_context, tab_restore_service_->entries().front()->id,
      WindowOpenDisposition::NEW_BACKGROUND_TAB);
  RestoreAndroidTabGroups(env, jtab_model, restore_context.GetTabGroups());
  return !restored_tabs.empty();
}

void RecentlyClosedTabsBridge::ClearRecentlyClosedEntries(JNIEnv* env) {
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
  tab_restore_service_ = nullptr;
}

void RecentlyClosedTabsBridge::EnsureTabRestoreService() {
  if (tab_restore_service_)
    return;

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);

  // TabRestoreServiceFactory::GetForProfile() can return nullptr (e.g. in
  // incognito mode).
  if (tab_restore_service_) {
    // This does nothing if the tabs have already been loaded or they
    // shouldn't be loaded.
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}

void RecentlyClosedTabsBridge::RestoreAndroidTabGroups(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jtab_model,
    const std::map<tab_groups::TabGroupId,
                   AndroidLiveTabContextRestoreWrapper::TabGroup>& groups) {
  for (const auto& group : groups) {
    base::span<int const> tab_ids(group.second.tab_ids);
    // Ignore single tabs. This can occur if a grouped tab is restored on its
    // own.
    if (tab_ids.size() < 2U) {
      continue;
    }

    const int group_id = tab_ids[0];
    Java_RecentlyClosedBridge_restoreTabGroup(
        env, bridge_, jtab_model, group_id,
        ConvertUTF16ToJavaString(env, group.second.visual_data.title()),
        base::android::ToJavaIntArray(env, tab_ids.subspan(1)));
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

}  // namespace recent_tabs
