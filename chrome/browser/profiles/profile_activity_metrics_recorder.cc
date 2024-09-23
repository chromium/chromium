// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace {

ProfileActivityMetricsRecorder* g_profile_activity_metrics_recorder = nullptr;

// The maximum number of profiles that are recorded. This means that all
// profiles with bucket index greater than |kMaxProfileBucket| won't be included
// in the metrics.
constexpr int kMaxProfileBucket = 100;

// Long time of inactivity that is treated as if user starts the browser anew.
constexpr base::TimeDelta kLongTimeOfInactivity = base::Minutes(30);

int GetMetricsBucketIndex(const Profile* profile) {
  if (profile->IsGuestSession())
    return 0;

  if (!g_browser_process->profile_manager()) {
    VLOG(1) << "Failed to read profile bucket index because profile manager "
               "doesn't exist.";
    return -1;
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    // This can happen if the profile is deleted.
    VLOG(1) << "Failed to read profile bucket index because attributes entry "
               "doesn't exist.";
    return -1;
  }
  return entry->GetMetricsBucketIndex();
}

void RecordProfileSessionDuration(const Profile* profile,
                                  base::TimeDelta session_length) {
  if (!profile || session_length.InMinutes() <= 0)
    return;

  int profile_bucket = GetMetricsBucketIndex(profile);

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    base::Histogram::FactoryGet("Profile.SessionDuration.PerProfile", 0,
                                kMaxProfileBucket, kMaxProfileBucket + 1,
                                base::HistogramBase::kUmaTargetedHistogramFlag)
        ->AddCount(profile_bucket, session_length.InMinutes());
  }
}

void RecordBrowserActivation(const Profile* profile) {
  DCHECK(profile);
  int profile_bucket = GetMetricsBucketIndex(profile);

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    UMA_HISTOGRAM_EXACT_LINEAR("Profile.BrowserActive.PerProfile",
                               profile_bucket, kMaxProfileBucket);
  }
}

void RecordProfileSwitch() {
  int profiles_count =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfProfilesAtProfileSwitch",
                           profiles_count);
}

void RecordUserAction(const Profile* profile) {
  if (!profile)
    return;

  int profile_bucket = GetMetricsBucketIndex(profile);

  if (0 <= profile_bucket && profile_bucket <= kMaxProfileBucket) {
    UMA_HISTOGRAM_EXACT_LINEAR("Profile.UserAction.PerProfile", profile_bucket,
                               kMaxProfileBucket);
  }
}

void RecordProfilesState() {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RecordProfilesState();
}

void RecordAccountMetrics(const Profile* profile) {
  DCHECK(profile);

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    // This can happen if the profile is deleted / for guest profile.
    return;
  }

  entry->RecordAccountNamesMetric();
}

}  // namespace

// static
void ProfileActivityMetricsRecorder::Initialize() {
  DCHECK(!g_profile_activity_metrics_recorder);
  g_profile_activity_metrics_recorder = new ProfileActivityMetricsRecorder();
}

// static
void ProfileActivityMetricsRecorder::CleanupForTesting() {
  DCHECK(g_profile_activity_metrics_recorder);
  delete g_profile_activity_metrics_recorder;
  g_profile_activity_metrics_recorder = nullptr;
}

void ProfileActivityMetricsRecorder::OnBrowserSetLastActive(Browser* browser) {
  Profile* active_profile = browser->profile()->GetOriginalProfile();

  RecordBrowserActivation(active_profile);
  RecordAccountMetrics(active_profile);

  if (running_session_profile_ != active_profile) {
    // No-op, if starting a new session (|running_session_profile_| is nullptr).
    RecordProfileSessionDuration(
        running_session_profile_,
        base::TimeTicks::Now() - running_session_start_);

    running_session_profile_ = active_profile;
    running_session_start_ = base::TimeTicks::Now();
    profile_observation_.Reset();
    profile_observation_.Observe(running_session_profile_.get());

    // Record state at startup (when |last_session_end_| is 0) and whenever the
    // user starts browsing after a longer time of inactivity. Do it
    // asynchronously because active_time of the just activated profile is also
    // updated from OnBrowserSetLastActive() in another BrowserListObserver and
    // we have no guarantee if this happens before or after this function call.
    if (last_session_end_.is_null() ||
        (running_session_start_ - last_session_end_ > kLongTimeOfInactivity)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&RecordProfilesState));
    }
  }

  if (last_active_profile_ != active_profile) {
    if (last_active_profile_ != nullptr)
      RecordProfileSwitch();
    last_active_profile_ = active_profile;
  }

  // This browsing session is still lasting.
  last_session_end_ = base::TimeTicks::Now();
}

void ProfileActivityMetricsRecorder::OnSessionEnded(
    base::TimeDelta session_length,
    base::TimeTicks session_end) {
  // If this call is emitted after OnProfileWillBeDestroyed, return
  // early. We already logged the session duration there.
  if (!running_session_profile_)
    return;

  // |session_length| can't be used here because it was measured across all
  // profiles.
  RecordProfileSessionDuration(running_session_profile_,
                               session_end - running_session_start_);
  DCHECK(
      profile_observation_.IsObservingSource(running_session_profile_.get()));
  profile_observation_.Reset();
  running_session_profile_ = nullptr;
  last_session_end_ = base::TimeTicks::Now();
}

void ProfileActivityMetricsRecorder::OnProfileWillBeDestroyed(
    Profile* profile) {
  DCHECK_EQ(profile, running_session_profile_);

  // The profile may be deleted without an OnSessionEnded call if, for
  // example, the browser shuts down.
  //
  // TODO(crbug.com/40700582): explore having
  // DesktopSessionDurationTracker call OnSessionEnded() when the
  // profile is destroyed. Remove this workaround if this is done.
  DCHECK(
      profile_observation_.IsObservingSource(running_session_profile_.get()));
  profile_observation_.Reset();
  running_session_profile_ = nullptr;
  last_active_profile_ = nullptr;
  last_session_end_ = base::TimeTicks::Now();
}

ProfileActivityMetricsRecorder::ProfileActivityMetricsRecorder() {
  BrowserList::AddObserver(this);
  metrics::DesktopSessionDurationTracker::Get()->AddObserver(this);
  action_callback_ = base::BindRepeating(
      &ProfileActivityMetricsRecorder::OnUserAction, base::Unretained(this));
  base::AddActionCallback(action_callback_);
}

ProfileActivityMetricsRecorder::~ProfileActivityMetricsRecorder() {
  BrowserList::RemoveObserver(this);
  metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
  base::RemoveActionCallback(action_callback_);
}

void ProfileActivityMetricsRecorder::OnUserAction(const std::string& action,
                                                  base::TimeTicks action_time) {
  RecordUserAction(running_session_profile_);
}
