// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/core/hint_cache.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/hints_fetcher_factory.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_filter.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// The component version used with a manual config. This ensures that any hint
// component received from the Optimization Hints component on a subsequent
// startup will have a newer version than it.
constexpr char kManualConfigComponentVersion[] = "0.0.0";

// Provides a random time delta in seconds between |kFetchRandomMinDelay| and
// |kFetchRandomMaxDelay|.
base::TimeDelta RandomFetchDelay() {
  return base::TimeDelta::FromSeconds(base::RandInt(
      optimization_guide::features::ActiveTabsHintsFetchRandomMinDelaySecs(),
      optimization_guide::features::ActiveTabsHintsFetchRandomMaxDelaySecs()));
}

void MaybeRunUpdateClosure(base::OnceClosure update_closure) {
  if (update_closure)
    std::move(update_closure).Run();
}

// Returns whether the particular component version can be processed, and if it
// can be, locks the semaphore (in the form of a pref) to signal that the
// processing of this particular version has started.
bool CanProcessComponentVersion(PrefService* pref_service,
                                const base::Version& version) {
  DCHECK(version.IsValid());

  const std::string previous_attempted_version_string = pref_service->GetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion);
  if (!previous_attempted_version_string.empty()) {
    const base::Version previous_attempted_version =
        base::Version(previous_attempted_version_string);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in hints processing pref";
      // Clear pref for fresh start next time.
      pref_service->ClearPref(
          optimization_guide::prefs::kPendingHintsProcessingVersion);
      return false;
    }
    if (previous_attempted_version.CompareTo(version) == 0) {
      // Previously attempted same version without completion.
      return false;
    }
  }

  // Write config version to pref.
  pref_service->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion,
      version.GetString());
  return true;
}

// Returns whether |optimization_type| is whitelisted by |optimizations|. If
// it is whitelisted, this will return true and |optimization_metadata| will be
// populated with the metadata provided by the hint, if applicable. If
// |page_hint| is not provided or |optimization_type| is not whitelisted, this
// will return false.
bool IsOptimizationTypeAllowed(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::Optimization>& optimizations,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata,
    base::Optional<uint64_t>* tuning_version) {
  DCHECK(tuning_version);
  *tuning_version = base::nullopt;

  for (const auto& optimization : optimizations) {
    if (optimization_type != optimization.optimization_type())
      continue;

    if (optimization.has_tuning_version()) {
      *tuning_version = optimization.tuning_version();

      if (optimization.tuning_version() == UINT64_MAX) {
        // UINT64_MAX is the sentinel value indicating that the optimization
        // should not be served and was only added to the list for metrics
        // purposes.
        return false;
      }
    }

    // We found an optimization that can be applied. Populate optimization
    // metadata if applicable and return.
    if (optimization_metadata) {
      switch (optimization.metadata_case()) {
        case optimization_guide::proto::Optimization::kPerformanceHintsMetadata:
          optimization_metadata->set_performance_hints_metadata(
              optimization.performance_hints_metadata());
          break;
        case optimization_guide::proto::Optimization::kPublicImageMetadata:
          optimization_metadata->set_public_image_metadata(
              optimization.public_image_metadata());
          break;
        case optimization_guide::proto::Optimization::kLoadingPredictorMetadata:
          optimization_metadata->set_loading_predictor_metadata(
              optimization.loading_predictor_metadata());
          break;
        case optimization_guide::proto::Optimization::kAnyMetadata:
          optimization_metadata->set_any_metadata(optimization.any_metadata());
          break;
        case optimization_guide::proto::Optimization::METADATA_NOT_SET:
          // Some optimization types do not have metadata, make sure we do not
          // DCHECK.
          break;
      }
    }
    return true;
  }

  return false;
}

// Logs an OptimizationAutotuning event for the navigation with |navigation_id|,
// if |navigation_id| and |tuning_version| are non-null.
void MaybeLogOptimizationAutotuningUKMForNavigation(
    base::Optional<int64_t> navigation_id,
    optimization_guide::proto::OptimizationType optimization_type,
    base::Optional<int64_t> tuning_version) {
  if (!navigation_id || !tuning_version) {
    // Only log if we can correlate the tuning event with a navigation.
    return;
  }

  ukm::SourceId ukm_source_id =
      ukm::ConvertToSourceId(*navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::OptimizationGuideAutotuning builder(ukm_source_id);
  builder.SetOptimizationType(optimization_type)
      .SetTuningVersion(*tuning_version)
      .Record(ukm::UkmRecorder::Get());
}

// Util class for recording whether a hints fetch race against the current
// navigation was attempted. The result is recorded when it goes out of scope
// and its destructor is called.
class ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder {
 public:
  explicit ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder(
      content::NavigationHandle* navigation_handle)
      : race_attempt_status_(
            optimization_guide::RaceNavigationFetchAttemptStatus::kUnknown),
        navigation_data_(
            OptimizationGuideNavigationData::GetFromNavigationHandle(
                navigation_handle)) {}

  ~ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder() {
    DCHECK_NE(race_attempt_status_,
              optimization_guide::RaceNavigationFetchAttemptStatus::kUnknown);
    DCHECK_NE(
        race_attempt_status_,
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        race_attempt_status_);
    if (navigation_data_)
      navigation_data_->set_hints_fetch_attempt_status(race_attempt_status_);
  }

  void set_race_attempt_status(
      optimization_guide::RaceNavigationFetchAttemptStatus
          race_attempt_status) {
    race_attempt_status_ = race_attempt_status;
  }

 private:
  optimization_guide::RaceNavigationFetchAttemptStatus race_attempt_status_;
  OptimizationGuideNavigationData* navigation_data_;
};

// Returns true if the optimization type should be ignored when is newly
// registered as the optimization type is likely launched.
bool ShouldIgnoreNewlyRegisteredOptimizationType(
    optimization_guide::proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case optimization_guide::proto::NOSCRIPT:
    case optimization_guide::proto::RESOURCE_LOADING:
    case optimization_guide::proto::LITE_PAGE_REDIRECT:
    case optimization_guide::proto::DEFER_ALL_SCRIPT:
      return true;
    default:
      return false;
  }
  return false;
}

// Reads component file and parses it into a Configuration proto. Should not be
// called on the UI thread.
std::unique_ptr<optimization_guide::proto::Configuration> ReadComponentFile(
    const optimization_guide::HintsComponentInfo& info) {
  optimization_guide::ProcessHintsComponentResult out_result;
  std::unique_ptr<optimization_guide::proto::Configuration> config =
      optimization_guide::ProcessHintsComponent(info, &out_result);
  if (!config) {
    optimization_guide::RecordProcessHintsComponentResult(out_result);
    return nullptr;
  }

  // Do not record the process hints component result for success cases until
  // we processed all of the hints and filters in it.
  return config;
}

}  // namespace

OptimizationGuideHintsManager::OptimizationGuideHintsManager(
    Profile* profile,
    PrefService* pref_service,
    optimization_guide::OptimizationGuideStore* hint_store,
    optimization_guide::TopHostProvider* top_host_provider,
    optimization_guide::TabUrlProvider* tab_url_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile),
      pref_service_(pref_service),
      hint_cache_(std::make_unique<optimization_guide::HintCache>(
          hint_store,
          optimization_guide::features::MaxHostKeyedHintCacheSize())),
      page_navigation_hints_fetchers_(
          optimization_guide::features::MaxConcurrentPageNavigationFetches()),
      hints_fetcher_factory_(
          std::make_unique<optimization_guide::HintsFetcherFactory>(
              url_loader_factory,
              optimization_guide::features::
                  GetOptimizationGuideServiceGetHintsURL(),
              pref_service,
              content::GetNetworkConnectionTracker())),
      external_app_packages_approved_for_fetch_(
          optimization_guide::features::
              ExternalAppPackageNamesApprovedForFetch()),
      top_host_provider_(top_host_provider),
      tab_url_provider_(tab_url_provider),
      clock_(base::DefaultClock::GetInstance()),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);

  hint_cache_->Initialize(
      optimization_guide::switches::
          ShouldPurgeOptimizationGuideStoreOnStartup(),
      base::BindOnce(&OptimizationGuideHintsManager::OnHintCacheInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service)
    navigation_predictor_service->AddObserver(this);
}

OptimizationGuideHintsManager::~OptimizationGuideHintsManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OptimizationGuideHintsManager::Shutdown() {
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->RemoveObserver(this);

  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service)
    navigation_predictor_service->RemoveObserver(this);
}

// static
optimization_guide::OptimizationGuideDecision OptimizationGuideHintsManager::
    GetOptimizationGuideDecisionFromOptimizationTypeDecision(
        optimization_guide::OptimizationTypeDecision
            optimization_type_decision) {
  switch (optimization_type_decision) {
    case optimization_guide::OptimizationTypeDecision::
        kAllowedByOptimizationFilter:
    case optimization_guide::OptimizationTypeDecision::kAllowedByHint:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTypeDecision::kUnknown:
    case optimization_guide::OptimizationTypeDecision::
        kHadOptimizationFilterButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHadHintButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHintFetchStartedButNotAvailableInTime:
    case optimization_guide::OptimizationTypeDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    case optimization_guide::OptimizationTypeDecision::kNotAllowedByHint:
    case optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint:
    case optimization_guide::OptimizationTypeDecision::kNoHintAvailable:
    case optimization_guide::OptimizationTypeDecision::
        kNotAllowedByOptimizationFilter:
      return optimization_guide::OptimizationGuideDecision::kFalse;
  }
}

void OptimizationGuideHintsManager::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  if (!CanProcessComponentVersion(pref_service_, info.version)) {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing);
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  std::unique_ptr<optimization_guide::StoreUpdateData> update_data =
      profile_->IsOffTheRecord()
          ? nullptr
          : hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version);

  // Processes the hints from the newly available component on a background
  // thread, providing a StoreUpdateData for component update from the hint
  // cache, so that each hint within the component can be moved into it. In the
  // case where the component's version is not newer than the optimization guide
  // store's component version, StoreUpdateData will be a nullptr and hint
  // processing will be skipped.
  // base::Unretained(this) is safe since |this| owns |background_task_runner_|
  // and the callback will be canceled if destroyed.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadComponentFile, info),
      base::BindOnce(&OptimizationGuideHintsManager::UpdateComponentHints,
                     ui_weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_update_closure_), std::move(update_data)));

  // Only replace hints component info if it is not the same - otherwise we will
  // destruct the object and it will be invalid later.
  if (!hints_component_info_ ||
      hints_component_info_->version.CompareTo(info.version) != 0) {
    hints_component_info_.emplace(info.version, info.path);
  }
}

void OptimizationGuideHintsManager::ProcessOptimizationFilters(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>&
        allowlist_optimization_filters,
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>&
        blocklist_optimization_filters) {
  optimization_types_with_filter_.clear();
  allowlist_optimization_filters_.clear();
  blocklist_optimization_filters_.clear();
  ProcessOptimizationFilterSet(allowlist_optimization_filters,
                               /*is_allowlist=*/true);
  ProcessOptimizationFilterSet(blocklist_optimization_filters,
                               /*is_allowlist=*/false);
}

void OptimizationGuideHintsManager::ProcessOptimizationFilterSet(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>& filters,
    bool is_allowlist) {
  for (const auto& filter : filters) {
    if (filter.optimization_type() !=
        optimization_guide::proto::TYPE_UNSPECIFIED) {
      optimization_types_with_filter_.insert(filter.optimization_type());
    }

    // Do not put anything in memory that we don't have registered.
    if (registered_optimization_types_.find(filter.optimization_type()) ==
        registered_optimization_types_.end()) {
      continue;
    }

    optimization_guide::RecordOptimizationFilterStatus(
        filter.optimization_type(),
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig);

    // Do not parse duplicate optimization filters.
    if (allowlist_optimization_filters_.find(filter.optimization_type()) !=
            allowlist_optimization_filters_.end() ||
        blocklist_optimization_filters_.find(filter.optimization_type()) !=
            blocklist_optimization_filters_.end()) {
      optimization_guide::RecordOptimizationFilterStatus(
          filter.optimization_type(),
          optimization_guide::OptimizationFilterStatus::
              kFailedServerFilterDuplicateConfig);
      continue;
    }

    // Parse optimization filter.
    optimization_guide::OptimizationFilterStatus status;
    std::unique_ptr<optimization_guide::OptimizationFilter>
        optimization_filter =
            optimization_guide::ProcessOptimizationFilter(filter, &status);
    if (optimization_filter) {
      if (is_allowlist) {
        allowlist_optimization_filters_.insert(
            {filter.optimization_type(), std::move(optimization_filter)});
      } else {
        blocklist_optimization_filters_.insert(
            {filter.optimization_type(), std::move(optimization_filter)});
      }
    }
    optimization_guide::RecordOptimizationFilterStatus(
        filter.optimization_type(), status);
  }
}

void OptimizationGuideHintsManager::OnHintCacheInitialized() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
      optimization_guide::switches::ParseComponentConfigFromCommandLine();
  if (manual_config) {
    std::unique_ptr<optimization_guide::StoreUpdateData> update_data =
        profile_->IsOffTheRecord()
            ? nullptr
            : hint_cache_->MaybeCreateUpdateDataForComponentHints(
                  base::Version(kManualConfigComponentVersion));
    // Allow |UpdateComponentHints| to block startup so that the first
    // navigation gets the hints when a command line hint proto is provided.
    UpdateComponentHints(base::DoNothing(), std::move(update_data),
                         std::move(manual_config));
  }

  // If the store is available, clear all hint state so newly registered types
  // can have their hints immediately included in hint fetches.
  if (hint_cache_->IsHintStoreAvailable() && should_clear_hints_for_new_type_) {
    ClearHostKeyedHints();
    should_clear_hints_for_new_type_ = false;
  }

  // Register as an observer regardless of hint proto override usage. This is
  // needed as a signal during testing.
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->AddObserver(this);
}

void OptimizationGuideHintsManager::UpdateComponentHints(
    base::OnceClosure update_closure,
    std::unique_ptr<optimization_guide::StoreUpdateData> update_data,
    std::unique_ptr<optimization_guide::proto::Configuration> config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If we get here, the component file has been processed correctly and did not
  // crash the device.
  pref_service_->ClearPref(
      optimization_guide::prefs::kPendingHintsProcessingVersion);

  if (!config) {
    MaybeRunUpdateClosure(std::move(update_closure));
    return;
  }

  ProcessOptimizationFilters(config->optimization_allowlists(),
                             config->optimization_blacklists());

  // Don't store hints in the store if it's off the record.
  if (update_data && !profile_->IsOffTheRecord()) {
    bool did_process_hints = hint_cache_->ProcessAndCacheHints(
        config->mutable_hints(), update_data.get());
    optimization_guide::RecordProcessHintsComponentResult(
        did_process_hints
            ? optimization_guide::ProcessHintsComponentResult::kSuccess
            : optimization_guide::ProcessHintsComponentResult::
                  kProcessedNoHints);
  } else {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints);
  }

  if (update_data) {
    hint_cache_->UpdateComponentHints(
        std::move(update_data),
        base::BindOnce(&OptimizationGuideHintsManager::OnComponentHintsUpdated,
                       ui_weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_closure),
                       /* hints_updated=*/true));
  } else {
    OnComponentHintsUpdated(std::move(update_closure), /*hints_updated=*/false);
  }
}

void OptimizationGuideHintsManager::OnComponentHintsUpdated(
    base::OnceClosure update_closure,
    bool hints_updated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      optimization_guide::kComponentHintsUpdatedResultHistogramString,
      hints_updated);

  if (optimization_guide::features::
          ShouldBatchUpdateHintsForActiveTabsAndTopHosts()) {
    SetLastHintsFetchAttemptTime(clock_->Now());
    if (optimization_guide::switches::ShouldOverrideFetchHintsTimer()) {
      FetchHintsForActiveTabs();
    } else if (!active_tabs_hints_fetch_timer_.IsRunning()) {
      // Batch update hints with a random delay.
      active_tabs_hints_fetch_timer_.Start(
          FROM_HERE, RandomFetchDelay(), this,
          &OptimizationGuideHintsManager::FetchHintsForActiveTabs);
    }
  }

  MaybeRunUpdateClosure(std::move(update_closure));
}

void OptimizationGuideHintsManager::ListenForNextUpdateForTesting(
    base::OnceClosure next_update_closure) {
  DCHECK(!next_update_closure_)
      << "Only one update closure is supported at a time";
  next_update_closure_ = std::move(next_update_closure);
}

void OptimizationGuideHintsManager::SetHintsFetcherFactoryForTesting(
    std::unique_ptr<optimization_guide::HintsFetcherFactory>
        hints_fetcher_factory) {
  hints_fetcher_factory_ = std::move(hints_fetcher_factory);
}

void OptimizationGuideHintsManager::SetClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

void OptimizationGuideHintsManager::ScheduleActiveTabsHintsFetch() {
  DCHECK(!active_tabs_hints_fetch_timer_.IsRunning());

  const base::TimeDelta active_tabs_refresh_duration =
      optimization_guide::features::GetActiveTabsFetchRefreshDuration();
  const base::TimeDelta time_since_last_fetch =
      clock_->Now() - GetLastHintsFetchAttemptTime();
  if (time_since_last_fetch >= active_tabs_refresh_duration) {
    // Fetched hints in the store should be updated and an attempt has not
    // been made in last
    // |optimization_guide::features::GetActiveTabsFetchRefreshDuration()|.
    SetLastHintsFetchAttemptTime(clock_->Now());
    active_tabs_hints_fetch_timer_.Start(
        FROM_HERE, RandomFetchDelay(), this,
        &OptimizationGuideHintsManager::FetchHintsForActiveTabs);
  } else {
    // If the fetched hints in the store are still up-to-date, set a timer
    // for when the hints need to be updated.
    base::TimeDelta fetcher_delay =
        active_tabs_refresh_duration - time_since_last_fetch;
    active_tabs_hints_fetch_timer_.Start(
        FROM_HERE, fetcher_delay, this,
        &OptimizationGuideHintsManager::ScheduleActiveTabsHintsFetch);
  }
}

const std::vector<GURL>
OptimizationGuideHintsManager::GetActiveTabURLsToRefresh() {
  if (!tab_url_provider_)
    return {};

  std::vector<GURL> active_tab_urls = tab_url_provider_->GetUrlsOfActiveTabs(
      optimization_guide::features::GetActiveTabsStalenessTolerance());

  std::set<GURL> urls_to_refresh;
  for (const auto& url : active_tab_urls) {
    if (!optimization_guide::IsValidURLForURLKeyedHint(url))
      continue;

    if (!hint_cache_->HasURLKeyedEntryForURL(url))
      urls_to_refresh.insert(url);
  }
  return std::vector<GURL>(urls_to_refresh.begin(), urls_to_refresh.end());
}

void OptimizationGuideHintsManager::FetchHintsForActiveTabs() {
  active_tabs_hints_fetch_timer_.Stop();
  active_tabs_hints_fetch_timer_.Start(
      FROM_HERE,
      optimization_guide::features::GetActiveTabsFetchRefreshDuration(), this,
      &OptimizationGuideHintsManager::ScheduleActiveTabsHintsFetch);

  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile_->IsOffTheRecord(), pref_service_)) {
    return;
  }

  if (!HasOptimizationTypeToFetchFor())
    return;

  std::vector<std::string> top_hosts;
  if (top_host_provider_)
    top_hosts = top_host_provider_->GetTopHosts();

  const std::vector<GURL> active_tab_urls_to_refresh =
      GetActiveTabURLsToRefresh();

  base::UmaHistogramCounts100(
      "OptimizationGuide.HintsManager.ActiveTabUrlsToFetchFor",
      active_tab_urls_to_refresh.size());

  if (top_hosts.empty() && active_tab_urls_to_refresh.empty())
    return;

  if (!batch_update_hints_fetcher_) {
    DCHECK(hints_fetcher_factory_);
    batch_update_hints_fetcher_ = hints_fetcher_factory_->BuildInstance();
  }

  // Add hosts of active tabs to list of hosts to fetch for. Since we are mainly
  // fetching for updated information on tabs, add those to the front of the
  // list.
  base::flat_set<std::string> top_hosts_set =
      base::flat_set<std::string>(top_hosts.begin(), top_hosts.end());
  for (const auto& url : active_tab_urls_to_refresh) {
    if (!url.has_host() ||
        top_hosts_set.find(url.host()) == top_hosts_set.end()) {
      continue;
    }
    if (!hint_cache_->HasHint(url.host())) {
      top_hosts_set.insert(url.host());
      top_hosts.insert(top_hosts.begin(), url.host());
    }
  }

  batch_update_hints_fetcher_->FetchOptimizationGuideServiceHints(
      top_hosts, active_tab_urls_to_refresh, registered_optimization_types_,
      optimization_guide::proto::CONTEXT_BATCH_UPDATE,
      g_browser_process->GetApplicationLocale(),
      base::BindOnce(
          &OptimizationGuideHintsManager::OnHintsForActiveTabsFetched,
          ui_weak_ptr_factory_.GetWeakPtr(), top_hosts_set,
          base::flat_set<GURL>(active_tab_urls_to_refresh.begin(),
                               active_tab_urls_to_refresh.end())));
}

void OptimizationGuideHintsManager::OnHintsForActiveTabsFetched(
    const base::flat_set<std::string>& hosts_fetched,
    const base::flat_set<GURL>& urls_fetched,
    base::Optional<std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
        get_hints_response) {
  if (!get_hints_response)
    return;

  hint_cache_->UpdateFetchedHints(
      std::move(*get_hints_response),
      clock_->Now() +
          optimization_guide::features::GetActiveTabsFetchRefreshDuration(),
      hosts_fetched, urls_fetched,
      base::BindOnce(
          &OptimizationGuideHintsManager::OnFetchedActiveTabsHintsStored,
          ui_weak_ptr_factory_.GetWeakPtr()));
}

void OptimizationGuideHintsManager::OnPageNavigationHintsFetched(
    base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
    const base::Optional<GURL>& navigation_url,
    const base::flat_set<GURL>& page_navigation_urls_requested,
    const base::flat_set<std::string>& page_navigation_hosts_requested,
    base::Optional<std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
        get_hints_response) {
  if (!get_hints_response.has_value() || !get_hints_response.value()) {
    if (navigation_url) {
      CleanUpFetcherForNavigation(*navigation_url);
      PrepareToInvokeRegisteredCallbacks(*navigation_url);
    }
    return;
  }

  hint_cache_->UpdateFetchedHints(
      std::move(*get_hints_response),
      clock_->Now() +
          optimization_guide::features::GetActiveTabsFetchRefreshDuration(),
      page_navigation_hosts_requested, page_navigation_urls_requested,
      base::BindOnce(
          &OptimizationGuideHintsManager::OnFetchedPageNavigationHintsStored,
          ui_weak_ptr_factory_.GetWeakPtr(), navigation_data_weak_ptr,
          navigation_url, page_navigation_hosts_requested));
}

void OptimizationGuideHintsManager::OnFetchedActiveTabsHintsStored() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOCAL_HISTOGRAM_BOOLEAN("OptimizationGuide.FetchedHints.Stored", true);

  if (!optimization_guide::features::ShouldPersistHintsToDisk()) {
    // If we aren't persisting hints to disk, there's no point in purging
    // hints from disk or starting a new fetch since at this point we should
    // just be fetching everything on page navigation and only storing
    // in-memory.
    return;
  }

  hint_cache_->PurgeExpiredFetchedHints();
}

void OptimizationGuideHintsManager::OnFetchedPageNavigationHintsStored(
    base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
    const base::Optional<GURL>& navigation_url,
    const base::flat_set<std::string>& page_navigation_hosts_requested) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (navigation_data_weak_ptr) {
    navigation_data_weak_ptr->set_hints_fetch_end(base::TimeTicks::Now());
  }

  if (navigation_url) {
    CleanUpFetcherForNavigation(*navigation_url);
    PrepareToInvokeRegisteredCallbacks(*navigation_url);
  }
}

bool OptimizationGuideHintsManager::IsHintBeingFetchedForNavigation(
    const GURL& navigation_url) {
  return page_navigation_hints_fetchers_.Get(navigation_url) !=
         page_navigation_hints_fetchers_.end();
}

void OptimizationGuideHintsManager::CleanUpFetcherForNavigation(
    const GURL& navigation_url) {
  auto it = page_navigation_hints_fetchers_.Peek(navigation_url);
  if (it != page_navigation_hints_fetchers_.end())
    page_navigation_hints_fetchers_.Erase(it);
}

base::Time OptimizationGuideHintsManager::GetLastHintsFetchAttemptTime() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(pref_service_->GetInt64(
          optimization_guide::prefs::kHintsFetcherLastFetchAttempt)));
}

void OptimizationGuideHintsManager::SetLastHintsFetchAttemptTime(
    base::Time last_attempt_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_service_->SetInt64(
      optimization_guide::prefs::kHintsFetcherLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void OptimizationGuideHintsManager::LoadHintForNavigation(
    content::NavigationHandle* navigation_handle,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto& url = navigation_handle->GetURL();
  if (!url.has_host()) {
    std::move(callback).Run();
    return;
  }

  LoadHintForHost(url.host(), std::move(callback));
}

void OptimizationGuideHintsManager::LoadHintForHost(
    const std::string& host,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  hint_cache_->LoadHint(
      host,
      base::BindOnce(&OptimizationGuideHintsManager::OnHintLoaded,
                     ui_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool OptimizationGuideHintsManager::IsGoogleURL(const GURL& url) const {
  return google_util::IsGoogleHostname(url.host(),
                                       google_util::DISALLOW_SUBDOMAIN);
}

bool OptimizationGuideHintsManager::IsAllowedToFetchForNavigationPrediction(
    const base::Optional<NavigationPredictorKeyedService::Prediction>
        prediction) const {
  if (!prediction)
    return false;

  if (prediction->prediction_source() ==
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage) {
    const base::Optional<GURL> source_document_url =
        prediction->source_document_url();
    if (!source_document_url || source_document_url->is_empty())
      return false;

    // We only extract next predicted navigations from Google URLs.
    return IsGoogleURL(*source_document_url);
  }

  if (prediction->prediction_source() ==
      NavigationPredictorKeyedService::PredictionSource::kExternalAndroidApp) {
    if (external_app_packages_approved_for_fetch_.empty())
      return false;

    const base::Optional<std::vector<std::string>> external_app_packages_name =
        prediction->external_app_packages_name();
    if (!external_app_packages_name || external_app_packages_name->empty())
      return false;

    for (const auto& package_name : *external_app_packages_name) {
      if (external_app_packages_approved_for_fetch_.find(package_name) ==
          external_app_packages_approved_for_fetch_.end())
        return false;
    }
    // If we get here, all apps have been approved for fetching.
    return true;
  }

  return false;
}

void OptimizationGuideHintsManager::OnPredictionUpdated(
    const base::Optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsAllowedToFetchForNavigationPrediction(prediction))
    return;

  // Extract the target hosts and URLs. Use a flat set to remove duplicates.
  // |target_hosts_serialized| is the ordered list of non-duplicate hosts.
  // TODO(sophiechang): See if we can make this logic simpler.
  base::flat_set<std::string> target_hosts;
  std::vector<std::string> target_hosts_serialized;
  std::vector<GURL> target_urls;
  for (const auto& url : prediction->sorted_predicted_urls()) {
    if (!IsAllowedToFetchNavigationHints(url))
      continue;

    // Insert the host to |target_hosts|. The host is inserted to
    // |target_hosts_serialized| only if it was not a duplicate insertion to
    // |target_hosts|.
    std::pair<base::flat_set<std::string>::iterator, bool> insert_result =
        target_hosts.insert(url.host());
    if (insert_result.second)
      target_hosts_serialized.push_back(url.host());

    // Ensure that the 2 data structures remain synchronized.
    DCHECK_EQ(target_hosts.size(), target_hosts_serialized.size());

    if (!hint_cache_->HasURLKeyedEntryForURL(url))
      target_urls.push_back(url);
  }

  if (target_hosts.empty() && target_urls.empty())
    return;

  if (!batch_update_hints_fetcher_) {
    DCHECK(hints_fetcher_factory_);
    batch_update_hints_fetcher_ = hints_fetcher_factory_->BuildInstance();
  }

  // Use the batch update hints fetcher for fetches off the SRP since we are
  // not fetching for the current navigation, even though we are fetching using
  // the page navigation context. However, since we do want to load the hints
  // returned, we pass this through to the page navigation callback.
  batch_update_hints_fetcher_->FetchOptimizationGuideServiceHints(
      target_hosts_serialized, target_urls, registered_optimization_types_,
      optimization_guide::proto::CONTEXT_BATCH_UPDATE,
      g_browser_process->GetApplicationLocale(),
      base::BindOnce(
          &OptimizationGuideHintsManager::OnPageNavigationHintsFetched,
          ui_weak_ptr_factory_.GetWeakPtr(), nullptr, base::nullopt,
          target_urls, target_hosts));
}

void OptimizationGuideHintsManager::OnHintLoaded(
    base::OnceClosure callback,
    const optimization_guide::proto::Hint* loaded_hint) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record the result of loading a hint. This is used as a signal for testing.
  LOCAL_HISTOGRAM_BOOLEAN(optimization_guide::kLoadedHintLocalHistogramString,
                          loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  std::move(callback).Run();
}

void OptimizationGuideHintsManager::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  bool should_load_new_optimization_filter = false;

  DictionaryPrefUpdate previously_registered_opt_types(
      pref_service_,
      optimization_guide::prefs::kPreviouslyRegisteredOptimizationTypes);
  for (const auto optimization_type : optimization_types) {
    if (optimization_type == optimization_guide::proto::TYPE_UNSPECIFIED)
      continue;

    if (registered_optimization_types_.find(optimization_type) !=
        registered_optimization_types_.end()) {
      continue;
    }
    registered_optimization_types_.insert(optimization_type);

    base::Optional<double> value = previously_registered_opt_types->FindBoolKey(
        optimization_guide::proto::OptimizationType_Name(optimization_type));
    if (!value) {
      if (!profile_->IsOffTheRecord() &&
          !ShouldIgnoreNewlyRegisteredOptimizationType(optimization_type)) {
        should_clear_hints_for_new_type_ = true;
      }
      previously_registered_opt_types->SetBoolKey(
          optimization_guide::proto::OptimizationType_Name(optimization_type),
          true);
    }

    if (!should_load_new_optimization_filter) {
      if (optimization_types_with_filter_.find(optimization_type) !=
          optimization_types_with_filter_.end()) {
        should_load_new_optimization_filter = true;
      }
    }
  }

  // If the store is available, clear all hint state so newly registered types
  // can have their hints immediately included in hint fetches.
  if (hint_cache_->IsHintStoreAvailable() && should_clear_hints_for_new_type_) {
    ClearHostKeyedHints();
    should_clear_hints_for_new_type_ = false;
  }

  if (should_load_new_optimization_filter) {
    if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
      std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
          optimization_guide::switches::ParseComponentConfigFromCommandLine();
      if (manual_config->optimization_allowlists_size() > 0 ||
          manual_config->optimization_blacklists_size() > 0) {
        ProcessOptimizationFilters(manual_config->optimization_allowlists(),
                                   manual_config->optimization_blacklists());
      }
    } else {
      DCHECK(hints_component_info_);
      OnHintsComponentAvailable(*hints_component_info_);
    }
  } else {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
  }
}

bool OptimizationGuideHintsManager::HasLoadedOptimizationAllowlist(
    optimization_guide::proto::OptimizationType optimization_type) {
  return allowlist_optimization_filters_.find(optimization_type) !=
         allowlist_optimization_filters_.end();
}

bool OptimizationGuideHintsManager::HasLoadedOptimizationBlocklist(
    optimization_guide::proto::OptimizationType optimization_type) {
  return blocklist_optimization_filters_.find(optimization_type) !=
         blocklist_optimization_filters_.end();
}

void OptimizationGuideHintsManager::CanApplyOptimizationAsync(
    const GURL& navigation_url,
    const base::Optional<int64_t>& navigation_id,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide::OptimizationMetadata metadata;
  optimization_guide::OptimizationTypeDecision type_decision =
      CanApplyOptimization(navigation_url, navigation_id, optimization_type,
                           &metadata);
  optimization_guide::OptimizationGuideDecision decision =
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(type_decision);
  // It's possible that a hint that applies to |navigation_url| will come in
  // later, so only run the callback if we are sure we can apply the decision.
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue ||
      HasAllInformationForDecisionAvailable(navigation_url,
                                            optimization_type)) {
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ApplyDecisionAsync." +
            optimization_guide::GetStringNameForOptimizationType(
                optimization_type),
        type_decision);
    std::move(callback).Run(decision, metadata);
    return;
  }

  registered_callbacks_[navigation_url][optimization_type].push_back(
      std::make_pair(navigation_id, std::move(callback)));
}

optimization_guide::OptimizationTypeDecision
OptimizationGuideHintsManager::CanApplyOptimization(
    const GURL& navigation_url,
    const base::Optional<int64_t>& navigation_id,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Clear out optimization metadata if provided.
  if (optimization_metadata)
    *optimization_metadata = {};

  // If the type is not registered, we probably don't have a hint for it, so
  // just return.
  if (registered_optimization_types_.find(optimization_type) ==
      registered_optimization_types_.end()) {
    return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
  }

  // If the URL doesn't have a host, we cannot query the hint for it, so just
  // return early.
  if (!navigation_url.has_host())
    return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
  const auto& host = navigation_url.host();

  // Check if the URL should be filtered out if we have an optimization filter
  // for the type.

    // Check if we have an allowlist loaded into memory for it, and if we do,
    // see if the URL matches anything in the filter.
    if (allowlist_optimization_filters_.find(optimization_type) !=
        allowlist_optimization_filters_.end()) {
      return allowlist_optimization_filters_[optimization_type]->Matches(
                 navigation_url)
                 ? optimization_guide::OptimizationTypeDecision::
                       kAllowedByOptimizationFilter
                 : optimization_guide::OptimizationTypeDecision::
                       kNotAllowedByOptimizationFilter;
    }

    // Check if we have a blocklist loaded into memory for it, and if we do, see
    // if the URL matches anything in the filter.
    if (blocklist_optimization_filters_.find(optimization_type) !=
        blocklist_optimization_filters_.end()) {
      return blocklist_optimization_filters_[optimization_type]->Matches(
                 navigation_url)
                 ? optimization_guide::OptimizationTypeDecision::
                       kNotAllowedByOptimizationFilter
                 : optimization_guide::OptimizationTypeDecision::
                       kAllowedByOptimizationFilter;
    }

    // Check if we had an optimization filter for it, but it was not loaded into
    // memory.
    if (optimization_types_with_filter_.find(optimization_type) !=
        optimization_types_with_filter_.end()) {
      return optimization_guide::OptimizationTypeDecision::
          kHadOptimizationFilterButNotLoadedInTime;
    }

  base::Optional<uint64_t> tuning_version;

  // First, check if the optimization type is whitelisted by a URL-keyed hint.
  const optimization_guide::proto::Hint* url_keyed_hint =
      hint_cache_->GetURLKeyedHint(navigation_url);
  if (url_keyed_hint) {
    DCHECK_EQ(url_keyed_hint->page_hints_size(), 1);
    if (url_keyed_hint->page_hints_size() > 0) {
      bool is_allowed = IsOptimizationTypeAllowed(
          url_keyed_hint->page_hints(0).whitelisted_optimizations(),
          optimization_type, optimization_metadata, &tuning_version);
      if (is_allowed || tuning_version) {
        MaybeLogOptimizationAutotuningUKMForNavigation(
            navigation_id, optimization_type, tuning_version);
        return is_allowed ? optimization_guide::OptimizationTypeDecision::
                                kAllowedByHint
                          : optimization_guide::OptimizationTypeDecision::
                                kNotAllowedByHint;
      }
    }
  }

  // Check if we have a hint already loaded for this navigation.
  const optimization_guide::proto::Hint* loaded_hint =
      hint_cache_->GetHostKeyedHintIfLoaded(host);
  if (!loaded_hint) {
    if (hint_cache_->HasHint(host)) {
      // If we do not have a hint already loaded and we do not have one in the
      // cache, we do not know what to do with the URL so just return.
      // Otherwise, we do have information, but we just do not know it yet.
      if (optimization_guide::features::ShouldPersistHintsToDisk()) {
        return optimization_guide::OptimizationTypeDecision::
            kHadHintButNotLoadedInTime;
      } else {
        return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
      }
    }

    if (IsHintBeingFetchedForNavigation(navigation_url)) {
      return optimization_guide::OptimizationTypeDecision::
          kHintFetchStartedButNotAvailableInTime;
    }

    return optimization_guide::OptimizationTypeDecision::kNoHintAvailable;
  }

  bool is_allowed = IsOptimizationTypeAllowed(
      loaded_hint->whitelisted_optimizations(), optimization_type,
      optimization_metadata, &tuning_version);
  if (is_allowed || tuning_version) {
    MaybeLogOptimizationAutotuningUKMForNavigation(
        navigation_id, optimization_type, tuning_version);
    return is_allowed
               ? optimization_guide::OptimizationTypeDecision::kAllowedByHint
               : optimization_guide::OptimizationTypeDecision::
                     kNotAllowedByHint;
  }

  const optimization_guide::proto::PageHint* matched_page_hint =
      loaded_hint
          ? optimization_guide::FindPageHintForURL(navigation_url, loaded_hint)
          : nullptr;
  if (!matched_page_hint)
    return optimization_guide::OptimizationTypeDecision::kNotAllowedByHint;

  is_allowed = IsOptimizationTypeAllowed(
      matched_page_hint->whitelisted_optimizations(), optimization_type,
      optimization_metadata, &tuning_version);
  MaybeLogOptimizationAutotuningUKMForNavigation(
      navigation_id, optimization_type, tuning_version);
  return is_allowed
             ? optimization_guide::OptimizationTypeDecision::kAllowedByHint
             : optimization_guide::OptimizationTypeDecision::kNotAllowedByHint;
}

void OptimizationGuideHintsManager::PrepareToInvokeRegisteredCallbacks(
    const GURL& navigation_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (registered_callbacks_.find(navigation_url) == registered_callbacks_.end())
    return;

  LoadHintForHost(
      navigation_url.host(),
      base::BindOnce(
          &OptimizationGuideHintsManager::OnReadyToInvokeRegisteredCallbacks,
          ui_weak_ptr_factory_.GetWeakPtr(), navigation_url));
}

void OptimizationGuideHintsManager::OnReadyToInvokeRegisteredCallbacks(
    const GURL& navigation_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (registered_callbacks_.find(navigation_url) ==
      registered_callbacks_.end()) {
    return;
  }

  for (auto& opt_type_and_callbacks :
       registered_callbacks_.at(navigation_url)) {
    optimization_guide::proto::OptimizationType opt_type =
        opt_type_and_callbacks.first;

    for (auto& navigation_id_and_callback : opt_type_and_callbacks.second) {
      base::Optional<int64_t> navigation_id = navigation_id_and_callback.first;
      optimization_guide::OptimizationMetadata metadata;
      optimization_guide::OptimizationTypeDecision type_decision =
          CanApplyOptimization(navigation_url, navigation_id, opt_type,
                               &metadata);
      optimization_guide::OptimizationGuideDecision decision =
          GetOptimizationGuideDecisionFromOptimizationTypeDecision(
              type_decision);
      base::UmaHistogramEnumeration(
          "OptimizationGuide.ApplyDecisionAsync." +
              optimization_guide::GetStringNameForOptimizationType(opt_type),
          type_decision);
      std::move(navigation_id_and_callback.second).Run(decision, metadata);
    }
  }
  registered_callbacks_.erase(navigation_url);
}

void OptimizationGuideHintsManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  current_effective_connection_type_ = effective_connection_type;
}

bool OptimizationGuideHintsManager::HasOptimizationTypeToFetchFor() {
  if (registered_optimization_types_.empty())
    return false;

  for (const auto& optimization_type : registered_optimization_types_) {
    if (optimization_types_with_filter_.find(optimization_type) ==
        optimization_types_with_filter_.end()) {
      return true;
    }
  }
  return false;
}

bool OptimizationGuideHintsManager::IsAllowedToFetchNavigationHints(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!HasOptimizationTypeToFetchFor())
    return false;

  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile_->IsOffTheRecord(), pref_service_)) {
    return false;
  }
  DCHECK(!profile_->IsOffTheRecord());

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return false;

  base::Optional<net::EffectiveConnectionType> ect_max_threshold =
      optimization_guide::features::
          GetMaxEffectiveConnectionTypeForNavigationHintsFetch();
  // If the threshold is unavailable, return early since there is no safe way to
  // proceed.
  if (!ect_max_threshold.has_value())
    return false;

  if (current_effective_connection_type_ <
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G ||
      current_effective_connection_type_ > ect_max_threshold.value()) {
    return false;
  }

  return true;
}

void OptimizationGuideHintsManager::OnNavigationStartOrRedirect(
    content::NavigationHandle* navigation_handle,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LoadHintForNavigation(navigation_handle, std::move(callback));

  if (optimization_guide::switches::
          DisableFetchingHintsAtNavigationStartForTesting()) {
    return;
  }

  MaybeFetchHintsForNavigation(navigation_handle);
}

void OptimizationGuideHintsManager::MaybeFetchHintsForNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (registered_optimization_types_.empty())
    return;

  const GURL url = navigation_handle->GetURL();
  if (!IsAllowedToFetchNavigationHints(url))
    return;

  ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder
      race_navigation_recorder(navigation_handle);

  // We expect that if the URL is being fetched for, we have already run through
  // the logic to decide if we also require fetching hints for the host.
  if (IsHintBeingFetchedForNavigation(url)) {
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchAlreadyInProgress);
    return;
  }

  std::vector<std::string> hosts;
  std::vector<GURL> urls;
  if (!hint_cache_->HasHint(url.host())) {
    hosts.push_back(url.host());
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost);
  }

  if (!hint_cache_->HasURLKeyedEntryForURL(url)) {
    urls.push_back(url);
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchURL);
  }

  if (hosts.empty() && urls.empty()) {
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted);
    return;
  }

  DCHECK(hints_fetcher_factory_);
  auto it = page_navigation_hints_fetchers_.Put(
      url, hints_fetcher_factory_->BuildInstance());

  UMA_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.HintsManager.ConcurrentPageNavigationFetches",
      page_navigation_hints_fetchers_.size());

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  navigation_data->set_hints_fetch_start(base::TimeTicks::Now());
  it->second->FetchOptimizationGuideServiceHints(
      hosts, urls, registered_optimization_types_,
      optimization_guide::proto::CONTEXT_PAGE_NAVIGATION,
      g_browser_process->GetApplicationLocale(),
      base::BindOnce(
          &OptimizationGuideHintsManager::OnPageNavigationHintsFetched,
          ui_weak_ptr_factory_.GetWeakPtr(), navigation_data->GetWeakPtr(), url,
          base::flat_set<GURL>({url}),
          base::flat_set<std::string>({url.host()})));

  if (!hosts.empty() && !urls.empty()) {
    race_navigation_recorder.set_race_attempt_status(
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL);
  }
}

void OptimizationGuideHintsManager::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The callbacks will be invoked when the fetch request comes back, so it
  // will be cleaned up later.
  for (const auto& url : navigation_redirect_chain) {
    if (IsHintBeingFetchedForNavigation(url))
      continue;

    PrepareToInvokeRegisteredCallbacks(url);
  }
}

optimization_guide::OptimizationGuideStore*
OptimizationGuideHintsManager::hint_store() {
  return hint_cache_->hint_store();
}

bool OptimizationGuideHintsManager::HasAllInformationForDecisionAvailable(
    const GURL& navigation_url,
    optimization_guide::proto::OptimizationType optimization_type) {
  if (HasLoadedOptimizationAllowlist(optimization_type) ||
      HasLoadedOptimizationBlocklist(optimization_type)) {
    // If we have an optimization filter for the optimization type, it is
    // consulted instead of any hints that may be available.
    return true;
  }

  bool has_host_keyed_hint = hint_cache_->HasHint(navigation_url.host());
  const auto* host_keyed_hint =
      hint_cache_->GetHostKeyedHintIfLoaded(navigation_url.host());
  if (has_host_keyed_hint && host_keyed_hint == nullptr) {
    // If we have a host-keyed hint in the cache and it is not loaded, we do not
    // have all information available, regardless of whether we can fetch hints
    // or not.
    return false;
  }

  if (!IsAllowedToFetchNavigationHints(navigation_url)) {
    // If we are not allowed to fetch hints for the navigation, we have all
    // information available if the host-keyed hint we have has been loaded
    // already or we don't have a hint available.
    return host_keyed_hint != nullptr || !has_host_keyed_hint;
  }

  if (IsHintBeingFetchedForNavigation(navigation_url)) {
    // If a hint is being fetched for the navigation, then we do not have all
    // information available yet.
    return false;
  }

  // If we are allowed to fetch hints for the navigation, we only have all
  // information available for certain if we have attempted to get the URL-keyed
  // hint and if the host-keyed hint is loaded.
  return hint_cache_->HasURLKeyedEntryForURL(navigation_url) &&
         host_keyed_hint != nullptr;
}

void OptimizationGuideHintsManager::ClearFetchedHints() {
  hint_cache_->ClearFetchedHints();
  optimization_guide::HintsFetcher::ClearHostsSuccessfullyFetched(
      pref_service_);
}

void OptimizationGuideHintsManager::ClearHostKeyedHints() {
  hint_cache_->ClearHostKeyedHints();
  optimization_guide::HintsFetcher::ClearHostsSuccessfullyFetched(
      pref_service_);
}

void OptimizationGuideHintsManager::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const base::Optional<optimization_guide::OptimizationMetadata>& metadata) {
  std::unique_ptr<optimization_guide::proto::Hint> hint =
      std::make_unique<optimization_guide::proto::Hint>();
  hint->set_key(url.spec());
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_type);
  if (!metadata) {
    hint_cache_->AddHintForTesting(url, std::move(hint));
    PrepareToInvokeRegisteredCallbacks(url);
    return;
  }
  if (metadata->loading_predictor_metadata()) {
    *optimization->mutable_loading_predictor_metadata() =
        *metadata->loading_predictor_metadata();
  } else if (metadata->performance_hints_metadata()) {
    *optimization->mutable_performance_hints_metadata() =
        *metadata->performance_hints_metadata();
  } else if (metadata->public_image_metadata()) {
    *optimization->mutable_public_image_metadata() =
        *metadata->public_image_metadata();
  } else if (metadata->any_metadata()) {
    *optimization->mutable_any_metadata() = *metadata->any_metadata();
  } else {
    NOTREACHED();
  }
  hint_cache_->AddHintForTesting(url, std::move(hint));
  PrepareToInvokeRegisteredCallbacks(url);
}
