// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/recently_closed_tabs_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/token.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/RecentlyClosedBridge_jni.h"
#include "chrome/android/chrome_jni_headers/RecentlyClosedTab_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

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

jni_zero::ScopedJavaLocalRef<jobject> CreateJavaRecentlyClosedTab(
    JNIEnv* env,
    const sessions::tab_restore::Tab& tab) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.current_navigation_index);
  return Java_RecentlyClosedTab_Constructor(
      env, tab.id.id(), tab.timestamp.InMillisecondsSinceUnixEpoch(),
      current_navigation.title(), current_navigation.virtual_url(),
      tab.group ? std::optional<base::Token>(tab.group->token())
                : std::nullopt);
}

std::vector<jni_zero::ScopedJavaLocalRef<jobject>> PrepareTabs(
    JNIEnv* env,
    TabIterator& it,
    const sessions::TabRestoreService::Entries::const_iterator& current_entry,
    size_t tab_count) {
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> ret;
  ret.reserve(tab_count);
  while (it.CurrentEntry() == current_entry) {
    ret.push_back(CreateJavaRecentlyClosedTab(env, *it));
    ++it;
  }
  return ret;
}

// Add a tab entry to the main entries list.
void AddTabToEntries(JNIEnv* env,
                     const sessions::tab_restore::Tab& tab,
                     const JavaRef<jobject>& jentries) {
  Java_RecentlyClosedBridge_addTabToEntries(
      env, jentries, CreateJavaRecentlyClosedTab(env, tab));
}

void AddGroupToEntries(
    JNIEnv* env,
    TabIterator& it,
    const sessions::TabRestoreService::Entries::const_iterator& current_entry,
    const sessions::tab_restore::Group& group,
    const JavaRef<jobject>& jentries) {
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> tabs =
      PrepareTabs(env, it, current_entry, group.tabs.size());

  Java_RecentlyClosedBridge_addGroupToEntries(
      env, jentries, group.id.id(),
      group.timestamp.InMillisecondsSinceUnixEpoch(), group.visual_data.title(),
      static_cast<int>(group.visual_data.color()), tabs);
}

void AddBulkEventToEntries(
    JNIEnv* env,
    TabIterator& it,
    const sessions::TabRestoreService::Entries::const_iterator& current_entry,
    const sessions::tab_restore::Window& window,
    const JavaRef<jobject>& jentries) {
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> tabs =
      PrepareTabs(env, it, current_entry, window.tabs.size());

  std::vector<std::optional<base::Token>> group_ids;
  std::vector<const std::u16string*> group_titles;

  const size_t group_count = window.tab_groups.size();
  group_ids.reserve(group_count);
  group_titles.reserve(group_count);
  for (const auto& tab_group : window.tab_groups) {
    group_ids.push_back(tab_group.first.token());
    group_titles.push_back(&tab_group.second->visual_data.title());
  }

  Java_RecentlyClosedBridge_addBulkEventToEntries(
      env, jentries, window.id.id(),
      window.timestamp.InMillisecondsSinceUnixEpoch(), group_ids, group_titles,
      tabs);
}

// Add `entries` to `jentries`.
void AddEntriesToList(JNIEnv* env,
                      const sessions::TabRestoreService::Entries& entries,
                      const JavaRef<jobject>& jentries,
                      int max_entry_count) {
  int added_count = 0;
  for (auto it = TabIterator::begin(entries);
       it != TabIterator::end(entries) && added_count < max_entry_count;
       ++added_count) {
    if (it.IsCurrentEntryTab()) {
      AddTabToEntries(env, *it, jentries);
      ++it;
      continue;
    }

    auto entry = it.CurrentEntry();
    if ((*entry)->type == sessions::tab_restore::Type::GROUP) {
      const auto& group =
          static_cast<const sessions::tab_restore::Group&>(**entry);
      AddGroupToEntries(env, it, entry, group, jentries);
      continue;
    }
    if ((*entry)->type == sessions::tab_restore::Type::WINDOW) {
      const auto& window =
          static_cast<const sessions::tab_restore::Window&>(**entry);
      AddBulkEventToEntries(env, it, entry, window, jentries);
      continue;
    }
    NOTREACHED_IN_MIGRATION();
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
  return (*current_entry_)->type == sessions::tab_restore::Type::TAB;
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
  current_tab_ = std::nullopt;
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

const sessions::tab_restore::Tab& TabIterator::operator*() const {
  return current_tab_
             ? ***current_tab_
             : static_cast<const sessions::tab_restore::Tab&>(**current_entry_);
}

const sessions::tab_restore::Tab* TabIterator::operator->() const {
  return current_tab_ ? (*current_tab_)->get()
                      : static_cast<const sessions::tab_restore::Tab*>(
                            current_entry_->get());
}

void TabIterator::SetupInnerTabList() {
  if (current_entry_ == entries_->cend()) {
    return;
  }

  if ((*current_entry_)->type == sessions::tab_restore::Type::GROUP) {
    tabs_ =
        &static_cast<const sessions::tab_restore::Group*>(current_entry_->get())
             ->tabs;
  }
  if ((*current_entry_)->type == sessions::tab_restore::Type::WINDOW) {
    tabs_ = &static_cast<const sessions::tab_restore::Window*>(
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
  if (tab_restore_service_) {
    tab_restore_service_->RemoveObserver(this);
  }
}

void RecentlyClosedTabsBridge::Destroy(JNIEnv* env) {
  delete this;
}

jboolean RecentlyClosedTabsBridge::GetRecentlyClosedEntries(
    JNIEnv* env,
    const JavaParamRef<jobject>& jentries_list,
    jint max_entry_count) {
  EnsureTabRestoreService();
  if (!tab_restore_service_) {
    return false;
  }

  AddEntriesToList(env, tab_restore_service_->entries(), jentries_list,
                   max_entry_count);
  return true;
}

jboolean RecentlyClosedTabsBridge::OpenRecentlyClosedTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_model,
    jint tab_session_id,
    jint j_disposition) {
  if (!tab_restore_service_) {
    return false;
  }

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
  if (!tab_restore_service_) {
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
  if (tab_restore_service_) {
    tab_restore_service_->ClearEntries();
  }
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
  if (tab_restore_service_) {
    return;
  }

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
    std::string saved_tab_group_id =
        group.second.saved_tab_group_id
            ? group.second.saved_tab_group_id->AsLowercaseString()
            : "";
    Java_RecentlyClosedBridge_restoreTabGroup(
        env, bridge_, jtab_model, saved_tab_group_id,
        group.second.visual_data.title(), (int)group.second.visual_data.color(),
        group.second.tab_ids);
  }
}

static jlong JNI_RecentlyClosedBridge_Init(JNIEnv* env,
                                           const JavaParamRef<jobject>& jbridge,
                                           Profile* profile) {
  RecentlyClosedTabsBridge* bridge = new RecentlyClosedTabsBridge(
      ScopedJavaGlobalRef<jobject>(env, jbridge.obj()), profile);
  return reinterpret_cast<intptr_t>(bridge);
}

}  // namespace recent_tabs
