// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_
#define CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"

class RecentTabsPagePrefs {
 public:
  explicit RecentTabsPagePrefs(Profile* profile);
  void Destroy(JNIEnv* env);

  RecentTabsPagePrefs(const RecentTabsPagePrefs&) = delete;
  RecentTabsPagePrefs& operator=(const RecentTabsPagePrefs&) = delete;

  bool GetSnapshotDocumentCollapsed(JNIEnv* env);
  void SetSnapshotDocumentCollapsed(JNIEnv* env, bool is_collapsed);

  bool GetRecentlyClosedTabsCollapsed(JNIEnv* env);
  void SetRecentlyClosedTabsCollapsed(JNIEnv* env, bool is_collapsed);

  bool GetSyncPromoCollapsed(JNIEnv* env);
  void SetSyncPromoCollapsed(JNIEnv* env, bool is_collapsed);

  bool GetForeignSessionCollapsed(JNIEnv* env, const std::string& session_tag);
  void SetForeignSessionCollapsed(JNIEnv* env,
                                  const std::string& session_tag,
                                  bool is_collapsed);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  virtual ~RecentTabsPagePrefs();

  raw_ptr<Profile> profile_;  // weak
};

#endif  // CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_
