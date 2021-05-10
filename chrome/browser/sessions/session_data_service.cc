// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_service.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace {
// Pref name for Status preference.
extern const char kSessionDataStatusPref[] = "sessions.session_data_status";
}  // namespace

// static
void SessionDataService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(kSessionDataStatusPref,
                                static_cast<int>(Status::kUninitialized));
}

SessionDataService::SessionDataService(
    Profile* profile,
    std::unique_ptr<SessionDataDeleter> deleter)
    : profile_(profile), deleter_(std::move(deleter)) {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(deleter_);

  Status last_status = static_cast<Status>(
      profile_->GetPrefs()->GetInteger(kSessionDataStatusPref));

  int int_status = static_cast<int>(last_status);
  if (int_status < 0 || int_status > static_cast<int>(Status::kMaxValue))
    last_status = Status::kUninitialized;

  SetStatusPref(Status::kInitialized);
  RecordHistogramForLastSession(last_status);

  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);

  BrowserList::AddObserver(this);
}

SessionDataService::~SessionDataService() {
  BrowserList::RemoveObserver(this);
}

void SessionDataService::RecordHistogramForLastSession(Status last_status) {
  if (last_status == Status::kUninitialized)
    return;

  auto* policy = profile_->GetSpecialStoragePolicy();
  if (!policy || !policy->HasSessionOnlyOrigins())
    return;

  base::UmaHistogramEnumeration("Session.SessionData.StatusFromLastSession",
                                last_status);
}

void SessionDataService::SetStatusPref(Status status) {
  profile_->GetPrefs()->SetInteger(kSessionDataStatusPref,
                                   static_cast<int>(status));
}

void SessionDataService::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  // A window was opened. Ensure that we run another cleanup the next time
  // all windows are closed.
  SetStatusPref(Status::kInitialized);
  cleanup_started_ = false;
}

void SessionDataService::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  // Clear session data if the last window for a profile has been closed and
  // closing the last window would normally close Chrome.
  if (browser_defaults::kBrowserAliveWithNoWindows)
    return;

  // Check for any open windows for the current profile that we aren't tracking.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile_)
      return;
  }
  StartCleanup();
}

void SessionDataService::StartCleanup() {
  if (cleanup_started_)
    return;

  if (force_keep_session_state_)
    return;

  if (browser_shutdown::IsTryingToQuit()) {
    SetStatusPref(Status::kNoDeletionDueToShutdown);
    return;
  }

  cleanup_started_ = true;
  SetStatusPref(Status::kDeletionStarted);

  // Using base::Unretained is safe as DeleteSessionOnlyData() uses a
  // ScopedProfileKeepAlive.
  deleter_->DeleteSessionOnlyData(
      base::BindOnce(&SessionDataService::OnCleanupFinished,
                     base::Unretained(this), base::TimeTicks::Now()));
}

void SessionDataService::OnCleanupFinished(base::TimeTicks time_started) {
  SetStatusPref(Status::kDeletionFinished);
  base::UmaHistogramMediumTimes("Session.SessionData.CleanupTime",
                                base::TimeTicks::Now() - time_started);
}

void SessionDataService::SetForceKeepSessionState() {
  SetStatusPref(Status::kNoDeletionDueToForceKeepSessionData);
  force_keep_session_state_ = true;
}
