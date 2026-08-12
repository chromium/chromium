// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace glic {

GlicInstanceHelperMetrics::GlicInstanceHelperMetrics() = default;

GlicInstanceHelperMetrics::~GlicInstanceHelperMetrics() {
  if (!bound_instances_.empty()) {
    base::UmaHistogramCounts100("Glic.Tab.InstanceBindCount",
                                bound_instances_.size());
  }
  if (!pinned_by_instances_.empty()) {
    base::UmaHistogramCounts100("Glic.Tab.InstancePinCount",
                                pinned_by_instances_.size());
  }
  if (is_daisy_chained_) {
    AutoOpenCloseReason close_reason =
        (current_metric_action_ == DaisyChainFirstAction::kTabSwitched)
            ? AutoOpenCloseReason::kTabSwitched
            : AutoOpenCloseReason::kExplicitlyClosed;
    FlushAutoOpenMetrics(close_reason, base::TimeTicks::Now());
  }
}

void GlicInstanceHelperMetrics::OnBoundToInstance(
    const InstanceId& instance_id) {
  bound_instances_.insert(instance_id);
}

void GlicInstanceHelperMetrics::OnPinnedByInstance(
    const InstanceId& instance_id) {
  pinned_by_instances_.insert(instance_id);
}

void GlicInstanceHelperMetrics::SetIsDaisyChained(DaisyChainSource source,
                                                  ukm::SourceId source_id) {
  if (is_daisy_chained_) {
    return;
  }
  is_daisy_chained_ = true;
  daisy_chain_source_ = source;
  metric_finalized_ = false;
  current_metric_action_ = DaisyChainFirstAction::kNoAction;
  start_time_ = base::TimeTicks::Now();
  first_action_time_ = base::TimeTicks();
  prompt_count_ = 0;
  ukm_recorded_ = false;
  source_id_ = source_id;
  flush_timer_.Stop();
}

void GlicInstanceHelperMetrics::OnDaisyChainAction(
    DaisyChainFirstAction action) {
  if (!is_daisy_chained_ || action == DaisyChainFirstAction::kNoAction) {
    return;
  }

  if (action == DaisyChainFirstAction::kInputSubmitted) {
    prompt_count_++;
    flush_timer_.Stop();
  }

  if (first_action_time_.is_null()) {
    first_action_time_ = base::TimeTicks::Now();
  }

  // Explicit panel closure or tab switch starts/restarts the 5s debounce timer
  // for both UKM and UMA.
  if (action == DaisyChainFirstAction::kSidePanelClosed ||
      action == DaisyChainFirstAction::kTabSwitched) {
    AutoOpenCloseReason close_reason =
        (action == DaisyChainFirstAction::kTabSwitched)
            ? AutoOpenCloseReason::kTabSwitched
            : AutoOpenCloseReason::kExplicitlyClosed;
    flush_timer_.Start(
        FROM_HERE, base::Seconds(5),
        base::BindOnce(&GlicInstanceHelperMetrics::FlushAutoOpenMetrics,
                       base::Unretained(this), close_reason,
                       base::TimeTicks::Now()));
  }

  if (metric_finalized_) {
    return;
  }

  // Set the current action. Interim ambiguous actions (e.g. kTabSwitched or
  // kSidePanelClosed) can be upgraded to a terminal engagement action (e.g.
  // kInputSubmitted) if triggered within the 5s debounce window. Once a
  // terminal action executes, `metric_finalized_` is set to true to prevent
  // subsequent dismissals from overwriting it.
  current_metric_action_ = action;

  if (action != DaisyChainFirstAction::kSidePanelClosed &&
      action != DaisyChainFirstAction::kTabSwitched) {
    // Terminal action (e.g. user input). Flush UMA immediately.
    FlushFirstActionMetric();
  }
}

void GlicInstanceHelperMetrics::FlushAutoOpenMetrics(
    AutoOpenCloseReason close_reason, base::TimeTicks close_time) {
  FlushFirstActionMetric();
  RecordUkm(close_reason, close_time);
}

void GlicInstanceHelperMetrics::FlushFirstActionMetric() {
  if (metric_finalized_) {
    return;
  }
  std::string source_str = GetDaisyChainSourceString(daisy_chain_source_);

  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Instance.AutoOpenedPanel.FirstAction.", source_str}),
      current_metric_action_);
  metric_finalized_ = true;
  flush_timer_.Stop();
}

void GlicInstanceHelperMetrics::RecordUkm(AutoOpenCloseReason close_reason,
                                          base::TimeTicks end_time) {
  if (!is_daisy_chained_ || ukm_recorded_ ||
      daisy_chain_source_ != DaisyChainSource::kAutoOpenPdf) {
    return;
  }

  if (source_id_ == ukm::kInvalidSourceId) {
    return;
  }

  base::TimeDelta auto_open_duration;
  if (!start_time_.is_null() && !end_time.is_null()) {
    auto_open_duration = end_time - start_time_;
  }
  int64_t bucketed_duration_ms = ukm::GetExponentialBucketMinForUserTiming(
      auto_open_duration.InMilliseconds());

  DaisyChainFirstAction first_action = current_metric_action_;
  if (first_action == DaisyChainFirstAction::kNoAction) {
    if (close_reason == AutoOpenCloseReason::kTabSwitched) {
      first_action = DaisyChainFirstAction::kTabSwitched;
    } else {
      first_action = DaisyChainFirstAction::kSidePanelClosed;
    }
  }

  ukm::builders::Glic_AutoOpen_Closed builder(source_id_);
  builder.SetSessionDurationMs(bucketed_duration_ms)
      .SetCloseReason(static_cast<int64_t>(close_reason))
      .SetFirstAction(static_cast<int64_t>(first_action))
      .SetPromptCount(ukm::GetExponentialBucketMinForCounts1000(prompt_count_));

  if (!first_action_time_.is_null() && !start_time_.is_null()) {
    base::TimeDelta first_action_duration = first_action_time_ - start_time_;
    builder.SetTimeToFirstActionMs(ukm::GetExponentialBucketMinForUserTiming(
        first_action_duration.InMilliseconds()));
  } else {
    builder.SetTimeToFirstActionMs(bucketed_duration_ms);
  }

  builder.Record(ukm::UkmRecorder::Get());
  ukm_recorded_ = true;
}

}  // namespace glic
