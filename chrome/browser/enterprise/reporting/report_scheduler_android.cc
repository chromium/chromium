// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_scheduler_android.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

ReportSchedulerAndroid::ReportSchedulerAndroid()
    : profile_(nullptr), prefs_(g_browser_process->local_state()) {}
ReportSchedulerAndroid::ReportSchedulerAndroid(Profile* profile)
    : profile_(profile), prefs_(profile_->GetPrefs()) {}

ReportSchedulerAndroid::~ReportSchedulerAndroid() = default;

PrefService* ReportSchedulerAndroid::GetPrefService() {
  return prefs_;
}

void ReportSchedulerAndroid::StartWatchingUpdatesIfNeeded(
    base::Time last_upload,
    base::TimeDelta upload_interval) {
  // No-op because in-app auto-update is not supported on Android.
}

void ReportSchedulerAndroid::StopWatchingUpdates() {
  // No-op because in-app auto-update is not supported on Android.
}

void ReportSchedulerAndroid::OnBrowserVersionUploaded() {
  // No-op because in-app auto-update is not supported on Android.
}

policy::DMToken ReportSchedulerAndroid::GetProfileDMToken() {
  std::optional<std::string> dm_token = reporting::GetUserDmToken(profile_);
  if (!dm_token || dm_token->empty())
    return policy::DMToken::CreateEmptyToken();
  return policy::DMToken::CreateValidToken(*dm_token);
}

std::string ReportSchedulerAndroid::GetProfileClientId() {
  return reporting::GetUserClientId(profile_).value_or(std::string());
}

}  // namespace enterprise_reporting
