// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include "base/test/gmock_callback_support.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class TestingPrefServiceSimple;

// Mocks the opt guide service, to be used in unittests.
//
// Can be used with `ChromeRenderViewHostTestHarness` based tests.
//
// For non ChromeRenderViewHostTestHarness based tests set the local state using
// `MockOptimizationGuideKeyedService::Initialize()` and then reset using
// `MockOptimizationGuideKeyedService::TearDown()`.
class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  static void Initialize(TestingPrefServiceSimple* local_state);
  static void InitializeWithExistingTestLocalState();
  static void TearDown();
  static void ResetForTesting();

  MockOptimizationGuideKeyedService();
  ~MockOptimizationGuideKeyedService() override;

  void Shutdown() override;

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
       optimization_guide::proto::RequestContextMetadata*
           request_context_metadata),
      (override));
  MOCK_METHOD(std::unique_ptr<Session>,
              StartSession,
              (optimization_guide::proto::ModelExecutionFeature feature));
  MOCK_METHOD(
      void,
      ExecuteModel,
      (optimization_guide::proto::ModelExecutionFeature,
       const google::protobuf::MessageLite&,
       optimization_guide::OptimizationGuideModelExecutionResultCallback));
  MOCK_METHOD(bool,
              ShouldFeatureBeCurrentlyEnabledForUser,
              (optimization_guide::proto::ModelExecutionFeature),
              (const));
  MOCK_METHOD(bool,
              ShouldFeatureBeCurrentlyAllowedForLogging,
              (optimization_guide::proto::ModelExecutionFeature feature),
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
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MOCK_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
