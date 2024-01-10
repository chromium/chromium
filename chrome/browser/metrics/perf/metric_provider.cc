// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_provider.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/metrics_proto/device_state.pb.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {
namespace {

// Name prefix of the histogram that counts the number of reports uploaded by a
// metric provider.
const char kUploadCountHistogramPrefix[] = "ChromeOS.CWP.Upload";

// Name prefix of the histogram that tracks the various outcomes of saving the
// collected profile to local cache.
const char kRecordOutcomeHistogramPrefix[] = "ChromeOS.CWP.Record";

// An upper bound on the count of reports expected to be uploaded by an UMA
// callback.
const int kMaxValueUploadReports = 10;

// The MD5 prefix to replace the original comm_md5_prefix of COMM events in perf
// data proto, if necessary. We used string "<redacted>" to compute this MD5
// prefix.
const uint64_t kRedactedCommMd5Prefix = 0xee1f021828a1fcbc;

// This function modifies the comm_md5_prefix of all the COMM events in the
// given perf data proto by replacing it with the md5 prefix of an artificial
// string.
void RedactCommMd5Prefixes(PerfDataProto* proto) {
  for (PerfDataProto::PerfEvent& event : *proto->mutable_events()) {
    if (event.has_comm_event()) {
      event.mutable_comm_event()->set_comm_md5_prefix(kRedactedCommMd5Prefix);
    }
  }
}

ThermalState ToProtoThermalStateEnum(
    base::PowerThermalObserver::DeviceThermalState state) {
  switch (state) {
    case base::PowerThermalObserver::DeviceThermalState::kUnknown:
      return THERMAL_STATE_UNKNOWN;
    case base::PowerThermalObserver::DeviceThermalState::kNominal:
      return THERMAL_STATE_NOMINAL;
    case base::PowerThermalObserver::DeviceThermalState::kFair:
      return THERMAL_STATE_FAIR;
    case base::PowerThermalObserver::DeviceThermalState::kSerious:
      return THERMAL_STATE_SERIOUS;
    case base::PowerThermalObserver::DeviceThermalState::kCritical:
      return THERMAL_STATE_CRITICAL;
  }
}

}  // namespace

using MetricCollector = internal::MetricCollector;

MetricProvider::MetricProvider(std::unique_ptr<MetricCollector> collector,
                               ProfileManager* profile_manager)
    : upload_uma_histogram_(std::string(kUploadCountHistogramPrefix) +
                            collector->ToolName()),
      record_uma_histogram_(std::string(kRecordOutcomeHistogramPrefix) +
                            collector->ToolName()),
      // Run the collector at a higher priority to enable fast triggering of
      // profile collections. In particular, we want fast triggering when
      // jankiness is detected, but even random based periodic collection
      // benefits from a higher priority, to avoid biasing the collection to
      // times when the system is not busy. The work performed on the dedicated
      // sequence is short and infrequent. Expensive parsing operations are
      // executed asynchronously on the thread pool.
      collector_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      metric_collector_(std::move(collector)),
      profile_manager_(profile_manager),
      weak_factory_(this) {
  metric_collector_->set_profile_done_callback(base::BindRepeating(
      &MetricProvider::OnProfileDone, weak_factory_.GetWeakPtr()));
}

MetricProvider::~MetricProvider() {
  // Destroy the metric_collector_ on the collector sequence.
  collector_task_runner_->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(metric_collector_)));
}

void MetricProvider::Init() {
  // It is safe to use base::Unretained to post tasks to the metric_collector_
  // on the collector sequence, since we control its lifetime. Any tasks
  // posted to it are bound to run before we destroy it on the collector
  // sequence.
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::Init,
                                base::Unretained(metric_collector_.get())));
}

bool MetricProvider::GetSampledProfiles(
    std::vector<SampledProfile>* sampled_profiles) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (cached_profile_data_.empty()) {
    base::UmaHistogramExactLinear(upload_uma_histogram_, 0,
                                  kMaxValueUploadReports);
    return false;
  }

  base::UmaHistogramExactLinear(upload_uma_histogram_,
                                cached_profile_data_.size(),
                                kMaxValueUploadReports);
  sampled_profiles->insert(
      sampled_profiles->end(),
      std::make_move_iterator(cached_profile_data_.begin()),
      std::make_move_iterator(cached_profile_data_.end()));
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::ResetCachedDataSize,
                                base::Unretained(metric_collector_.get())));
  cached_profile_data_.clear();
  return true;
}

void MetricProvider::OnUserLoggedIn() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::TimeTicks now = base::TimeTicks::Now();
  collector_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricCollector::RecordUserLogin,
                     base::Unretained(metric_collector_.get()), now));
}

void MetricProvider::Deactivate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Notifies the collector to turn off the timer. Does not delete any data that
  // was already collected and stored in |cached_profile_data|.
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::StopTimer,
                                base::Unretained(metric_collector_.get())));
}

void MetricProvider::SuspendDone(base::TimeDelta sleep_duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::ScheduleSuspendDoneCollection,
                                base::Unretained(metric_collector_.get()),
                                sleep_duration));
}

void MetricProvider::OnSessionRestoreDone(int num_tabs_restored) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  collector_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetricCollector::ScheduleSessionRestoreCollection,
                     base::Unretained(metric_collector_.get()),
                     num_tabs_restored));
}

// static
void MetricProvider::OnProfileDone(
    base::WeakPtr<MetricProvider> provider,
    std::unique_ptr<SampledProfile> sampled_profile) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MetricProvider::AddProfileToCache, provider,
                                std::move(sampled_profile)));
}

void MetricProvider::OnJankStarted() {
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::OnJankStarted,
                                base::Unretained(metric_collector_.get())));
}

void MetricProvider::OnJankStopped() {
  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::OnJankStopped,
                                base::Unretained(metric_collector_.get())));
}

void MetricProvider::EnableRecording() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  recording_enabled_ = true;
}

void MetricProvider::DisableRecording() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  recording_enabled_ = false;
}

MetricProvider::RecordAttemptStatus MetricProvider::AppSyncStateForUserProfile(
    Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return RecordAttemptStatus::kSyncServiceUnavailable;
  syncer::SyncUserSettings* sync_settings = sync_service->GetUserSettings();

  if (!sync_service->IsSyncFeatureEnabled())
    return RecordAttemptStatus::kChromeSyncFeatureDisabled;

  if (!sync_settings->GetSelectedOsTypes().Has(
          syncer::UserSelectableOsType::kOsApps))
    return RecordAttemptStatus::kOSAppSyncDisabled;
  return RecordAttemptStatus::kAppSyncEnabled;
}

// Check the current state of App Sync in the settings. This is done by getting
// all currently fully initialized profiles and reading the sync settings from
// them.
MetricProvider::RecordAttemptStatus MetricProvider::GetAppSyncState() {
  if (!profile_manager_)
    return RecordAttemptStatus::kProfileManagerUnset;

  std::vector<Profile*> profiles = profile_manager_->GetLoadedProfiles();
  // Tracks the number of user profiles initialized on Chrome OS other than the
  // Default profile.
  int user_profile_count = 0;

  for (Profile* profile : profiles) {
    // The Default profile, lock screen app profile and lock screen profile are
    // all not regular user profiles on Chrome OS. They always disable sync and
    // we would skip them.
    if (!ash::ProfileHelper::IsUserProfile(profile))
      continue;
    auto app_sync_state = AppSyncStateForUserProfile(profile);
    if (app_sync_state != RecordAttemptStatus::kAppSyncEnabled)
      return app_sync_state;
    user_profile_count++;
  }

  if (user_profile_count == 0)
    return RecordAttemptStatus::kNoLoadedProfile;

  return RecordAttemptStatus::kAppSyncEnabled;
}

void MetricProvider::AddProfileToCache(
    std::unique_ptr<SampledProfile> sampled_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!recording_enabled_) {
    base::UmaHistogramEnumeration(record_uma_histogram_,
                                  RecordAttemptStatus::kRecordingDisabled);
    return;
  }

  // For privacy reasons, Chrome can not collect Android app names that may be
  // present in the perf data, unless the user consent to enabling App Sync.
  // Therefore, if the user does not enable App Sync, we redact comm_md5_prefix
  // in all COMM events of perf data proto, so these MD5 prefixes can not be
  // used to recover Android app names. We perform the check on App Sync here
  // because the procedure to get the user profile (from which sync settings can
  // be obtained) must execute on the UI thread.
  auto app_sync_state = GetAppSyncState();
  base::UmaHistogramEnumeration(record_uma_histogram_, app_sync_state);
  if (app_sync_state != RecordAttemptStatus::kAppSyncEnabled)
    RedactCommMd5Prefixes(sampled_profile->mutable_perf_data());

  // Add the device thermal state and cpu speed limit to the profile.
  sampled_profile->set_thermal_state(ToProtoThermalStateEnum(thermal_state_));
  sampled_profile->set_cpu_speed_limit_percent(cpu_speed_limit_percent_);

  collector_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetricCollector::AddCachedDataDelta,
                                base::Unretained(metric_collector_.get()),
                                sampled_profile->ByteSize()));
  cached_profile_data_.resize(cached_profile_data_.size() + 1);
  cached_profile_data_.back().Swap(sampled_profile.get());

  if (!cache_updated_callback_.is_null())
    cache_updated_callback_.Run();
}

}  // namespace metrics
