// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace glic {

// LINT.IfChange(GlicSwitchConversationTarget)
enum class GlicSwitchConversationTarget {
  kUnknown = 0,
  kSwitchedToLastActive = 1,
  kSwitchedToExistingInstance = 2,
  kSwitchedToNewInstance = 3,
  kStartNewConversation = 4,
  kMaxValue = kStartNewConversation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicSwitchConversationTarget)

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

  // Called by the coordinator when switching conversations to record the target
  // type.
  void RecordSwitchConversationTarget(
      const std::optional<std::string>& requested_conversation_id,
      const std::optional<std::string>& target_instance_conversation_id,
      raw_ptr<GlicInstance> active_instance);

  void OnMemoryPressure(base::MemoryPressureLevel level);

  void OnHighMemoryUsage(int memory_mb);

 private:
  // Helper to calculate currently visible instances using
  // data_provider_->GetInstances()
  int GetVisibleInstanceCount() const;

  // Ends the current concurrent visibility period if one is active, logging
  // metrics.
  void EndConcurrentVisibility();

  // Gets the previous active conversation id that is not in the
  // excluded_instance.
  std::optional<std::string> GetMostRecentlyActiveConversationId(
      raw_ptr<GlicInstance> excluded_instance);

  const raw_ptr<DataProvider> data_provider_;

  // Tracks the start time of a "Concurrent Visibility" period, which is a
  // period where 2 or more instances are visible simultaneously.
  std::optional<base::TimeTicks> concurrent_visibility_start_time_;
  // Tracks the maximum number of visible instances during the current
  // concurrent visibility period.
  int concurrent_visibility_peak_count_ = 0;

  // Tracks the ID of the currently active conversation.
  std::optional<std::string> active_conversation_id_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_
