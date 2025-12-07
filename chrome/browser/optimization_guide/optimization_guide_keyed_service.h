// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/bookmarks/android/bookmark_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class BrowserContext;
}  // namespace content

namespace contextual_cueing {
class ZeroStateSuggestionsPageData;
}  // namespace contextual_cueing

namespace glic {
class GlicPageContextEligibilityObserver;
}  // namespace glic

namespace on_device_internals {
class PageHandler;
}

namespace optimization_guide {
class ChromeHintsManager;
class ModelExecutionEnabledBrowserTest;
class ModelExecutionLiveTest;
class ModelExecutionManager;
class ModelInfo;
class ModelQualityLogsUploaderService;
class ModelValidatorKeyedService;
class OnDeviceModelAvailabilityObserver;
class OptimizationGuideStore;
class OptimizationGuideKeyedServiceBrowserTest;
class PredictionManagerBrowserTestBase;
class PredictionModelDownloadClient;
class PredictionModelStoreBrowserTestBase;
class PushNotificationManager;
class TabUrlProvider;
class TopHostProvider;

#if BUILDFLAG(IS_ANDROID)
namespace android {
class OptimizationGuideBridge;
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace optimization_guide

class ChromeBrowserMainExtraPartsOptimizationGuide;
class GURL;
class OptimizationGuideLogger;
class OptimizationGuideNavigationData;
class Profile;

namespace settings {
class SettingsUI;
}  // namespace settings

// Keyed service that can be used to get information received from the remote
// Optimization Guide Service. For regular profiles, this will do the work to
// fetch the necessary information from the remote Optimization Guide Service
// in anticipation for when it is needed. For off the record profiles, this will
// act in a "read-only" mode where it will only serve information that was
// received from the remote Optimization Guide Service when not off the record
// and no information will be retrieved.
class OptimizationGuideKeyedService
    : public KeyedService,
      public optimization_guide::OptimizationGuideDecider,
      public optimization_guide::OptimizationGuideModelProvider,
      public optimization_guide::OnDeviceCapability,
      public optimization_guide::RemoteModelExecutor,
      public ProfileObserver {
 public:
  explicit OptimizationGuideKeyedService(
      content::BrowserContext* browser_context);

  OptimizationGuideKeyedService(const OptimizationGuideKeyedService&) = delete;
  OptimizationGuideKeyedService& operator=(
      const OptimizationGuideKeyedService&) = delete;

  ~OptimizationGuideKeyedService() override;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
#endif

  // Constructs a ModelBrokerClient with remote fallback capability.
  virtual std::unique_ptr<optimization_guide::ModelBrokerClient>
  CreateModelBrokerClient();

  // optimization_guide::OptimizationGuideDecider implementation:
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override;
  void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

  // optimization_guide::OptimizationGuideModelProvider implementation:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override;

  // optimization_guide::RemoteModelExecutor implementation:
  void ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      const optimization_guide::ModelExecutionOptions& options,
      optimization_guide::OptimizationGuideModelExecutionResultCallback
          callback) override;

  // optimization_guide::OnDeviceCapability
  // implementation:
  void BindModelBroker(
      mojo::PendingReceiver<optimization_guide::mojom::ModelBroker> receiver)
      override;
  std::unique_ptr<optimization_guide::OnDeviceSession> StartSession(
      optimization_guide::mojom::OnDeviceFeature feature,
      const optimization_guide::SessionConfigParams& config_params,
      base::WeakPtr<OptimizationGuideLogger> logger) override;
  void AddOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::mojom::OnDeviceFeature feature,
      optimization_guide::OnDeviceModelAvailabilityObserver* observer) override;
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::mojom::OnDeviceFeature feature,
      optimization_guide::OnDeviceModelAvailabilityObserver* observer) override;
  on_device_model::Capabilities GetOnDeviceCapabilities() override;
  optimization_guide::OnDeviceModelEligibilityReason
  GetOnDeviceModelEligibility(
      optimization_guide::mojom::OnDeviceFeature feature) override;
  void GetOnDeviceModelEligibilityAsync(
      optimization_guide::mojom::OnDeviceFeature feature,
      const on_device_model::Capabilities& capabilities,
      base::OnceCallback<
          void(optimization_guide::OnDeviceModelEligibilityReason)> callback)
      override;
  std::optional<optimization_guide::SamplingParamsConfig>
  GetSamplingParamsConfig(
      optimization_guide::mojom::OnDeviceFeature feature) override;
  std::optional<const optimization_guide::proto::Any> GetFeatureMetadata(
      optimization_guide::mojom::OnDeviceFeature feature) override;

  // Returns true if the `feature` should be currently enabled for this user.
  // Note that the return value here may not match the feature enable state on
  // chrome settings page since the latter takes effect on browser restart.
  // Virtualized for testing.
  virtual bool ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::UserVisibleFeatureKey feature) const;

  // Returns true if signed-in user is allowed to execute models, disregarding
  // the `allow_unsigned_user` switch.
  virtual bool ShouldFeatureAllowModelExecutionForSignedInUser(
      optimization_guide::UserVisibleFeatureKey feature) const;

  // Returns whether the `feature` should be currently allowed for showing the
  // Feedback UI (and sending Feedback reports).
  virtual bool ShouldFeatureBeCurrentlyAllowedForFeedback(
      optimization_guide::proto::LogAiDataRequest::FeatureCase feature) const;

  // Returns true if the opt-in setting should be shown for this profile for
  // given `feature`. This should only be called by settings UX.
  bool IsSettingVisible(
      optimization_guide::UserVisibleFeatureKey feature) const;

  // Returns true if the user passes all sign-in checks and is allowed to use
  // model execution. This does not perform any feature related checks such as
  // allowed by enterprise policy.
  virtual bool ShouldModelExecutionBeAllowedForUser() const;

  // Adds `observer` which can observe the change in feature settings.
  void AddModelExecutionSettingsEnabledObserver(
      optimization_guide::SettingsEnabledObserver* observer);

  // Removes `observer`.
  void RemoveModelExecutionSettingsEnabledObserver(
      optimization_guide::SettingsEnabledObserver* observer);

  // Adds hints for a URL with provided metadata to the optimization guide.
  // For testing purposes only. This will flush any callbacks for |url| that
  // were registered via |CanApplyOptimization|. If no applicable callbacks
  // were registered, this will just add the hint for later use.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const std::optional<optimization_guide::OptimizationMetadata>& metadata);

  // Adds hints for a URL for the given optimization types to the optimization
  // guide. For testing purposes only. This will flush any callbacks for |url|
  // that were registered via |CanApplyOptimization|. If no applicable callbacks
  // were registered, this will just add the hint for later use.
  void AddHintWithMultipleOptimizationsForTesting(
      const GURL& url,
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types);

  // Adds hints for a URL with provided metadata to the optimization guide.
  // Hints added via this method will work for `CanApplyOptimizationOnDemand`
  // calls. For testing purposes only.
  void AddOnDemandHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const optimization_guide::OptimizationGuideDecisionWithMetadata&
          decision);

  // Adds a model execution result to be provided when an execution request
  // comes in with the given `feature`.
  void AddExecutionResultForTesting(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OptimizationGuideModelExecutionResult result);

  // Override the model file sent to observers of |optimization_target|. Use
  // |TestModelInfoBuilder| to construct the model metadata. For
  // testing purposes only.
  void OverrideTargetModelForTesting(
      optimization_guide::proto::OptimizationTarget optimization_target,
      std::unique_ptr<optimization_guide::ModelInfo> model_info);

  void SetModelQualityLogsUploaderServiceForTesting(
      std::unique_ptr<optimization_guide::ModelQualityLogsUploaderService>
          uploader);

  void AllowUnsignedUserForTesting(
      optimization_guide::UserVisibleFeatureKey feature);

  // Creates the platform specific push notification manager. May returns
  // nullptr for desktop or when the push notification feature is disabled.
  static std::unique_ptr<optimization_guide::PushNotificationManager>
  MaybeCreatePushNotificationManager(Profile* profile);

  OptimizationGuideLogger* GetOptimizationGuideLogger() {
    return optimization_guide_logger_.get();
  }

  optimization_guide::ModelQualityLogsUploaderService*
  GetModelQualityLogsUploaderService() {
    return model_quality_logs_uploader_service_.get();
  }

  virtual optimization_guide::ModelExecutionFeaturesController*
  GetModelExecutionFeaturesController();

 private:
  friend class ChromeBrowserMainExtraPartsOptimizationGuide;
  friend class ChromeBrowsingDataRemoverDelegate;
  friend class contextual_cueing::ZeroStateSuggestionsPageData;
  friend class glic::GlicPageContextEligibilityObserver;
  friend class HintsFetcherBrowserTest;
  friend class on_device_internals::PageHandler;
  friend class OptimizationGuideInternalsUI;
  friend class OptimizationGuideMessageHandler;
  friend class OptimizationGuideWebContentsObserver;
  friend class optimization_guide::ModelExecutionEnabledBrowserTest;
  friend class optimization_guide::ModelExecutionLiveTest;
  friend class optimization_guide::ModelValidatorKeyedService;
  friend class optimization_guide::OptimizationGuideKeyedServiceBrowserTest;
  friend class optimization_guide::PredictionManagerBrowserTestBase;
  friend class optimization_guide::PredictionModelDownloadClient;
  friend class optimization_guide::PredictionModelStoreBrowserTestBase;
  friend class PersonalizedHintsFetcherBrowserTest;
  friend class settings::SettingsUI;

#if BUILDFLAG(IS_ANDROID)
  friend class optimization_guide::android::OptimizationGuideBridge;
#endif  // BUILDFLAG(IS_ANDROID)

  // Allows tests to override the value of `version_info::IsOfficialBuild()`.
  static void SetIsOfficialBuildForTesting(bool is_official_build);

  // Initializes |this|.
  void Initialize();

  void InitializeModelExecution(Profile* profile);

  // Virtualized for testing.
  virtual optimization_guide::ChromeHintsManager* GetHintsManager();

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  optimization_guide::PredictionManager* GetPredictionManager() {
    return &GetGlobalState().prediction_manager();
  }

  optimization_guide::OptimizationGuideGlobalState& GetGlobalState() {
    if (!optimization_guide_global_state_) {
      optimization_guide_global_state_ =
          optimization_guide::OptimizationGuideGlobalState::CreateOrGet();
    }
    return *optimization_guide_global_state_;
  }

  optimization_guide::ModelExecutionManager* GetModelExecutionManager() {
    return model_execution_manager_.get();
  }

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_data| has started or redirected. Virtual for testing.
  virtual void OnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data);

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_redirect_chain| has finished. Virtual for testing.
  virtual void OnNavigationFinish(
      const std::vector<GURL>& navigation_redirect_chain);

  // Clears data specific to the user.
  void ClearData();

  // KeyedService implementation:
  void Shutdown() override;

  // ProfileObserver implementation:
  void OnProfileInitializationComplete(Profile* profile) override;

  // optimization_guide::NewOptimizationGuideDecider implementation:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback,
      std::optional<optimization_guide::proto::RequestContextMetadata>
          request_context_metadata = std::nullopt) override;

  bool ComponentUpdatesEnabledProvider() const;

  // Records synthetic field trial for `feature` with trial name appended with
  // `feature_name`.
  void RecordModelExecutionFeatureSyntheticFieldTrial(
      optimization_guide::UserVisibleFeatureKey feature,
      std::string_view feature_name);

  raw_ptr<content::BrowserContext> browser_context_;

  // The store of hints.
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Must outlive `prediction_manager_` and `hints_manager_`.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // Keep a reference to this so it stays alive.
  // Even though this can be obtained from OptimizationGuideGlobalFeature, we
  // keep a reference here to handle difference in lifetime issues. At least in
  // tests, the GlobalFeatures is destroyed before the Profile.
  scoped_refptr<optimization_guide::OptimizationGuideGlobalState>
      optimization_guide_global_state_;

  // The tab URL provider to use for fetching information for the user's active
  // tabs. Will be null if the user is off the record.
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<optimization_guide::ChromeHintsManager> hints_manager_;

  // Manages the model execution. Not created for off the record profiles.
  std::unique_ptr<optimization_guide::ModelExecutionManager>
      model_execution_manager_;

  std::unique_ptr<optimization_guide::ModelExecutionFeaturesController>
      model_execution_features_controller_;

  // Manages the model quality logs uploader service. Not created for off the
  // record profiles.
  std::unique_ptr<optimization_guide::ModelQualityLogsUploaderService>
      model_quality_logs_uploader_service_;

#if BUILDFLAG(IS_ANDROID)
  // Manage and fetch the java object that wraps this OptimizationGuide on
  // android.
  std::unique_ptr<optimization_guide::android::OptimizationGuideBridge>
      android_bridge_;
#endif

  // Used to observe profile initialization event.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<OptimizationGuideKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
