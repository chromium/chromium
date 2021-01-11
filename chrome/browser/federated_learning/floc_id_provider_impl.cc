// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include <unordered_set>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/federated_learning/floc_event_logger.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/federated_learning/features/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace federated_learning {

namespace {

constexpr int kQueryHistoryWindowInDays = 7;

// The placeholder sorting-lsh version when the sorting-lsh feature is disabled.
constexpr uint32_t kSortingLshVersionPlaceholder = 0;

// Checks whether we can keep using the previous floc. If so, write to
// |next_compute_delay| the time period we should wait until the floc needs to
// be recomputed.
bool ShouldKeepUsingPreviousFloc(const FlocId& last_floc,
                                 base::TimeDelta* next_compute_delay) {
  // The floc has never been computed. This could happen with a fresh profile,
  // or some early trigger conditions were never met (e.g. sorting-lsh file has
  // never been ready).
  if (last_floc.compute_time().is_null())
    return false;

  // The browser started with a kFlocIdFinchConfigVersion param different from
  // the param when floc was computed last time.
  //
  // TODO(yaoxia): Ideally we want to compare the entire version that also
  // includes the sorting-lsh version. We'll need to postpone those checks to
  // a point where an existing sorting-lsh file would have been loaded, i.e. not
  // too soon when the file is not ready yet, but not too late if the file
  // wouldn't arrive due to e.g. component updater issue.
  if (last_floc.finch_config_version() !=
      static_cast<uint32_t>(kFlocIdFinchConfigVersion.Get())) {
    return false;
  }

  base::TimeDelta presumed_next_compute_delay =
      kFlocIdScheduledUpdateInterval.Get() + last_floc.compute_time() -
      base::Time::Now();

  // The last floc has expired.
  if (presumed_next_compute_delay <= base::TimeDelta())
    return false;

  // This could happen if the machine time has changed since the last
  // computation. Return false in order to keep computing the floc at the
  // anticipated schedule rather than potentially stop computing for a very long
  // time.
  if (presumed_next_compute_delay >= 2 * kFlocIdScheduledUpdateInterval.Get())
    return false;

  *next_compute_delay = presumed_next_compute_delay;

  return true;
}

}  // namespace

FlocIdProviderImpl::FlocIdProviderImpl(
    PrefService* prefs,
    PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    std::unique_ptr<FlocEventLogger> floc_event_logger)
    : prefs_(prefs),
      privacy_sandbox_settings_(privacy_sandbox_settings),
      history_service_(history_service),
      floc_event_logger_(std::move(floc_event_logger)),
      floc_id_(FlocId::ReadFromPrefs(prefs_)) {
  history_service->AddObserver(this);
  g_browser_process->floc_sorting_lsh_clusters_service()->AddObserver(this);

  // If the previous floc has expired, invalidate it. The next computation will
  // be "immediate", i.e. will occur after we first observe that the SortingLSH
  // file is loaded; otherwise, keep using the last floc (which may still have
  // be invalid), and schedule a recompute event with the desired delay.
  base::TimeDelta next_compute_delay;
  if (ShouldKeepUsingPreviousFloc(floc_id_, &next_compute_delay)) {
    ScheduleFlocComputation(next_compute_delay);
  } else {
    floc_id_.InvalidateIdAndSaveToPrefs(prefs_);
  }

  if (g_browser_process->floc_sorting_lsh_clusters_service()
          ->IsSortingLshClustersFileReady()) {
    OnSortingLshClustersFileReady();
  }

  MaybeTriggerImmediateComputation();
}

FlocIdProviderImpl::~FlocIdProviderImpl() = default;

std::string FlocIdProviderImpl::GetInterestCohortForJsApi(
    const GURL& url,
    const base::Optional<url::Origin>& top_frame_origin) const {
  // Check the Privacy Sandbox general settings.
  if (!IsPrivacySandboxAllowed())
    return std::string();

  // Check the Privacy Sandbox context specific settings.
  if (!privacy_sandbox_settings_->IsFlocAllowed(url, top_frame_origin))
    return std::string();

  if (!floc_id_.IsValid())
    return std::string();

  return floc_id_.ToStringForJsApi();
}

void FlocIdProviderImpl::OnComputeFlocCompleted(ComputeFlocResult result) {
  DCHECK(floc_computation_in_progress_);
  floc_computation_in_progress_ = false;

  // History-delete event came in when this computation was in progress. Ignore
  // this computation completely and recompute.
  if (need_recompute_) {
    need_recompute_ = false;
    ComputeFloc();
    return;
  }

  LogFlocComputedEvent(result);

  floc_id_ = result.floc_id;
  floc_id_.SaveToPrefs(prefs_);

  ScheduleFlocComputation(kFlocIdScheduledUpdateInterval.Get());
}

void FlocIdProviderImpl::LogFlocComputedEvent(const ComputeFlocResult& result) {
  floc_event_logger_->LogFlocComputedEvent(
      FlocEventLogger::Event{result.sim_hash_computed, result.sim_hash,
                             result.floc_id.compute_time()});
}

void FlocIdProviderImpl::Shutdown() {
  if (history_service_)
    history_service_->RemoveObserver(this);
  history_service_ = nullptr;

  g_browser_process->floc_sorting_lsh_clusters_service()->RemoveObserver(this);
}

void FlocIdProviderImpl::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // Set the |need_recompute_| flag so that we will recompute the floc
  // immediately after the in-progress one finishes, so as to avoid potential
  // data races.
  if (floc_computation_in_progress_) {
    need_recompute_ = true;
    return;
  }

  if (!floc_id_.IsValid())
    return;

  // Only invalidate the floc if it's delete-all or if the time range overlaps
  // with the time range of the history used to compute the current floc.
  if (!deletion_info.IsAllHistory() && !deletion_info.time_range().IsValid()) {
    return;
  }

  if (deletion_info.time_range().begin() > floc_id_.history_end_time() ||
      deletion_info.time_range().end() < floc_id_.history_begin_time()) {
    return;
  }

  // We log the invalidation event although it's technically not a recompute.
  // It'd give us a better idea how often the floc is invalidated due to
  // history-delete.
  LogFlocComputedEvent(ComputeFlocResult());

  floc_id_.InvalidateIdAndSaveToPrefs(prefs_);
}

void FlocIdProviderImpl::OnSortingLshClustersFileReady() {
  if (first_sorting_lsh_file_ready_seen_)
    return;

  first_sorting_lsh_file_ready_seen_ = true;

  MaybeTriggerImmediateComputation();
}

void FlocIdProviderImpl::MaybeTriggerImmediateComputation() {
  // If the floc computation is neither in progress nor scheduled, it means we
  // want to trigger an immediate computation as soon as the sorting-lsh file is
  // loaded.
  if (floc_computation_in_progress_ || compute_floc_timer_.IsRunning())
    return;

  if (!first_sorting_lsh_file_ready_seen_ &&
      base::FeatureList::IsEnabled(kFlocIdSortingLshBasedComputation)) {
    return;
  }

  ComputeFloc();
}

void FlocIdProviderImpl::ComputeFloc() {
  DCHECK(!floc_computation_in_progress_);

  floc_computation_in_progress_ = true;

  auto compute_floc_completed_callback =
      base::BindOnce(&FlocIdProviderImpl::OnComputeFlocCompleted,
                     weak_ptr_factory_.GetWeakPtr());

  CheckCanComputeFloc(
      base::BindOnce(&FlocIdProviderImpl::OnCheckCanComputeFlocCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(compute_floc_completed_callback)));
}

void FlocIdProviderImpl::CheckCanComputeFloc(CanComputeFlocCallback callback) {
  if (!IsPrivacySandboxAllowed()) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void FlocIdProviderImpl::OnCheckCanComputeFlocCompleted(
    ComputeFlocCompletedCallback callback,
    bool can_compute_floc) {
  if (!can_compute_floc) {
    std::move(callback).Run(ComputeFlocResult());
    return;
  }

  GetRecentlyVisitedURLs(
      base::BindOnce(&FlocIdProviderImpl::OnGetRecentlyVisitedURLsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool FlocIdProviderImpl::IsPrivacySandboxAllowed() const {
  return privacy_sandbox_settings_->IsPrivacySandboxAllowed();
}

void FlocIdProviderImpl::GetRecentlyVisitedURLs(
    GetRecentlyVisitedURLsCallback callback) {
  history::QueryOptions options;
  options.SetRecentDayRange(kQueryHistoryWindowInDays);
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  history_service_->QueryHistory(base::string16(), options, std::move(callback),
                                 &history_task_tracker_);
}

void FlocIdProviderImpl::OnGetRecentlyVisitedURLsCompleted(
    ComputeFlocCompletedCallback callback,
    history::QueryResults results) {
  std::unordered_set<std::string> domains;

  base::Time history_begin_time = base::Time::Max();
  base::Time history_end_time = base::Time::Min();

  for (const history::URLResult& url_result : results) {
    if (!url_result.floc_allowed())
      continue;

    if (url_result.visit_time() < history_begin_time)
      history_begin_time = url_result.visit_time();

    if (url_result.visit_time() > history_end_time)
      history_end_time = url_result.visit_time();

    domains.insert(net::registry_controlled_domains::GetDomainAndRegistry(
        url_result.url(),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  if (domains.size() <
      static_cast<size_t>(kFlocIdMinimumHistoryDomainSizeRequired.Get())) {
    std::move(callback).Run(ComputeFlocResult());
    return;
  }

  ApplySortingLshPostProcessing(std::move(callback),
                                FlocId::SimHashHistory(domains),
                                history_begin_time, history_end_time);
}

void FlocIdProviderImpl::ApplySortingLshPostProcessing(
    ComputeFlocCompletedCallback callback,
    uint64_t sim_hash,
    base::Time history_begin_time,
    base::Time history_end_time) {
  if (!base::FeatureList::IsEnabled(kFlocIdSortingLshBasedComputation)) {
    std::move(callback).Run(ComputeFlocResult(
        sim_hash, FlocId(sim_hash, history_begin_time, history_end_time,
                         kSortingLshVersionPlaceholder)));
    return;
  }

  g_browser_process->floc_sorting_lsh_clusters_service()->ApplySortingLsh(
      sim_hash,
      base::BindOnce(&FlocIdProviderImpl::DidApplySortingLshPostProcessing,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     sim_hash, history_begin_time, history_end_time));
}

void FlocIdProviderImpl::DidApplySortingLshPostProcessing(
    ComputeFlocCompletedCallback callback,
    uint64_t sim_hash,
    base::Time history_begin_time,
    base::Time history_end_time,
    base::Optional<uint64_t> final_hash,
    base::Version version) {
  if (!final_hash) {
    std::move(callback).Run(ComputeFlocResult(sim_hash, FlocId()));
    return;
  }

  std::move(callback).Run(ComputeFlocResult(
      sim_hash, FlocId(final_hash.value(), history_begin_time, history_end_time,
                       version.components().front())));
}

void FlocIdProviderImpl::ScheduleFlocComputation(base::TimeDelta delay) {
  compute_floc_timer_.Start(FROM_HERE, delay,
                            base::BindOnce(&FlocIdProviderImpl::ComputeFloc,
                                           weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace federated_learning
