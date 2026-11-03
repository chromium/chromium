// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"

#include <jni.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/RecentTabsPagePrefs_jni.h"

static bool JNI_RecentTabsPagePrefs_GetSnapshotDocumentCollapsed(
    Profile* profile) {
  CHECK(profile);
  return profile->GetPrefs()->GetBoolean(prefs::kNtpCollapsedSnapshotDocument);
}

static void JNI_RecentTabsPagePrefs_SetSnapshotDocumentCollapsed(
    Profile* profile,
    bool is_collapsed) {
  CHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedSnapshotDocument, is_collapsed);
}

static bool JNI_RecentTabsPagePrefs_GetRecentlyClosedTabsCollapsed(
    Profile* profile) {
  CHECK(profile);
  return profile->GetPrefs()->GetBoolean(
      prefs::kNtpCollapsedRecentlyClosedTabs);
}

static void JNI_RecentTabsPagePrefs_SetRecentlyClosedTabsCollapsed(
    Profile* profile,
    bool is_collapsed) {
  CHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedRecentlyClosedTabs, is_collapsed);
}

static bool JNI_RecentTabsPagePrefs_GetSyncPromoCollapsed(Profile* profile) {
  CHECK(profile);
  return profile->GetPrefs()->GetBoolean(prefs::kNtpCollapsedSyncPromo);
}

static void JNI_RecentTabsPagePrefs_SetSyncPromoCollapsed(Profile* profile,
                                                          bool is_collapsed) {
  CHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kNtpCollapsedSyncPromo, is_collapsed);
}

static bool JNI_RecentTabsPagePrefs_GetForeignSessionCollapsed(
    Profile* profile,
    const std::string& session_tag) {
  CHECK(profile);
  const base::DictValue& dict =
      profile->GetPrefs()->GetDict(prefs::kNtpCollapsedForeignSessions);
  return dict.contains(session_tag);
}

static void JNI_RecentTabsPagePrefs_SetForeignSessionCollapsed(
    Profile* profile,
    const std::string& session_tag,
    bool is_collapsed) {
  CHECK(profile);
  // Store session tags for collapsed sessions in a preference so that the
  // collapsed state persists.
  PrefService* prefs = profile->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kNtpCollapsedForeignSessions);
  if (is_collapsed) {
    update->Set(session_tag, true);
  } else {
    update->Remove(session_tag);
  }
}

// static
void RecentTabsPagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNtpCollapsedSnapshotDocument, false);
  registry->RegisterBooleanPref(prefs::kNtpCollapsedRecentlyClosedTabs, false);
  registry->RegisterBooleanPref(prefs::kNtpCollapsedSyncPromo, false);
  registry->RegisterDictionaryPref(prefs::kNtpCollapsedForeignSessions);
}

DEFINE_JNI(RecentTabsPagePrefs)
