// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace optimization_guide {
class OptimizationGuideService;
class TopHostProvider;
class PredictionManager;
}  // namespace optimization_guide

class OptimizationGuideHintsManager;

class OptimizationGuideKeyedService
    : public KeyedService,
      public optimization_guide::OptimizationGuideDecider {
 public:
  explicit OptimizationGuideKeyedService(
      content::BrowserContext* browser_context);
  ~OptimizationGuideKeyedService() override;

  // Initializes the service. |optimization_guide_service| is the
  // Optimization Guide Service that is being listened to and is guaranteed to
  // outlive |this|. |profile_path| is the path to user data on disk.
  void Initialize(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& profile_path);

  OptimizationGuideHintsManager* GetHintsManager() {
    return hints_manager_.get();
  }

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  optimization_guide::PredictionManager* GetPredictionManager() {
    return prediction_manager_.get();
  }

  // Prompts the load of the hint for the navigation, if there is at least one
  // optimization type registered and there is a hint available.
  void MaybeLoadHintForNavigation(content::NavigationHandle* navigation_handle);

  // Clears data specific to the user.
  void ClearData();

  // optimization_guide::OptimizationGuideDecider implementation:
  void RegisterOptimizationTypesAndTargets(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types,
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets) override;
  optimization_guide::OptimizationGuideDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target)
      override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;
  optimization_guide::OptimizationGuideDecision
  ShouldTargetNavigationAndCanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

  // KeyedService implementation:
  void Shutdown() override;

  // Updates |prediction_manager_| with the provided fcp value.
  void UpdateSessionFCP(base::TimeDelta fcp);

 private:
  content::BrowserContext* browser_context_;

  // The optimization types registered prior to initialization.
  std::vector<optimization_guide::proto::OptimizationType>
      pre_initialized_optimization_types_;

  // The optimization targets registered prior to initialization.
  std::vector<optimization_guide::proto::OptimizationTarget>
      pre_initialized_optimization_targets_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;

  // Manages the storing, loading, and evaluating of optimization target
  // prediction models.
  std::unique_ptr<optimization_guide::PredictionManager> prediction_manager_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedService);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
