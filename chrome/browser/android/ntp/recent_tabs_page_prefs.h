// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_
#define CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class RecentTabsPagePrefs {
 public:
  RecentTabsPagePrefs() = delete;
  RecentTabsPagePrefs(const RecentTabsPagePrefs&) = delete;
  RecentTabsPagePrefs& operator=(const RecentTabsPagePrefs&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_RECENT_TABS_PAGE_PREFS_H_
