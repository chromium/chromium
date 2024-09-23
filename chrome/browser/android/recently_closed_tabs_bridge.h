// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RECENTLY_CLOSED_TABS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_RECENTLY_CLOSED_TABS_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context_wrapper.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Profile;

namespace recent_tabs {

// Used to iterating over sessions::TabRestoreService::Entries in most recently
// added tab to least recently added tab order.
class TabIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = size_t;
  using value_type = sessions::tab_restore::Tab;
  using pointer = value_type*;
  using reference = value_type&;

  TabIterator(const sessions::TabRestoreService::Entries& entries,
              sessions::TabRestoreService::Entries::const_iterator it);

  ~TabIterator();

  static TabIterator begin(const sessions::TabRestoreService::Entries& entries);

  static TabIterator end(const sessions::TabRestoreService::Entries& entries);

  // Whether the current entry is a sessions::tab_restore::Tab.
  bool IsCurrentEntryTab() const;

  // Gets an iterator to the current entry being traversed.
  sessions::TabRestoreService::Entries::const_iterator CurrentEntry() const;

  TabIterator& operator++();
  TabIterator operator++(int);
  bool operator==(TabIterator other) const;
  bool operator!=(TabIterator other) const;
  const sessions::tab_restore::Tab& operator*() const;
  const sessions::tab_restore::Tab* operator->() const;

 private:
  void SetupInnerTabList();

  const raw_ref<const sessions::TabRestoreService::Entries> entries_;
  sessions::TabRestoreService::Entries::const_iterator current_entry_;
  raw_ptr<const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>>
      tabs_ = nullptr;
  std::optional<std::vector<
      std::unique_ptr<sessions::tab_restore::Tab>>::const_reverse_iterator>
      current_tab_ = std::nullopt;
};

// Provides the list of recently closed tabs to Java.
class RecentlyClosedTabsBridge : public sessions::TabRestoreServiceObserver {
 public:
  RecentlyClosedTabsBridge(base::android::ScopedJavaGlobalRef<jobject> jbridge,
                           Profile* profile);

  RecentlyClosedTabsBridge(const RecentlyClosedTabsBridge&) = delete;
  RecentlyClosedTabsBridge& operator=(const RecentlyClosedTabsBridge&) = delete;

  void Destroy(JNIEnv* env);

  jboolean GetRecentlyClosedEntries(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jentries,
      jint max_entry_count);
  jboolean OpenRecentlyClosedTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jtab_model,
      jint tab_session_id,
      jint j_disposition);
  jboolean OpenRecentlyClosedEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jtab_model,
      jint session_id);
  jboolean OpenMostRecentlyClosedEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jtab_model);
  void ClearRecentlyClosedEntries(JNIEnv* env);

  // Observer callback for TabRestoreServiceObserver. Notifies the Java bridge
  // that the recently closed tabs list has changed.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;

  // Observer callback when our associated TabRestoreService is destroyed.
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  ~RecentlyClosedTabsBridge() override;

  // Construct and initialize tab_restore_service_ if it's NULL.
  // tab_restore_service_ may still be NULL, however, in incognito mode.
  void EnsureTabRestoreService();

  void RestoreAndroidTabGroups(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jtab_model,
      const std::map<tab_groups::TabGroupId,
                     AndroidLiveTabContextRestoreWrapper::TabGroup>& groups);

  // The Java RecentlyClosedBridge.
  base::android::ScopedJavaGlobalRef<jobject> bridge_;

  // The profile whose recently closed tabs are being monitored.
  raw_ptr<Profile> profile_;

  // TabRestoreService that we are observing.
  raw_ptr<sessions::TabRestoreService> tab_restore_service_;
};

}  // namespace recent_tabs

#endif  // CHROME_BROWSER_ANDROID_RECENTLY_CLOSED_TABS_BRIDGE_H_
