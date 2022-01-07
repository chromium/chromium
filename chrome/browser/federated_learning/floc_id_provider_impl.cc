// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include <unordered_set>

#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/federated_learning/floc_event_logger.h"
#include "chrome/browser/ui/webui/federated_learning/floc_internals.mojom.h"
#include "components/federated_learning/features/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace federated_learning {

namespace {

constexpr int kQueryHistoryWindowInDays = 7;

struct StartupComputeDecision {
  bool invalidate_existing_floc = true;
  // Will be absl::nullopt if should recompute immediately.
  absl::optional<base::TimeDelta> next_compute_delay;
};

// Determine whether we can keep using the previous floc and/or when should the
// next floc computation occur.
StartupComputeDecision GetStartupComputeDecision(
    const FlocId& last_floc,
    base::Time floc_accessible_since) {
  // The floc has never been computed. This could happen with a fresh profile,
  // or some early trigger conditions were never met (e.g. sorting-lsh file has
  // never been ready).
  if (last_floc.compute_time().is_null()) {
    return StartupComputeDecision{.invalidate_existing_floc = true,
                                  .next_compute_delay = absl::nullopt};
  }

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
    return StartupComputeDecision{.invalidate_existing_floc = true,
                                  .next_compute_delay = absl::nullopt};
  }

  base::TimeDelta presumed_next_compute_delay =
      kFlocIdScheduledUpdateInterval.Get() + last_floc.compute_time() -
      base::Time::Now();

  // The last floc has expired.
  if (presumed_next_compute_delay <= base::TimeDelta()) {
    return StartupComputeDecision{.invalidate_existing_floc = true,
                                  .next_compute_delay = absl::nullopt};
  }

  // This could happen if the machine time has changed since the last
  // computation. Recompute immediately to align with the expected schedule
  // rather than potentially stop computing for a very long time.
  if (presumed_next_compute_delay >= 2 * kFlocIdScheduledUpdateInterval.Get()) {
    return StartupComputeDecision{.invalidate_existing_floc = true,
                                  .next_compute_delay = absl::nullopt};
  }

  // Normally "floc_accessible_since <= last_floc.history_begin_time()" is an
  // invariant, because we monitor its update and reset the floc accordingly.
  // But "Clear on exit" may cause a cookie deletion on shutdown (practically on
  // startup) that will reset floc_accessible_since to base::Time::Now and
  // break the invariant on startup.
  if (floc_accessible_since > last_floc.history_begin_time()) {
    return StartupComputeDecision{
        .invalidate_existing_floc = true,
        .next_compute_delay = presumed_next_compute_delay};
  }

  return StartupComputeDecision{
      .invalidate_existing_floc = false,
      .next_compute_delay = presumed_next_compute_delay};
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
  privacy_sandbox_settings->AddObserver(this);
  history_service_observation_.Observe(history_service);
  g_browser_process->floc_sorting_lsh_clusters_service()->AddObserver(this);

  StartupComputeDecision decision = GetStartupComputeDecision(
      floc_id_, privacy_sandbox_settings->FlocDataAccessibleSince());

  // Invalidate the expired floc and/or assign a better invalid reason.
  if (decision.invalidate_existing_floc) {
    // We only switch from one invalid status to another invalid status when
    // the next cohort computation becomes ready to run (i.e.
    // kInvalidWaitingToStart).
    FlocId::Status maybe_new_status =
        decision.next_compute_delay.has_value()
            ? (floc_id_.status() == FlocId::Status::kValid)
                  ? FlocId::Status::kInvalidReset
                  : floc_id_.status()
            : FlocId::Status::kInvalidWaitingToStart;

    floc_id_.UpdateStatusAndSaveToPrefs(prefs_, maybe_new_status);
  }

  // Schedule the next floc computation if a delay is needed; otherwise, the
  // next computation will occur as soon as the sorting-lsh file is loaded.
  if (decision.next_compute_delay.has_value())
    ScheduleFlocComputation(decision.next_compute_delay.value());

  if (g_browser_process->floc_sorting_lsh_clusters_service()
          ->IsSortingLshClustersFileReady()) {
    OnSortingLshClustersFileReady();
  }
}

FlocIdProviderImpl::~FlocIdProviderImpl() {
  g_browser_process->floc_sorting_lsh_clusters_service()->RemoveObserver(this);
}

blink::mojom::InterestCohortPtr FlocIdProviderImpl::GetInterestCohortForJsApi(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin) const {
  // Check the general floc setting.
  if (!IsFlocAllowed())
    return blink::mojom::InterestCohort::New();

  // Check the context specific floc setting.
  if (!privacy_sandbox_settings_->IsFlocAllowedForContext(url,
                                                          top_frame_origin)) {
    return blink::mojom::InterestCohort::New();
  }

  if (!floc_id_.IsValid())
    return blink::mojom::InterestCohort::New();

  return floc_id_.ToInterestCohortForJsApi();
}

mojom::WebUIFlocStatusPtr FlocIdProviderImpl::GetFlocStatusForWebUi() const {
  mojom::WebUIFlocStatusPtr status = mojom::WebUIFlocStatus::New();

  if (floc_id_.IsValid()) {
    status->id = base::NumberToString(floc_id_.ToUint64());
    status->version = base::StrCat(
        {"chrome.", base::NumberToString(floc_id_.finch_config_version()), ".",
         base::NumberToString(floc_id_.sorting_lsh_version())});
  }

  status->compute_time = floc_id_.compute_time();

  status->feature_pages_with_ad_resources_default_included_in_floc_computation =
      base::FeatureList::IsEnabled(
          kFlocPagesWithAdResourcesDefaultIncludedInFlocComputation);
  status->feature_interest_cohort_api_origin_trial =
      base::FeatureList::IsEnabled(
          blink::features::kInterestCohortAPIOriginTrial);
  status->feature_interest_cohort_feature_policy = base::FeatureList::IsEnabled(
      blink::features::kInterestCohortFeaturePolicy);

  status->feature_param_scheduled_update_interval =
      kFlocIdScheduledUpdateInterval.Get();
  status->feature_param_minimum_history_domain_size_required =
      kFlocIdMinimumHistoryDomainSizeRequired.Get();
  status->feature_param_finch_config_version = kFlocIdFinchConfigVersion.Get();

  return status;
}

void FlocIdProviderImpl::MaybeRecordFlocToUkm(ukm::SourceId source_id) {
  if (!need_ukm_recording_)
    return;

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::FlocPageLoad builder(source_id);

  if (floc_id_.IsValid())
    builder.SetFlocId(floc_id_.ToUint64());

  builder.Record(ukm_recorder->Get());

  need_ukm_recording_ = false;
}

base::Time FlocIdProviderImpl::GetApproximateNextComputeTime() const {
  if (!compute_floc_timer_.IsRunning())
    return base::Time::Now();

  // Convert the TimeTicks type the timer provides to base::Time.
  return base::Time::Now() +
         (compute_floc_timer_.desired_run_time() - base::TimeTicks::Now());
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

  need_ukm_recording_ = true;

  ScheduleFlocComputation(kFlocIdScheduledUpdateInterval.Get());
}

void FlocIdProviderImpl::LogFlocComputedEvent(const ComputeFlocResult& result) {
  floc_event_logger_->LogFlocComputedEvent(
      FlocEventLogger::Event{result.sim_hash_computed, result.sim_hash,
                             result.floc_id.compute_time()});
}

void FlocIdProviderImpl::Shutdown() {
  privacy_sandbox_settings_->RemoveObserver(this);
  history_service_observation_.Reset();
  g_browser_process->floc_sorting_lsh_clusters_service()->RemoveObserver(this);
}

void FlocIdProviderImpl::OnFlocDataAccessibleSinceUpdated(
    bool reset_compute_timer) {
  // Set the |need_recompute_| flag so that we will recompute the floc
  // immediately after the in-progress one finishes, so as to avoid potential
  // data races. This function maybe have been called in response to a user
  // deliberately resetting floc, in this case the recomputed floc should be
  // invalid as the floc-accessible timestamp was just updated to now. The
  // floc computation is fast so it's exceedingly unlikely to populate enough
  // history for the recomputed ID to be valid between one floc calculation and
  // the next.
  if (floc_computation_in_progress_) {
    need_recompute_ = true;
    return;
  }

  // Clear any pending computes and re-schedule if requested.
  if (reset_compute_timer) {
    compute_floc_timer_.AbandonAndStop();
    ScheduleFlocComputation(kFlocIdScheduledUpdateInterval.Get());
    floc_id_.ResetComputeTimeAndSaveToPrefs(base::Time::Now(), prefs_);
  }

  // Note: we only invalidate the floc rather than recomputing, because we don't
  // want the floc to change more frequently than the scheduled update rate.

  // No-op if the floc is already invalid.
  if (!floc_id_.IsValid())
    return;

  // Invalidate the floc if the new floc-accessible-since time is greater than
  // the begin time of the history used to compute the current floc.
  if (privacy_sandbox_settings_->FlocDataAccessibleSince() >
      floc_id_.history_begin_time()) {
    floc_id_.UpdateStatusAndSaveToPrefs(prefs_, FlocId::Status::kInvalidReset);
  }
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
  LogFlocComputedEvent(
      ComputeFlocResult(FlocId::Status::kInvalidHistoryDeleted));

  floc_id_.UpdateStatusAndSaveToPrefs(prefs_,
                                      FlocId::Status::kInvalidHistoryDeleted);
}

void FlocIdProviderImpl::OnSortingLshClustersFileReady() {
  // If the floc computation is happening now or is scheduled, no-op; otherwise,
  // we want to trigger a computation as soon as the sorting-lsh file is loaded.
  if (floc_computation_in_progress_ || compute_floc_timer_.IsRunning())
    return;

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
  if (!IsFlocAllowed()) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void FlocIdProviderImpl::OnCheckCanComputeFlocCompleted(
    ComputeFlocCompletedCallback callback,
    bool can_compute_floc) {
  if (!can_compute_floc) {
    std::move(callback).Run(
        ComputeFlocResult(FlocId::Status::kInvalidDisallowedByUserSettings));
    return;
  }

  GetRecentlyVisitedURLs(
      base::BindOnce(&FlocIdProviderImpl::OnGetRecentlyVisitedURLsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool FlocIdProviderImpl::IsFlocAllowed() const {
  return privacy_sandbox_settings_->IsFlocAllowed();
}

void FlocIdProviderImpl::GetRecentlyVisitedURLs(
    GetRecentlyVisitedURLsCallback callback) {
  base::Time now = base::Time::Now();

  history::QueryOptions options;
  options.begin_time =
      std::max(privacy_sandbox_settings_->FlocDataAccessibleSince(),
               now - base::Days(kQueryHistoryWindowInDays));
  options.end_time = now;
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  history_service_->QueryHistory(std::u16string(), options, std::move(callback),
                                 &history_task_tracker_);
}

void FlocIdProviderImpl::OnGetRecentlyVisitedURLsCompleted(
    ComputeFlocCompletedCallback callback,
    history::QueryResults results) {
  std::unordered_set<std::string> domains;

  base::Time history_begin_time = base::Time::Max();
  base::Time history_end_time = base::Time::Min();

  for (const history::URLResult& url_result : results) {
    if (!(url_result.content_annotations().annotation_flags &
          history::VisitContentAnnotationFlag::kFlocEligibleRelaxed)) {
      continue;
    }

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
    std::move(callback).Run(ComputeFlocResult(
        FlocId::Status::kInvalidNotEnoughElgibleHistoryDomains));
    return;
  }

  uint64_t sim_hash = FlocId::SimHashHistory(domains);

  // Apply the sorting-lsh post processing to compute the final versioned floc.
  // The final floc may be invalid if the file is corrupted or the floc is in
  // the block list.
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
    absl::optional<uint64_t> final_hash,
    base::Version version) {
  if (!final_hash) {
    std::move(callback).Run(ComputeFlocResult(
        sim_hash, FlocId::CreateInvalid(FlocId::Status::kInvalidBlocked)));
    return;
  }

  std::move(callback).Run(ComputeFlocResult(
      sim_hash,
      FlocId::CreateValid(final_hash.value(), history_begin_time,
                          history_end_time, version.components().front())));
}

void FlocIdProviderImpl::ScheduleFlocComputation(base::TimeDelta delay) {
  compute_floc_timer_.Start(FROM_HERE, delay,
                            base::BindOnce(&FlocIdProviderImpl::ComputeFloc,
                                           weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace federated_learning
