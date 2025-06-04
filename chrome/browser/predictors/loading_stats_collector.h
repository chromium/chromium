// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_STATS_COLLECTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_STATS_COLLECTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace predictors {

struct OptimizationGuidePrediction;
class ResourcePrefetchPredictor;
struct PageRequestSummary;

// Accumulates data from different speculative actions and collates this data
// with a summary of actual page load into statistical reports.
class LoadingStatsCollector {
 public:
  explicit LoadingStatsCollector(ResourcePrefetchPredictor* predictor);

  LoadingStatsCollector(const LoadingStatsCollector&) = delete;
  LoadingStatsCollector& operator=(const LoadingStatsCollector&) = delete;

  ~LoadingStatsCollector();

  // Records a summary of a page load. The summary is collated with speculative
  // actions taken for a given page load if any. The summary is compared with a
  // prediction by ResourcePrefetchPredictor and the Optimization Guide, if
  // |optimization_guide_prediction| is present.
  // All results are reported to UKM.
  void RecordPageRequestSummary(
      const PageRequestSummary& summary,
      const std::optional<OptimizationGuidePrediction>&
          optimization_guide_prediction);

 private:
  raw_ptr<ResourcePrefetchPredictor, DanglingUntriaged> predictor_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_STATS_COLLECTOR_H_
