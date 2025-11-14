// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_METRICS_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace glic {

class GlicInstance;

class GlicInstanceCoordinatorMetrics {
 public:
  // Interface for components that need read-only access to the list of
  // GlicInstances.
  class DataProvider {
   public:
    virtual ~DataProvider() = default;

    // Returns all currently managed instances, including any warmed instance.
    virtual std::vector<GlicInstance*> GetInstances() = 0;
  };

  explicit GlicInstanceCoordinatorMetrics(DataProvider* data_provider);
  ~GlicInstanceCoordinatorMetrics();

  // Called by the coordinator whenever an instance's visibility changes.
  void OnInstanceVisibilityChanged();

 private:
  // Helper to calculate currently visible instances using
  // data_provider_->GetInstances()
  int GetVisibleInstanceCount() const;

  // Ends the current concurrent visibility period if one is active, logging
  // metrics.
  void EndConcurrentVisibility();

  const raw_ptr<DataProvider> data_provider_;

  // Tracks the start time of a "Concurrent Visibility" period, which is a
  // period where 2 or more instances are visible simultaneously.
  std::optional<base::TimeTicks> concurrent_visibility_start_time_;
  // Tracks the maximum number of visible instances during the current
  // concurrent visibility period.
  int concurrent_visibility_peak_count_ = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_METRICS_H_
