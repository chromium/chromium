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
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

namespace optimization_guide {
namespace android {
class OptimizationGuideBridge;
}  // namespace android
class OptimizationGuideStore;
class PredictionManager;
class PredictionManagerBrowserTestBase;
class PredictionModelDownloadClient;
class TabUrlProvider;
class TopHostProvider;
}  // namespace optimization_guide

class GURL;
class OptimizationGuideHintsManager;

// Keyed service that can be used to get information received from the remote
// Optimization Guide Service. For regular profiles, this will do the work to
// fetch the necessary information from the remote Optimization Guide Service
// in anticipation for when it is needed. For off the record profiles, this will
// act in a "read-only" mode where it will only serve information that was
// received from the remote Optimization Guide Service when not off the record
// and no information will be retrieved.
class OptimizationGuideKeyedService
    : public KeyedService,
      public optimization_guide::OptimizationGuideDecider {
 public:
  explicit OptimizationGuideKeyedService(
      content::BrowserContext* browser_context);
  ~OptimizationGuideKeyedService() override;

  // optimization_guide::OptimizationGuideDecider implementation:
  void RegisterOptimizationTargets(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets) override;
  void ShouldTargetNavigationAsync(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationGuideTargetDecisionCallback callback)
      override;
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::Optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override;
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override;
  void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

  // Adds hints for a URL with provided metadata to the optimization guide.
  // For testing purposes only. This will flush any callbacks for |url| that
  // were registered via |CanApplyOptimizationAsync|. If no applicable callbacks
  // were registered, this will just add the hint for later use.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const base::Optional<optimization_guide::OptimizationMetadata>& metadata);

  // Override the model file sent to observers of |optimization_target|. For
  // testing purposes only.
  void OverrideTargetModelFileForTesting(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::Optional<optimization_guide::proto::Any>& model_metadata,
      const base::FilePath& file_path);

 private:
  friend class ChromeBrowsingDataRemoverDelegate;
  friend class HintsFetcherBrowserTest;
  friend class OptimizationGuideKeyedServiceBrowserTest;
  friend class OptimizationGuideWebContentsObserver;
  friend class optimization_guide::PredictionModelDownloadClient;
  friend class optimization_guide::PredictionManagerBrowserTestBase;
  friend class optimization_guide::android::OptimizationGuideBridge;

  // Initializes |this|.
  void Initialize();

  // Virtualized for testing.
  virtual OptimizationGuideHintsManager* GetHintsManager();

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  optimization_guide::PredictionManager* GetPredictionManager() {
    return prediction_manager_.get();
  }

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_handle| has started or redirected.
  void OnNavigationStartOrRedirect(
      content::NavigationHandle* navigation_handle);

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_redirect_chain| has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain);

  // Clears data specific to the user.
  void ClearData();

  // KeyedService implementation:
  void Shutdown() override;

  content::BrowserContext* browser_context_;

  // The store of hints.
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;

  // The store of optimization target prediction models and features.
  std::unique_ptr<optimization_guide::OptimizationGuideStore>
      prediction_model_and_features_store_;

  // Manages the storing, loading, and evaluating of optimization target
  // prediction models.
  std::unique_ptr<optimization_guide::PredictionManager> prediction_manager_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // The tab URL provider to use for fetching information for the user's active
  // tabs. Will be null if the user is off the record.
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedService);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
