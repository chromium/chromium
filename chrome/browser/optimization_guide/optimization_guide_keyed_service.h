// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/bookmarks/android/bookmark_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class BrowserContext;
}  // namespace content

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace optimization_guide {
class ChromeHintsManager;
class ModelExecutionEnabledBrowserTest;
class ModelExecutionLiveTest;
class ModelExecutionManager;
class ModelInfo;
class ModelQualityLogsUploaderService;
class ModelValidatorKeyedService;
class OnDeviceModelAvailabilityObserver;
class OnDeviceModelComponentStateManager;
class OptimizationGuideStore;
class OptimizationGuideKeyedServiceBrowserTest;
class PredictionManager;
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
      public optimization_guide::OptimizationGuideModelExecutor,
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
      optimization_guide::OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override;

  // optimization_guide::OptimizationGuideModelExecutor implementation:
  bool CanCreateOnDeviceSession(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason*
          on_device_model_eligibility_reason) override;
  std::unique_ptr<Session> StartSession(
      optimization_guide::ModelBasedCapabilityKey feature,
      const std::optional<optimization_guide::SessionConfigParams>&
          config_params) override;
  void ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultCallback
          callback) override;
  void AddOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelAvailabilityObserver* observer) override;
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelAvailabilityObserver* observer) override;

  // Returns true if the `feature` should be currently enabled for this user.
  // Note that the return value here may not match the feature enable state on
  // chrome settings page since the latter takes effect on browser restart.
  // Virtualized for testing.
  virtual bool ShouldFeatureBeCurrentlyEnabledForUser(
      optimization_guide::UserVisibleFeatureKey feature) const;

  // Returns true if signed-in user is allowed to execute models, disregarding
  // the `allow_unsigned_user` switch.
  bool ShouldFeatureAllowModelExecutionForSignedInUser(
      optimization_guide::UserVisibleFeatureKey feature) const;

  // Returns whether the `feature` should be currently allowed for showing the
  // Feedback UI (and sending Feedback reports).
  virtual bool ShouldFeatureBeCurrentlyAllowedForFeedback(
      optimization_guide::proto::LogAiDataRequest::FeatureCase feature) const;

  // Returns true if the opt-in setting should be shown for this profile for
  // given `feature`. This should only be called by settings UX.
  bool IsSettingVisible(
      optimization_guide::UserVisibleFeatureKey feature) const;

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

 private:
  friend class BrowserView;
  friend class ChromeBrowserMainExtraPartsOptimizationGuide;
  friend class ChromeBrowsingDataRemoverDelegate;
  friend class HintsFetcherBrowserTest;
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

  // Evaluates and logs the device performance class.
  static void DeterminePerformanceClass(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  // Initializes |this|.
  void Initialize();

  // Virtualized for testing.
  virtual optimization_guide::ChromeHintsManager* GetHintsManager();

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  optimization_guide::PredictionManager* GetPredictionManager() {
    return prediction_manager_.get();
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

  // Returns whether all conditions are met to show the IPH promo for
  // experimental AI.
  bool ShouldShowExperimentalAIPromo() const;

  download::BackgroundDownloadService* BackgroundDownloadServiceProvider();

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
  scoped_refptr<optimization_guide::OnDeviceModelComponentStateManager>
      on_device_component_manager_;

  // The tab URL provider to use for fetching information for the user's active
  // tabs. Will be null if the user is off the record.
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<optimization_guide::ChromeHintsManager> hints_manager_;

  // Manages the storing, loading, and evaluating of optimization target
  // prediction models.
  std::unique_ptr<optimization_guide::PredictionManager> prediction_manager_;

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
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
