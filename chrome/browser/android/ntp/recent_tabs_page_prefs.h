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

  jboolean GetSnapshotDocumentCollapsed(JNIEnv* env);
  void SetSnapshotDocumentCollapsed(
      JNIEnv* env,
      jboolean is_collapsed);

  jboolean GetRecentlyClosedTabsCollapsed(JNIEnv* env);
  void SetRecentlyClosedTabsCollapsed(
      JNIEnv* env,
      jboolean is_collapsed);

  jboolean GetSyncPromoCollapsed(JNIEnv* env);
  void SetSyncPromoCollapsed(JNIEnv* env,
                             jboolean is_collapsed);

  jboolean GetForeignSessionCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& session_tag);
  void SetForeignSessionCollapsed(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& session_tag,
      jboolean is_collapsed);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  virtual ~RecentTabsPagePrefs();

  raw_ptr<Profile> profile_;  // weak
};

#endif  // CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_
