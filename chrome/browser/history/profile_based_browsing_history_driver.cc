// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/profile_based_browsing_history_driver.h"

#include <utility>

#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#endif

void ProfileBasedBrowsingHistoryDriver::OnRemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the profile has activity logging enabled also clean up any URLs from the
  // extension activity log. The extension activity log contains URLS which
  // websites an extension has activity on so it will indirectly contain
  // websites that a user has visited.
  extensions::ActivityLog* activity_log =
      extensions::ActivityLog::GetInstance(GetProfile());
  for (const history::ExpireHistoryArgs& expire_entry : expire_list) {
    activity_log->RemoveURLs(expire_entry.urls);
  }
#endif

  for (const history::ExpireHistoryArgs& expire_entry : expire_list) {
    webapps::AppBannerSettingsHelper::ClearHistoryForURLs(GetProfile(),
                                                          expire_entry.urls);
  }
}

bool ProfileBasedBrowsingHistoryDriver::AllowHistoryDeletions() {
  return GetProfile()->GetPrefs()->GetBoolean(
      prefs::kAllowDeletingBrowserHistory);
}

bool ProfileBasedBrowsingHistoryDriver::ShouldHideWebHistoryUrl(
    const GURL& url) {
  return !CanAddURLToHistory(url);
}

history::WebHistoryService*
ProfileBasedBrowsingHistoryDriver::GetWebHistoryService() {
  return WebHistoryServiceFactory::GetForProfile(GetProfile());
}

void ProfileBasedBrowsingHistoryDriver::
    ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
        const syncer::SyncService* sync_service,
        history::WebHistoryService* history_service,
        base::OnceCallback<void(bool)> callback) {
  return browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service, history_service, std::move(callback));
}
