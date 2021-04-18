// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/core/command_line_top_host_provider.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"
#else
#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"
#endif

namespace {

// Returns the top host provider to be used with this keyed service. Can return
// nullptr if the user or browser is not permitted to call the remote
// Optimization Guide Service.
std::unique_ptr<optimization_guide::TopHostProvider>
GetTopHostProviderIfUserPermitted(content::BrowserContext* browser_context) {
  // First check whether the command-line flag should be used.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider =
      optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();
  if (top_host_provider)
    return top_host_provider;

  // If not enabled by flag, see if the user is allowed to fetch from the remote
  // Optimization Guide Service.
  return OptimizationGuideTopHostProvider::CreateIfAllowed(browser_context);
}

// Returns the OptimizationGuideDecision from |optimization_target_decision|.
optimization_guide::OptimizationGuideDecision
GetOptimizationGuideDecisionFromOptimizationTargetDecision(
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  switch (optimization_target_decision) {
    case optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch:
    case optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback:
      return optimization_guide::OptimizationGuideDecision::kFalse;
    case optimization_guide::OptimizationTargetDecision::kPageLoadMatches:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTargetDecision::
        kModelNotAvailableOnClient:
    case optimization_guide::OptimizationTargetDecision::kUnknown:
    case optimization_guide::OptimizationTargetDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
  }
}

// Logs |optimization_target_decision| for |optimization_target| in a histogram
// and passes the corresponding OptimizationGuideDecision to |callback|.
void LogOptimizationTargetDecisionAndPassOptimizationGuideDecision(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationGuideTargetDecisionCallback callback,
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  base::UmaHistogramExactLinear(
      "OptimizationGuide.TargetDecision." +
          optimization_guide::GetStringNameForOptimizationTarget(
              optimization_target),
      static_cast<int>(optimization_target_decision),
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kMaxValue));

  std::move(callback).Run(
      GetOptimizationGuideDecisionFromOptimizationTargetDecision(
          optimization_target_decision));
}

const char kOldOptimizationGuideHintStore[] = "previews_hint_cache_store";

// Deletes old store paths that were written in incorrect locations.
void DeleteOldStorePaths(const base::FilePath& profile_path) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          base::GetDeletePathRecursivelyCallback(),
          profile_path.AddExtensionASCII(kOldOptimizationGuideHintStore)));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          base::GetDeletePathRecursivelyCallback(),
          profile_path.AddExtension(
              optimization_guide::
                  kOptimizationGuidePredictionModelAndFeaturesStore)));
}

}  // namespace

OptimizationGuideKeyedService::OptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Initialize();
}

OptimizationGuideKeyedService::~OptimizationGuideKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OptimizationGuideKeyedService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // Regardless of whether the profile is off the record or not, we initialize
  // the Optimization Guide with the database associated with the original
  // profile.
  auto* proto_db_provider = content::BrowserContext::GetDefaultStoragePartition(
                                profile->GetOriginalProfile())
                                ->GetProtoDatabaseProvider();
  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  // We have different behavior if |this| is created for an incognito profile.
  // For incognito profiles, we act in "read-only" mode of the original
  // profile's store and do not fetch any new hints or models.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  optimization_guide::OptimizationGuideStore* hint_store;
  optimization_guide::OptimizationGuideStore*
      prediction_model_and_features_store;
  if (profile->IsOffTheRecord()) {
    OptimizationGuideKeyedService* original_ogks =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            profile->GetOriginalProfile());
    DCHECK(original_ogks);
    hint_store = original_ogks->GetHintsManager()->hint_store();
    prediction_model_and_features_store =
        original_ogks->GetPredictionManager()->model_and_features_store();
  } else {
    url_loader_factory =
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetURLLoaderFactoryForBrowserProcess();

    top_host_provider_ = GetTopHostProviderIfUserPermitted(browser_context_);
    bool optimization_guide_fetching_enabled = top_host_provider_ != nullptr;
    UMA_HISTOGRAM_BOOLEAN("OptimizationGuide.RemoteFetchingEnabled",
                          optimization_guide_fetching_enabled);
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOptimizationGuideRemoteFetching",
        optimization_guide_fetching_enabled ? "Enabled" : "Disabled");

#if defined(OS_ANDROID)
    tab_url_provider_ = std::make_unique<
        optimization_guide::android::OptimizationGuideTabUrlProviderAndroid>(
        profile);
#else
    tab_url_provider_ =
        std::make_unique<OptimizationGuideTabUrlProvider>(profile);
#endif

    hint_store_ =
        optimization_guide::features::ShouldPersistHintsToDisk()
            ? std::make_unique<optimization_guide::OptimizationGuideStore>(
                  proto_db_provider,
                  profile_path.Append(
                      optimization_guide::kOptimizationGuideHintStore),
                  base::ThreadPool::CreateSequencedTaskRunner(
                      {base::MayBlock(), base::TaskPriority::BEST_EFFORT}))
            : nullptr;
    hint_store = hint_store_.get();

    prediction_model_and_features_store_ =
        std::make_unique<optimization_guide::OptimizationGuideStore>(
            proto_db_provider,
            profile_path.Append(
                optimization_guide::
                    kOptimizationGuidePredictionModelAndFeaturesStore),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
    prediction_model_and_features_store =
        prediction_model_and_features_store_.get();
  }

  hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
      profile, profile->GetPrefs(), hint_store, top_host_provider_.get(),
      tab_url_provider_.get(), url_loader_factory);
  prediction_manager_ = std::make_unique<optimization_guide::PredictionManager>(
      prediction_model_and_features_store, top_host_provider_.get(),
      url_loader_factory, profile->GetPrefs(), profile);

  // The previous store paths were written in incorrect locations. Delete the
  // old paths. Remove this code in 04/2022 since it should be assumed that all
  // clients that had the previous path have had their previous stores deleted.
  DeleteOldStorePaths(profile_path);
}

OptimizationGuideHintsManager*
OptimizationGuideKeyedService::GetHintsManager() {
  return hints_manager_.get();
}

void OptimizationGuideKeyedService::OnNavigationStartOrRedirect(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          hints_manager_->registered_optimization_types();
  if (!registered_optimization_types.empty()) {
    hints_manager_->OnNavigationStartOrRedirect(navigation_handle,
                                                base::DoNothing());
  }

  if (navigation_data) {
    navigation_data->set_registered_optimization_types(
        hints_manager_->registered_optimization_types());
    navigation_data->set_registered_optimization_targets(
        prediction_manager_->GetRegisteredOptimizationTargets());
  }
}

void OptimizationGuideKeyedService::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  hints_manager_->OnNavigationFinish(navigation_redirect_chain);
}

void OptimizationGuideKeyedService::RegisterOptimizationTargets(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets) {
  std::vector<std::pair<optimization_guide::proto::OptimizationTarget,
                        base::Optional<optimization_guide::proto::Any>>>
      optimization_targets_and_metadata;
  for (optimization_guide::proto::OptimizationTarget optimization_target :
       optimization_targets) {
    optimization_targets_and_metadata.emplace_back(
        std::make_pair(optimization_target, base::nullopt));
  }
  prediction_manager_->RegisterOptimizationTargets(
      optimization_targets_and_metadata);
}

void OptimizationGuideKeyedService::ShouldTargetNavigationAsync(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationGuideTargetDecisionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  optimization_guide::OptimizationTargetDecision target_decision =
      prediction_manager_->ShouldTargetNavigation(navigation_handle,
                                                  optimization_target);
  LogOptimizationTargetDecisionAndPassOptimizationGuideDecision(
      optimization_target, std::move(callback), target_decision);
}

void OptimizationGuideKeyedService::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const base::Optional<optimization_guide::proto::Any>& model_metadata,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  prediction_manager_->AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, observer);
}

void OptimizationGuideKeyedService::RemoveObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetModelObserver* observer) {
    prediction_manager_->RemoveObserverForOptimizationTargetModel(
        optimization_target, observer);
}

void OptimizationGuideKeyedService::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
    hints_manager_->RegisterOptimizationTypes(optimization_types);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager_->CanApplyOptimization(url, /*navigation_id=*/base::nullopt,
                                           optimization_type,
                                           optimization_metadata);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ApplyDecision." +
          optimization_guide::GetStringNameForOptimizationType(
              optimization_type),
      optimization_type_decision);
  return OptimizationGuideHintsManager::
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(
          optimization_type_decision);
}

void OptimizationGuideKeyedService::CanApplyOptimizationAsync(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  hints_manager_->CanApplyOptimizationAsync(
      navigation_handle->GetURL(), navigation_handle->GetNavigationId(),
      optimization_type, std::move(callback));
}

void OptimizationGuideKeyedService::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const base::Optional<optimization_guide::OptimizationMetadata>& metadata) {
  hints_manager_->AddHintForTesting(url, optimization_type, metadata);
}

void OptimizationGuideKeyedService::ClearData() {
  hints_manager_->ClearFetchedHints();
  prediction_manager_->ClearHostModelFeatures();
}

void OptimizationGuideKeyedService::Shutdown() {
  hints_manager_->Shutdown();
}

void OptimizationGuideKeyedService::OverrideTargetModelFileForTesting(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const base::Optional<optimization_guide::proto::Any>& model_metadata,
    const base::FilePath& file_path) {
  prediction_manager_->OverrideTargetModelFileForTesting(
      optimization_target, model_metadata, file_path);
}
