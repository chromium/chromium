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
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebContents;
}

namespace glic {

class Host;
class GlicInstance;

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
  };

  explicit GlicInstanceCoordinatorMetrics(DataProvider* data_provider,
                                          PrefService* pref_service);
  ~GlicInstanceCoordinatorMetrics();

  // Starts periodic recording of memory metrics.
  void StartPeriodicMemoryMetricsRecording();

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

  // Called on memory pressure events to record memory footprint metrics.
  void OnMemoryPressure(base::MemoryPressureLevel level);

  // Called periodically to record memory footprint metrics using the averaging
  // and totals scheme.
  void RecordPeriodicMemoryMetrics();

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

  // Helper method to calculate and record memory footprint metrics with a given
  // suffix.
  void RecordMemoryFootprint(std::string_view suffix);

  void OnPinningPrefChanged();

  const raw_ptr<DataProvider> data_provider_;

  // Tracks the start time of a "Concurrent Visibility" period, which is a
  // period where 2 or more instances are visible simultaneously.
  std::optional<base::TimeTicks> concurrent_visibility_start_time_;
  // Tracks the maximum number of visible instances during the current
  // concurrent visibility period.
  int concurrent_visibility_peak_count_ = 0;

  // Tracks the ID of the currently active conversation.
  std::optional<std::string> active_conversation_id_;

  base::RepeatingTimer memory_metrics_timer_;

  PrefChangeRegistrar pref_registrar_;
  bool is_pinned_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_COORDINATOR_METRICS_H_
