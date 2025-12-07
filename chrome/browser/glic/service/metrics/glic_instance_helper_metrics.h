// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_HELPER_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_HELPER_METRICS_H_

#include "base/containers/flat_set.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/public/glic_instance.h"

namespace glic {

// LINT.IfChange(DaisyChainFirstAction)
enum class DaisyChainFirstAction {
  kNoAction = 0,
  kInputSubmitted = 1,
  kSwitchedConversation = 2,
  kRecursiveDaisyChain = 3,
  kSidePanelClosed = 4,
  kMaxValue = kSidePanelClosed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicDaisyChainFirstAction)

// Helper class for tracking metrics scoped to a specific tab's
// GlicInstanceHelper. GlicInstanceHelper attaches a Glic Instance ID to a Tab
// and allows the GlicInstanceCoordinator to track tab destruction. This class
// manages stateful metrics that persist across Glic instance bindings for a
// single tab, such as tracking unique instances that have bound to or pinned
// the tab. It also manages the state machine for "Daisy Chain" metrics, where
// we track the first significant user action in a Glic panel that was opened
// from another Glic panel.
class GlicInstanceHelperMetrics {
 public:
  GlicInstanceHelperMetrics();
  ~GlicInstanceHelperMetrics();

  void OnBoundToInstance(const InstanceId& instance_id);
  void OnPinnedByInstance(const InstanceId& instance_id);

  // Marks the tab as being part of a daisy chain session.
  void SetIsDaisyChained();

  // Records a significant user action during a daisy chain session.
  // Only the *first* action is recorded as the session outcome.
  void OnDaisyChainAction(DaisyChainFirstAction action);

  void FlushMetric();

 private:
  base::flat_set<InstanceId> bound_instances_;
  base::flat_set<InstanceId> pinned_by_instances_;

  bool is_daisy_chained_ = false;
  bool metric_finalized_ = false;
  DaisyChainFirstAction current_metric_action_ =
      DaisyChainFirstAction::kNoAction;
  base::OneShotTimer flush_timer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INSTANCE_HELPER_METRICS_H_
