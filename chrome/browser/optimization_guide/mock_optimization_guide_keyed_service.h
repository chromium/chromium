// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mocks the opt guide service, to be used in unittests.
//
// Can be used with `ChromeRenderViewHostTestHarness` based tests.
class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  static void InitializeWithExistingTestLocalState();
  static void ResetForTesting();

  MockOptimizationGuideKeyedService();
  ~MockOptimizationGuideKeyedService() override;

  void Shutdown() override;

  MOCK_METHOD(std::unique_ptr<optimization_guide::ModelBrokerClient>,
              CreateModelBrokerClient,
              (),
              (override));
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>& urls,
       const base::flat_set<optimization_guide::proto::OptimizationType>&
           optimization_types,
       optimization_guide::proto::RequestContext request_context,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
           callback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
  MOCK_METHOD(std::unique_ptr<Session>,
              StartSession,
              (optimization_guide::ModelBasedCapabilityKey feature,
               const std::optional<optimization_guide::SessionConfigParams>&
                   config_params));
  MOCK_METHOD(
      void,
      ExecuteModel,
      (optimization_guide::ModelBasedCapabilityKey,
       const google::protobuf::MessageLite&,
       const std::optional<base::TimeDelta>&,
       optimization_guide::OptimizationGuideModelExecutionResultCallback));
  MOCK_METHOD(void,
              AddOnDeviceModelAvailabilityChangeObserver,
              (optimization_guide::ModelBasedCapabilityKey feature,
               optimization_guide::OnDeviceModelAvailabilityObserver* observer),
              (override));
  MOCK_METHOD(void,
              RemoveOnDeviceModelAvailabilityChangeObserver,
              (optimization_guide::ModelBasedCapabilityKey feature,
               optimization_guide::OnDeviceModelAvailabilityObserver* observer),
              (override));
  MOCK_METHOD(on_device_model::Capabilities,
              GetOnDeviceCapabilities,
              (),
              (override));
  MOCK_METHOD(bool,
              ShouldFeatureBeCurrentlyEnabledForUser,
              (optimization_guide::UserVisibleFeatureKey),
              (const, override));
  MOCK_METHOD(bool,
              ShouldFeatureAllowModelExecutionForSignedInUser,
              (optimization_guide::UserVisibleFeatureKey),
              (const, override));
  MOCK_METHOD(bool,
              ShouldModelExecutionBeAllowedForUser,
              (),
              (const, override));
  MOCK_METHOD(
      bool,
      ShouldFeatureBeCurrentlyAllowedForFeedback,
      (optimization_guide::proto::LogAiDataRequest::FeatureCase feature),
      (const));
  MOCK_METHOD(void,
              UploadModelQualityLogs,
              (std::unique_ptr<optimization_guide::ModelQualityLogEntry>));
  MOCK_METHOD(void,
              AddObserverForOptimizationTargetModel,
              (optimization_guide::proto::OptimizationTarget,
               const std::optional<optimization_guide::proto::Any>&,
               optimization_guide::OptimizationTargetModelObserver*),
              (override));
  MOCK_METHOD(void,
              RemoveObserverForOptimizationTargetModel,
              (optimization_guide::proto::OptimizationTarget,
               optimization_guide::OptimizationTargetModelObserver*),
              (override));

  MOCK_METHOD(void,
              OnNavigationStartOrRedirect,
              (OptimizationGuideNavigationData*),
              (override));

  MOCK_METHOD(void, OnNavigationFinish, (const std::vector<GURL>&), (override));

  MOCK_METHOD(optimization_guide::OnDeviceModelEligibilityReason,
              GetOnDeviceModelEligibility,
              (optimization_guide::ModelBasedCapabilityKey),
              (override));

  MOCK_METHOD(void,
              GetOnDeviceModelEligibilityAsync,
              (optimization_guide::ModelBasedCapabilityKey,
               const on_device_model::Capabilities&,
               base::OnceCallback<
                   void(optimization_guide::OnDeviceModelEligibilityReason)>),
              (override));

  MOCK_METHOD(std::optional<optimization_guide::SamplingParamsConfig>,
              GetSamplingParamsConfig,
              (optimization_guide::ModelBasedCapabilityKey),
              (override));

  MOCK_METHOD(std::optional<const optimization_guide::proto::Any>,
              GetFeatureMetadata,
              (optimization_guide::ModelBasedCapabilityKey),
              (override));
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
