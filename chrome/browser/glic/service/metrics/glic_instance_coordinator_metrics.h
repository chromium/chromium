// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace glic {

class Host;
class GlicInstance;
class GlicInstanceImpl;
class GlicMetricReporter;

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

class GlicInstanceCoordinatorMetrics {
 public:
  // Interface for components that need read-only access to the list of
  // GlicInstances.
  class DataProvider {
   public:
    struct InstanceWebContents {
      raw_ptr<content::WebContents> webui_contents;
      raw_ptr<content::WebContents> web_client_contents;
    };

    virtual ~DataProvider() = default;

    virtual std::vector<InstanceWebContents>
    GetAllUnhibernatedWebContents() = 0;
    virtual int GetVisibleInstanceCount() const = 0;
    virtual std::vector<glic::mojom::ConversationInfoPtr>
    GetRecentlyActiveConversations(size_t limit) = 0;
    virtual std::vector<GlicInstanceImpl*> GetInstances() = 0;
    virtual Profile* GetProfile() const = 0;
  };

  explicit GlicInstanceCoordinatorMetrics(DataProvider* data_provider);
  ~GlicInstanceCoordinatorMetrics();

  // Starts periodic recording of instance count and memory metrics.
  void StartPeriodicMetricsRecording();

  // Called by the coordinator whenever an instance's visibility changes.
  void OnInstanceVisibilityChanged();

  // Called by the coordinator when switching conversations to record the target
  // type.
  void RecordSwitchConversationTarget(
      const std::optional<std::string>& requested_conversation_id,
      const std::optional<std::string>& target_instance_conversation_id,
      raw_ptr<GlicInstance> active_instance);

  // Called when activating a tab with a conversation to record candidate count.
  void RecordActivateTabCandidateTabCount(size_t count);

  // Called on memory release events to record memory footprint metrics.
  void OnReleaseMemory(base::MemoryLimit memory_limit);

  // Called periodically to record memory footprint metrics using the averaging
  // and totals scheme.
  void RecordPeriodicMemoryMetrics();

  // Records the total number of Glic instances alive when a new instance is
  // created.
  void RecordCountOnCreation();

  // Records the number of awake Glic instances when WebUI contents are created.
  void RecordCountAwakeOnContentsCreated();

  // Records the number of actuating Glic instances when an actor task is
  // created.
  void RecordCountActuatingOnTaskCreation();

  // Called on coordinator shutdown to finalize and record shutdown metrics.
  void OnShutdown();

 private:
  GlicMetricReporter GetMetricReporter() const;
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

  // Helper method to calculate and record memory footprint metrics with a given
  // suffix.
  void RecordMemoryFootprint(std::string_view suffix);

  // Called periodically to record the total number of Glic instances.
  void RecordPeriodicInstanceCount();

  const raw_ptr<DataProvider> data_provider_;

  // Tracks the start time of a "Concurrent Visibility" period, which is a
  // period where 2 or more instances are visible simultaneously.
  std::optional<base::TimeTicks> concurrent_visibility_start_time_;
  // Tracks the maximum number of visible instances during the current
  // concurrent visibility period.
  int concurrent_visibility_peak_count_ = 0;

  // Tracks the ID of the currently active conversation.
  std::optional<std::string> active_conversation_id_;

  // Tracks the peak total number of Glic instances (awake or hibernated) that
  // existed concurrently.
  size_t peak_instance_count_ = 0;

  // Monotonic latch set once OnShutdown() runs; suppresses post-shutdown
  // instance count updates during teardown.
  bool is_shutdown_ = false;

  base::RepeatingTimer memory_metrics_timer_;
  base::RepeatingTimer instance_count_metrics_timer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_
