// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include <unordered_set>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/federated_learning/floc_blocklist_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace federated_learning {

namespace {

constexpr size_t kMinHistoryDomainSizeToReportFlocId = 1;
constexpr base::TimeDelta kFlocScheduledUpdateInterval =
    base::TimeDelta::FromDays(1);
constexpr int kQueryHistoryWindowInDays = 7;
constexpr base::TimeDelta kSwaaNacAccountEnabledCachePeriod =
    base::TimeDelta::FromHours(12);

}  // namespace

FlocIdProviderImpl::FlocIdProviderImpl(
    syncer::SyncService* sync_service,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    FlocRemotePermissionService* floc_remote_permission_service,
    history::HistoryService* history_service,
    syncer::UserEventService* user_event_service)
    : sync_service_(sync_service),
      cookie_settings_(std::move(cookie_settings)),
      floc_remote_permission_service_(floc_remote_permission_service),
      history_service_(history_service),
      user_event_service_(user_event_service) {
  history_service->AddObserver(this);
  sync_service_->AddObserver(this);
  g_browser_process->floc_blocklist_service()->AddObserver(this);

  OnStateChanged(sync_service);

  if (g_browser_process->floc_blocklist_service()->BlocklistLoaded())
    OnBlocklistLoaded();
}

FlocIdProviderImpl::~FlocIdProviderImpl() = default;

std::string FlocIdProviderImpl::GetInterestCohortForJsApi(
    const url::Origin& requesting_origin,
    const net::SiteForCookies& site_for_cookies) const {
  // These checks could be / become unnecessary, as we are planning on
  // invalidating the |floc_id_| whenever a setting is disabled. Check them
  // anyway to be safe.
  if (!IsSyncHistoryEnabled() || !AreThirdPartyCookiesAllowed())
    return std::string();

  // Only allow floc access if cookie access is allowed.
  if (!cookie_settings_->IsCookieAccessAllowed(
          requesting_origin.GetURL(), site_for_cookies.RepresentativeUrl(),
          base::nullopt)) {
    return std::string();
  }

  if (!floc_id_.IsValid())
    return std::string();

  return floc_id_.ToString();
}

void FlocIdProviderImpl::OnComputeFlocCompleted(ComputeFlocTrigger trigger,
                                                ComputeFlocResult result) {
  DCHECK(floc_computation_in_progress_);
  floc_computation_in_progress_ = false;

  // Some recompute event came in when this computation was in progress. Ignore
  // this computation completely. Handle the pending one.
  if (pending_recompute_event_) {
    ComputeFlocTrigger recompute_trigger = pending_recompute_event_.value();
    pending_recompute_event_.reset();
    ComputeFloc(recompute_trigger);
    return;
  }

  LogFlocComputedEvent(trigger, result);
  floc_id_ = result.final_hash;

  // Abandon the scheduled task if any, and schedule a new compute-floc task
  // that is |kFlocScheduledUpdateInterval| from now.
  compute_floc_timer_.Start(
      FROM_HERE, kFlocScheduledUpdateInterval,
      base::BindOnce(&FlocIdProviderImpl::OnComputeFlocScheduledUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FlocIdProviderImpl::LogFlocComputedEvent(ComputeFlocTrigger trigger,
                                              const ComputeFlocResult& result) {
  if (!base::FeatureList::IsEnabled(features::kFlocIdComputedEventLogging))
    return;

  // Don't log if it's the 1st computation and sim_hash is not computed. This
  // is likely due to sync just gets enabled but some floc permission settings
  // are disabled. We don't want to mess up with the initial user event
  // messagings (and some sync integration tests would fail otherwise).
  if (trigger == ComputeFlocTrigger::kBrowserStart &&
      !result.sim_hash.IsValid()) {
    return;
  }

  auto specifics = std::make_unique<sync_pb::UserEventSpecifics>();
  specifics->set_event_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::UserEventSpecifics_FlocIdComputed* const floc_id_computed_event =
      specifics->mutable_floc_id_computed_event();

  sync_pb::UserEventSpecifics_FlocIdComputed_EventTrigger event_trigger;
  switch (trigger) {
    case ComputeFlocTrigger::kBrowserStart:
      event_trigger =
          sync_pb::UserEventSpecifics_FlocIdComputed_EventTrigger_NEW;
      break;
    case ComputeFlocTrigger::kScheduledUpdate:
      event_trigger =
          sync_pb::UserEventSpecifics_FlocIdComputed_EventTrigger_REFRESHED;
      break;
    case ComputeFlocTrigger::kHistoryDelete:
      event_trigger = sync_pb::
          UserEventSpecifics_FlocIdComputed_EventTrigger_HISTORY_DELETE;
      break;
  }

  floc_id_computed_event->set_event_trigger(event_trigger);

  if (result.sim_hash.IsValid())
    floc_id_computed_event->set_floc_id(result.sim_hash.ToUint64());

  user_event_service_->RecordUserEvent(std::move(specifics));
}

void FlocIdProviderImpl::Shutdown() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;

  if (history_service_)
    history_service_->RemoveObserver(this);
  history_service_ = nullptr;

  g_browser_process->floc_blocklist_service()->RemoveObserver(this);
}

void FlocIdProviderImpl::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // Set a pending event or override the existing one, that will get run when
  // the in-progress computation finishes.
  if (floc_computation_in_progress_) {
    DCHECK(first_floc_computation_triggered_);
    pending_recompute_event_ = ComputeFlocTrigger::kHistoryDelete;
    return;
  }

  if (!first_floc_computation_triggered_ || !floc_id_.IsValid())
    return;

  ComputeFloc(ComputeFlocTrigger::kHistoryDelete);
}

void FlocIdProviderImpl::OnBlocklistLoaded() {
  if (first_blocklist_loaded_seen_)
    return;

  first_blocklist_loaded_seen_ = true;

  MaybeTriggerFirstFlocComputation();
}

void FlocIdProviderImpl::OnStateChanged(syncer::SyncService* sync_service) {
  if (first_sync_history_enabled_seen_)
    return;

  if (!IsSyncHistoryEnabled())
    return;

  first_sync_history_enabled_seen_ = true;

  MaybeTriggerFirstFlocComputation();
}

void FlocIdProviderImpl::MaybeTriggerFirstFlocComputation() {
  if (first_floc_computation_triggered_)
    return;

  if (!first_sync_history_enabled_seen_ ||
      (base::FeatureList::IsEnabled(features::kFlocIdBlocklistFiltering) &&
       !first_blocklist_loaded_seen_)) {
    return;
  }

  ComputeFloc(ComputeFlocTrigger::kBrowserStart);
}

void FlocIdProviderImpl::OnComputeFlocScheduledUpdate() {
  // It's fine to skip the scheduled update as long as there's one in progress.
  // We won't be losing the recomputing frequency, as the in-progress one only
  // occurs sooner and when it finishes a new compute-floc task will be
  // scheduled.
  if (floc_computation_in_progress_)
    return;

  DCHECK(!pending_recompute_event_);

  ComputeFloc(ComputeFlocTrigger::kScheduledUpdate);
}

void FlocIdProviderImpl::ComputeFloc(ComputeFlocTrigger trigger) {
  DCHECK_NE(trigger == ComputeFlocTrigger::kBrowserStart,
            first_floc_computation_triggered_);
  DCHECK(!floc_computation_in_progress_);

  floc_computation_in_progress_ = true;
  first_floc_computation_triggered_ = true;

  auto compute_floc_completed_callback =
      base::BindOnce(&FlocIdProviderImpl::OnComputeFlocCompleted,
                     weak_ptr_factory_.GetWeakPtr(), trigger);

  CheckCanComputeFloc(
      base::BindOnce(&FlocIdProviderImpl::OnCheckCanComputeFlocCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(compute_floc_completed_callback)));
}

void FlocIdProviderImpl::CheckCanComputeFloc(CanComputeFlocCallback callback) {
  if (!IsSyncHistoryEnabled() || !AreThirdPartyCookiesAllowed()) {
    std::move(callback).Run(false);
    return;
  }

  IsSwaaNacAccountEnabled(std::move(callback));
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

bool FlocIdProviderImpl::IsSyncHistoryEnabled() const {
  syncer::SyncUserSettings* setting = sync_service_->GetUserSettings();
  DCHECK(setting);

  return sync_service_->IsSyncFeatureActive() &&
         sync_service_->GetActiveDataTypes().Has(
             syncer::HISTORY_DELETE_DIRECTIVES);
}

bool FlocIdProviderImpl::AreThirdPartyCookiesAllowed() const {
  return !cookie_settings_->ShouldBlockThirdPartyCookies();
}

void FlocIdProviderImpl::IsSwaaNacAccountEnabled(
    CanComputeFlocCallback callback) {
  if (!last_swaa_nac_account_enabled_query_time_.is_null() &&
      last_swaa_nac_account_enabled_query_time_ +
              kSwaaNacAccountEnabledCachePeriod >
          base::TimeTicks::Now()) {
    std::move(callback).Run(cached_swaa_nac_account_enabled_);
    return;
  }

  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "floc_id_provider_impl", "floc_remote_permission_service",
          R"(
        semantics {
          description:
            "Queries google to find out if user has enabled 'web and app "
            "activity' and 'ad personalization', and if the account type is "
            "NOT a child account. Those permission bits will be checked before "
            "computing the FLoC (Federated Learning of Cohorts) ID - an "
            "anonymous similarity hash value of user’s navigation history. "
            "This ensures that the FLoC ID is derived from data that Google "
            "already owns and the user has explicitly granted permission on "
            "what they will be used for."
          trigger:
            "This request is sent at each time a FLoC (Federated Learning of "
            "Cohorts) ID is to be computed. A FLoC ID is an anonymous "
            "similarity hash value of user’s navigation history. It'll be "
            "computed at the start of each browser profile session and will be "
            "refreshed every 24 hours during that session."
          data:
            "Google credentials if user is signed in."
        }
        policy {
            setting:
              "This feature cannot be disabled in settings, but disabling sync "
              "or third-party cookies will prevent it."
        })");

  floc_remote_permission_service_->QueryFlocPermission(
      base::BindOnce(&FlocIdProviderImpl::OnCheckSwaaNacAccountEnabledCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      partial_traffic_annotation);
}

void FlocIdProviderImpl::OnCheckSwaaNacAccountEnabledCompleted(
    CanComputeFlocCallback callback,
    bool enabled) {
  cached_swaa_nac_account_enabled_ = enabled;
  last_swaa_nac_account_enabled_query_time_ = base::TimeTicks::Now();
  std::move(callback).Run(enabled);
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
  for (const history::URLResult& url_result : results) {
    if (!url_result.publicly_routable())
      continue;

    domains.insert(net::registry_controlled_domains::GetDomainAndRegistry(
        url_result.url(),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  if (domains.size() < kMinHistoryDomainSizeToReportFlocId) {
    std::move(callback).Run(ComputeFlocResult());
    return;
  }

  ApplyAdditionalFiltering(std::move(callback),
                           FlocId::CreateFromHistory(domains));
}

void FlocIdProviderImpl::ApplyAdditionalFiltering(
    ComputeFlocCompletedCallback callback,
    const FlocId& sim_hash) {
  DCHECK(sim_hash.IsValid());

  if (base::FeatureList::IsEnabled(features::kFlocIdBlocklistFiltering) &&
      g_browser_process->floc_blocklist_service()->ShouldBlockFloc(
          sim_hash.ToUint64())) {
    std::move(callback).Run(ComputeFlocResult(sim_hash, FlocId()));
    return;
  }

  std::move(callback).Run(ComputeFlocResult(sim_hash, sim_hash));
}

}  // namespace federated_learning
