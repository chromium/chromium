// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/model_execution/chrome_on_device_model_service_controller.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/pref_names.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/core/command_line_top_host_provider.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/core/tab_url_provider.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/commerce/price_tracking/android/price_tracking_notification_bridge.h"
#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"
#else
#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"
#endif

namespace {

const char kOldOptimizationGuidePredictionModelAndFeaturesStore[] =
    "optimization_guide_model_and_features_store";

// Deletes old store paths that were written in incorrect locations.
void DeleteOldStorePaths(const base::FilePath& profile_path) {
  // Added 05/2022.

  // Delete profile_path/optimization_guide_model_and_features_store/...
  // as it contains pointers to the bad model download location.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeletePathRecursivelyCallback(profile_path.AppendASCII(
          kOldOptimizationGuidePredictionModelAndFeaturesStore)));

  // Delete the Chrome-wide model download location that wasn't previously
  // getting cleaned up when profiles were getting deleted.
  base::FilePath models_dir;
  base::PathService::Get(chrome::DIR_OPTIMIZATION_GUIDE_PREDICTION_MODELS,
                         &models_dir);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeletePathRecursivelyCallback(models_dir));
}

// Returns the profile to use for when setting up the keyed service when the
// profile is Off-The-Record. For guest profiles, returns a loaded profile if
// one exists, otherwise just the original profile of the OTR profile. Note:
// guest profiles are off-the-record and "original" profiles.
Profile* GetProfileForOTROptimizationGuide(Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->IsOffTheRecord());

  if (profile->IsGuestSession()) {
    // Guest sessions need to rely on the stores from real profiles
    // as guest profiles cannot fetch or store new models. Note: only
    // loaded profiles should be used as we do not want to force load
    // another profile as that can lead to start up regressions.
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    if (!profiles.empty()) {
      return profiles[0];
    }
  }
  return profile->GetOriginalProfile();
}

}  // namespace

// static
std::unique_ptr<optimization_guide::PushNotificationManager>
OptimizationGuideKeyedService::MaybeCreatePushNotificationManager(
    Profile* profile) {
  if (optimization_guide::features::IsPushNotificationsEnabled()) {
    auto push_notification_manager =
        std::make_unique<optimization_guide::PushNotificationManager>();
#if BUILDFLAG(IS_ANDROID)
    push_notification_manager->AddObserver(
        PriceTrackingNotificationBridge::GetForBrowserContext(profile));
#endif
    return push_notification_manager;
  }
  return nullptr;
}

OptimizationGuideKeyedService::OptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Initialize();
}

OptimizationGuideKeyedService::~OptimizationGuideKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

download::BackgroundDownloadService*
OptimizationGuideKeyedService::BackgroundDownloadServiceProvider() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  return BackgroundDownloadServiceFactory::GetForKey(profile->GetProfileKey());
}

bool OptimizationGuideKeyedService::ComponentUpdatesEnabledProvider() const {
  return g_browser_process->local_state()->GetBoolean(
      ::prefs::kComponentUpdatesEnabled);
}

void OptimizationGuideKeyedService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  base::FilePath profile_path = profile->GetOriginalProfile()->GetPath();

  // We have different behavior if |this| is created for an incognito profile.
  // For incognito profiles, we act in "read-only" mode of the original
  // profile's store and do not fetch any new hints or models.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store;
  base::WeakPtr<optimization_guide::OptimizationGuideStore>
      prediction_model_and_features_store;
  base::FilePath model_downloads_dir;
  if (profile->IsOffTheRecord()) {
    OptimizationGuideKeyedService* original_ogks =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            GetProfileForOTROptimizationGuide(profile));
    DCHECK(original_ogks);
    hint_store = original_ogks->GetHintsManager()->hint_store();
    prediction_model_and_features_store =
        original_ogks->GetPredictionManager()->model_and_features_store();
  } else {
    // Use the database associated with the original profile.
    auto* proto_db_provider = profile->GetOriginalProfile()
                                  ->GetDefaultStoragePartition()
                                  ->GetProtoDatabaseProvider();
    url_loader_factory = profile->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess();

    // Only create a top host provider from the command line if provided.
    top_host_provider_ =
        optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();

    bool optimization_guide_fetching_enabled =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            profile->IsOffTheRecord(), profile->GetPrefs());
    UMA_HISTOGRAM_BOOLEAN("OptimizationGuide.RemoteFetchingEnabled",
                          optimization_guide_fetching_enabled);
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOptimizationGuideRemoteFetching",
        optimization_guide_fetching_enabled ? "Enabled" : "Disabled",
        variations::SyntheticTrialAnnotationMode::kCurrentLog);

#if BUILDFLAG(IS_ANDROID)
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
                      {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
                  profile->GetPrefs())
            : nullptr;
    hint_store = hint_store_ ? hint_store_->AsWeakPtr() : nullptr;

    if (!optimization_guide::features::IsInstallWideModelStoreEnabled()) {
      // Do not explicitly hand off the model downloads directory to
      // off-the-record profiles. Underneath the hood, this variable is only
      // used in non off-the-record profiles to know where to download the model
      // files to. Off-the-record profiles read the model locations from the
      // original profiles they are associated with.
      model_downloads_dir = profile_path.Append(
          optimization_guide::kOptimizationGuidePredictionModelDownloads);
      prediction_model_and_features_store_ =
          std::make_unique<optimization_guide::OptimizationGuideStore>(
              proto_db_provider,
              profile_path.Append(
                  optimization_guide::
                      kOptimizationGuidePredictionModelMetadataStore),
              model_downloads_dir,
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
              profile->GetPrefs());
      prediction_model_and_features_store =
          prediction_model_and_features_store_->AsWeakPtr();
    }
  }

  optimization_guide_logger_ = std::make_unique<OptimizationGuideLogger>();
  hints_manager_ = std::make_unique<optimization_guide::ChromeHintsManager>(
      profile, profile->GetPrefs(), hint_store, top_host_provider_.get(),
      tab_url_provider_.get(), url_loader_factory,
      MaybeCreatePushNotificationManager(profile),
      IdentityManagerFactory::GetForProfile(profile),
      optimization_guide_logger_.get());

  prediction_manager_ = std::make_unique<optimization_guide::PredictionManager>(
      prediction_model_and_features_store,
      optimization_guide::features::IsInstallWideModelStoreEnabled()
          ? optimization_guide::PredictionModelStore::GetInstance()
          : nullptr,
      url_loader_factory, profile->GetPrefs(), profile->IsOffTheRecord(),
      g_browser_process->GetApplicationLocale(), model_downloads_dir,
      optimization_guide_logger_.get(),
      base::BindOnce(
          &OptimizationGuideKeyedService::BackgroundDownloadServiceProvider,
          // It's safe to use |base::Unretained(this)| here because
          // |this| owns |prediction_manager_|.
          base::Unretained(this)),
      base::BindRepeating(
          &OptimizationGuideKeyedService::ComponentUpdatesEnabledProvider,
          // It's safe to use |base::Unretained(this)| here because
          // |this| owns |prediction_manager_|.
          base::Unretained(this)));

  if (!profile->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    model_execution_manager_ =
        std::make_unique<optimization_guide::ModelExecutionManager>(
            url_loader_factory, IdentityManagerFactory::GetForProfile(profile),
            std::make_unique<
                optimization_guide::ChromeOnDeviceModelServiceController>(),
            optimization_guide_logger_.get());
  }

  // Register for profile initialization event to initialize the model
  // downloads.
  profile_observation_.Observe(profile);

  // Some previous paths were written in incorrect locations. Delete the
  // old paths.
  //
  // TODO(crbug.com/1328981): Remove this code in 05/2023 since it should be
  // assumed that all clients that had the previous path have had their previous
  // stores deleted.
  DeleteOldStorePaths(profile_path);

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
      optimization_guide_logger_,
      "OptimizationGuide: KeyedService is initalized");

  optimization_guide::LogFeatureFlagsInfo(optimization_guide_logger_.get(),
                                          profile->IsOffTheRecord(),
                                          profile->GetPrefs());
}

optimization_guide::ChromeHintsManager*
OptimizationGuideKeyedService::GetHintsManager() {
  return hints_manager_.get();
}

void OptimizationGuideKeyedService::OnNavigationStartOrRedirect(
    OptimizationGuideNavigationData* navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          hints_manager_->registered_optimization_types();
  if (!registered_optimization_types.empty()) {
    hints_manager_->OnNavigationStartOrRedirect(navigation_data,
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

void OptimizationGuideKeyedService::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const absl::optional<optimization_guide::proto::Any>& model_metadata,
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
      hints_manager_->CanApplyOptimization(url, optimization_type,
                                           optimization_metadata);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ApplyDecision." +
          optimization_guide::GetStringNameForOptimizationType(
              optimization_type),
      optimization_type_decision);
  return optimization_guide::ChromeHintsManager::
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(
          optimization_type_decision);
}

void OptimizationGuideKeyedService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  hints_manager_->CanApplyOptimization(url, optimization_type,
                                       std::move(callback));
}

void OptimizationGuideKeyedService::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types,
    optimization_guide::proto::RequestContext request_context,
    optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request_context !=
         optimization_guide::proto::RequestContext::CONTEXT_UNSPECIFIED);

  hints_manager_->CanApplyOptimizationOnDemand(urls, optimization_types,
                                               request_context, callback);
}

void OptimizationGuideKeyedService::ExecuteModel(
    optimization_guide::proto::ModelExecutionFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!model_execution_manager_) {
    std::move(callback).Run(base::unexpected(
        optimization_guide::OptimizationGuideModelExecutionError::
            FromModelExecutionError(
                optimization_guide::OptimizationGuideModelExecutionError::
                    ModelExecutionError::kGenericFailure)));
    return;
  }
  model_execution_manager_->ExecuteModel(feature, request_metadata,
                                         std::move(callback));
}

void OptimizationGuideKeyedService::OnProfileInitializationComplete(
    Profile* profile) {
  DCHECK(profile_observation_.IsObservingSource(profile));
  profile_observation_.Reset();

  if (!optimization_guide::features::IsInstallWideModelStoreEnabled()) {
    return;
  }

  if (profile->IsOffTheRecord()) {
    return;
  }

  GetPredictionManager()->MaybeInitializeModelDownloads(
      BackgroundDownloadServiceProvider());
}

void OptimizationGuideKeyedService::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const absl::optional<optimization_guide::OptimizationMetadata>& metadata) {
  hints_manager_->AddHintForTesting(url, optimization_type, metadata);
}

void OptimizationGuideKeyedService::ClearData() {
  hints_manager_->ClearFetchedHints();
}

void OptimizationGuideKeyedService::Shutdown() {
  hints_manager_->Shutdown();
}

void OptimizationGuideKeyedService::OverrideTargetModelForTesting(
    optimization_guide::proto::OptimizationTarget optimization_target,
    std::unique_ptr<optimization_guide::ModelInfo> model_info) {
  prediction_manager_->OverrideTargetModelForTesting(  // IN-TEST
      optimization_target, std::move(model_info));
}
