// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/command_line_top_host_provider.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/top_host_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

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

}  // namespace

OptimizationGuideKeyedService::OptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!browser_context_->IsOffTheRecord());
}

OptimizationGuideKeyedService::~OptimizationGuideKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OptimizationGuideKeyedService::Initialize(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(optimization_guide_service);

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  top_host_provider_ = GetTopHostProviderIfUserPermitted(browser_context_);
  bool optimization_guide_fetching_enabled = top_host_provider_ != nullptr;
  UMA_HISTOGRAM_BOOLEAN("OptimizationGuide.RemoteFetchingEnabled",
                        optimization_guide_fetching_enabled);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "SyntheticOptimizationGuideRemoteFetching",
      optimization_guide_fetching_enabled ? "Enabled" : "Disabled");
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
      pre_initialized_optimization_types_, optimization_guide_service, profile,
      profile_path, profile->GetPrefs(), database_provider,
      top_host_provider_.get(), url_loader_factory);
  prediction_manager_ = std::make_unique<optimization_guide::PredictionManager>(
      pre_initialized_optimization_targets_, profile_path, database_provider,
      top_host_provider_.get(), url_loader_factory, profile->GetPrefs(),
      profile);
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
  if (hints_manager_) {
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
    }
  }

  if (prediction_manager_ && navigation_data) {
    navigation_data->set_registered_optimization_targets(
        prediction_manager_->registered_optimization_targets());
  }
}

void OptimizationGuideKeyedService::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (hints_manager_)
    hints_manager_->OnNavigationFinish(navigation_redirect_chain);
}

void OptimizationGuideKeyedService::RegisterOptimizationTargets(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets) {
  if (prediction_manager_) {
    prediction_manager_->RegisterOptimizationTargets(optimization_targets);
  } else {
    // If the service has not been initialized yet, keep track of the
    // optimization targets that are registered, so that we can pass them to the
    // prediction manager at initialization.
    pre_initialized_optimization_targets_.insert(
        pre_initialized_optimization_targets_.begin(),
        optimization_targets.begin(), optimization_targets.end());
  }
}

void OptimizationGuideKeyedService::ShouldTargetNavigationAsync(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationTarget optimization_target,
    const base::flat_map<optimization_guide::proto::ClientModelFeature, float>&
        client_model_feature_values,
    optimization_guide::OptimizationGuideTargetDecisionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  if (!prediction_manager_) {
    // We are not initialized yet, so just return unknown.
    std::move(callback).Run(
        optimization_guide::OptimizationGuideDecision::kUnknown);
    return;
  }

  prediction_manager_->ShouldTargetNavigationAsync(
      navigation_handle, optimization_target, client_model_feature_values,
      base::BindOnce(
          &LogOptimizationTargetDecisionAndPassOptimizationGuideDecision,
          optimization_target, std::move(callback)));
}

void OptimizationGuideKeyedService::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  if (hints_manager_) {
    hints_manager_->RegisterOptimizationTypes(optimization_types);
  } else {
    // If the service has not been initialized yet, keep track of the
    // optimization types that are registered, so that we can pass them to the
    // hints manager at initialization.
    pre_initialized_optimization_types_.insert(
        pre_initialized_optimization_types_.begin(), optimization_types.begin(),
        optimization_types.end());
  }
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!hints_manager_) {
    // We are not initialized yet, just return unknown.
    return optimization_guide::OptimizationGuideDecision::kUnknown;
  }

  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager_->CanApplyOptimization(url, /*navigation_id=*/base::nullopt,
                                           optimization_type,
                                           optimization_metadata);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ApplyDecision." +
          optimization_guide::GetStringNameForOptimizationType(
              optimization_type),
      optimization_type_decision);
  return GetOptimizationGuideDecisionFromOptimizationTypeDecision(
      optimization_type_decision);
}

void OptimizationGuideKeyedService::CanApplyOptimizationAsync(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(navigation_handle->IsInMainFrame());

  if (!hints_manager_) {
    std::move(callback).Run(
        optimization_guide::OptimizationGuideDecision::kUnknown,
        /*metadata=*/{});
    return;
  }

  hints_manager_->CanApplyOptimizationAsync(
      navigation_handle->GetURL(), navigation_handle->GetNavigationId(),
      optimization_type, std::move(callback));
}

void OptimizationGuideKeyedService::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const base::Optional<optimization_guide::OptimizationMetadata>& metadata) {
  if (!hints_manager_)
    return;

  hints_manager_->AddHintForTesting(url, optimization_type, metadata);
}

void OptimizationGuideKeyedService::ClearData() {
  if (hints_manager_)
    hints_manager_->ClearFetchedHints();
  if (prediction_manager_)
    prediction_manager_->ClearHostModelFeatures();
}

void OptimizationGuideKeyedService::Shutdown() {
  if (hints_manager_) {
    hints_manager_->Shutdown();
    hints_manager_ = nullptr;
  }
}

void OptimizationGuideKeyedService::UpdateSessionFCP(base::TimeDelta fcp) {
  if (prediction_manager_)
    prediction_manager_->UpdateFCPSessionStatistics(fcp);
}

void OptimizationGuideKeyedService::OverrideTargetDecisionForTesting(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationGuideDecision optimization_guide_decision) {
  if (prediction_manager_) {
    prediction_manager_->OverrideTargetDecisionForTesting(
        optimization_target, optimization_guide_decision);
  }
}
