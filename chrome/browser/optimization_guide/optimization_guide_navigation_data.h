// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

// A representation of optimization guide information related to a navigation.
// Metrics will be recorded upon this object's destruction.
class OptimizationGuideNavigationData {
 public:
  explicit OptimizationGuideNavigationData(int64_t navigation_id);
  ~OptimizationGuideNavigationData();

  OptimizationGuideNavigationData(
      const OptimizationGuideNavigationData& other) = delete;
  OptimizationGuideNavigationData& operator=(
      const OptimizationGuideNavigationData&) = delete;

  // Returns the OptimizationGuideNavigationData for |navigation_handle|. Will
  // return nullptr if one cannot be created for it for any reason.
  static OptimizationGuideNavigationData* GetFromNavigationHandle(
      content::NavigationHandle* navigation_handle);

  base::WeakPtr<OptimizationGuideNavigationData> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // The navigation ID of the navigation handle that this data is associated
  // with.
  int64_t navigation_id() const { return navigation_id_; }

  // The optimization types that were registered at the start of the navigation.
  base::flat_set<optimization_guide::proto::OptimizationType>
  registered_optimization_types() const {
    return registered_optimization_types_;
  }
  void set_registered_optimization_types(
      base::flat_set<optimization_guide::proto::OptimizationType>
          registered_optimization_types) {
    registered_optimization_types_ = registered_optimization_types;
  }

  // The optimization targets that were registered at the start of the
  // navigation.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
  registered_optimization_targets() const {
    return registered_optimization_targets_;
  }
  void set_registered_optimization_targets(
      base::flat_set<optimization_guide::proto::OptimizationTarget>
          registered_optimization_targets) {
    registered_optimization_targets_ = registered_optimization_targets;
  }

  // The duration between the fetch for a hint for the navigation going out to
  // when it was received by the client if a fetch was initiated for the
  // navigation.
  base::Optional<base::TimeDelta> hints_fetch_latency() const;
  void set_hints_fetch_start(base::TimeTicks hints_fetch_start) {
    hints_fetch_start_ = hints_fetch_start;
  }
  void set_hints_fetch_end(base::TimeTicks hints_fetch_end) {
    hints_fetch_end_ = hints_fetch_end;
  }

  // The status for whether a hint for the page load was attempted to be fetched
  // from the remote Optimization Guide Service.
  base::Optional<optimization_guide::RaceNavigationFetchAttemptStatus>
  hints_fetch_attempt_status() const {
    return hints_fetch_attempt_status_;
  }
  void set_hints_fetch_attempt_status(
      optimization_guide::RaceNavigationFetchAttemptStatus
          hints_fetch_attempt_status) {
    hints_fetch_attempt_status_ = hints_fetch_attempt_status;
  }

 private:
  // Records metrics based on data currently held in |this|.
  void RecordMetrics() const;

  // Records the OptimizationGuide UKM event based on data currently held in
  // |this|.
  void RecordOptimizationGuideUKM() const;

  // The navigation ID of the navigation handle that this data is associated
  // with.
  const int64_t navigation_id_;

  // The optimization types that were registered at the start of the navigation.
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  // The optimization targets that were registered at the start of the
  // navigation.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      registered_optimization_targets_;

  // The map from optimization type to the last decision made for that type.
  base::flat_map<optimization_guide::proto::OptimizationType,
                 optimization_guide::OptimizationTypeDecision>
      optimization_type_decisions_;

  // The page hint for the navigation.
  base::Optional<std::unique_ptr<optimization_guide::proto::PageHint>>
      page_hint_;

  // The time that the hints fetch for this navigation started. Is only present
  // if a fetch was initiated for this navigation.
  base::Optional<base::TimeTicks> hints_fetch_start_;

  // The time that the hints fetch for the navigation ended. Is only present if
  // a fetch was initiated and successfully completed for this navigation.
  base::Optional<base::TimeTicks> hints_fetch_end_;

  // The status for whether a hint for the page load was attempted to be fetched
  // from the remote Optimization Guide Service.
  base::Optional<optimization_guide::RaceNavigationFetchAttemptStatus>
      hints_fetch_attempt_status_;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<OptimizationGuideNavigationData> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
