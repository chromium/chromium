// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "components/metrics/structured/buildflags/buildflags.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#endif

namespace glic {

namespace {

constexpr char kInvokeResultHistogramName[] = "Glic.InvokeResult";
constexpr char kInvokeSourceHistogramName[] = "Glic.Invoke.InvocationSource";
constexpr char kInvokeDurationHistogramName[] = "Glic.Invoke.Duration";

}  // namespace

glic::GlicCuiOutcome MapInvokeErrorToCuiOutcome(GlicInvokeError error) {
  switch (error) {
    case GlicInvokeError::kTimeout:
      return glic::GlicCuiOutcome::kFailedLatency;
    case GlicInvokeError::kCancelled:
      return glic::GlicCuiOutcome::kUnknownCancel;
    case GlicInvokeError::kTabClosed:
    case GlicInvokeError::kInstanceDestroyed:
      return glic::GlicCuiOutcome::kAbandoned;
    default:
      return glic::GlicCuiOutcome::kFailed;
  }
}

GlicInvokeMetrics::GlicInvokeMetrics(mojom::InvocationSource source)
    : source_(source),
      invoke_start_time_(base::TimeTicks::Now()),
      invocation_id_(base::RandUint64()) {
  base::UmaHistogramEnumeration(kInvokeSourceHistogramName, source_);
}

void GlicInvokeMetrics::RecordStarted(mojom::FeatureMode feature_mode,
                                      int embedder_type) const {
#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::glic::InvokeStarted()
          .SetInvocationId(invocation_id_)
          .SetInvocationSource(static_cast<int>(source_))
          .SetFeatureMode(static_cast<int>(feature_mode))
          .SetEmbedderType(embedder_type));
#endif
}

void GlicInvokeMetrics::RecordTaskPhaseCompleted(
    std::optional<GlicTaskType> task_type,
    base::TimeDelta duration) const {
#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::glic::InvokeTaskPhaseCompleted()
          .SetInvocationId(invocation_id_)
          .SetTaskType(
              task_type.has_value() ? static_cast<int>(task_type.value()) : 0)
          .SetDurationTimeDelta(duration.InMilliseconds()));
#endif
}

void GlicInvokeMetrics::RecordSuccess(
    std::optional<GlicTaskType> final_task_type) const {
  base::UmaHistogramEnumeration(kInvokeResultHistogramName,
                                GlicInvokeResult::kSuccess);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeResultHistogramName,
                         GetInvocationSourceString(source_)),
      GlicInvokeResult::kSuccess);

  base::TimeDelta duration = base::TimeTicks::Now() - invoke_start_time_;
  base::UmaHistogramLongTimes100(kInvokeDurationHistogramName, duration);
  base::UmaHistogramLongTimes100(
      base::StringPrintf("%s.%s", kInvokeDurationHistogramName,
                         GetInvocationSourceString(source_)),
      duration);

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::glic::InvokeTerminated()
          .SetInvocationId(invocation_id_)
          .SetOutcome(static_cast<
                      metrics::structured::events::v2::glic::GlicCuiOutcome>(
              static_cast<int>(glic::GlicCuiOutcome::kSuccess)))
          .SetStoppedTaskType(final_task_type.has_value()
                                  ? static_cast<int>(final_task_type.value())
                                  : 0)
          .SetTimeSinceStart(duration.InMilliseconds()));
#endif
}

void GlicInvokeMetrics::RecordError(
    GlicInvokeError result,
    std::optional<GlicTaskType> stopped_task) const {
  base::UmaHistogramEnumeration(kInvokeResultHistogramName, result);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeResultHistogramName,
                         GetInvocationSourceString(source_)),
      result);

  base::TimeDelta duration = base::TimeTicks::Now() - invoke_start_time_;
  base::UmaHistogramLongTimes100(kInvokeDurationHistogramName, duration);
  base::UmaHistogramLongTimes100(
      base::StringPrintf("%s.%s", kInvokeDurationHistogramName,
                         GetInvocationSourceString(source_)),
      duration);

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::glic::InvokeTerminated()
          .SetInvocationId(invocation_id_)
          .SetOutcome(static_cast<
                      metrics::structured::events::v2::glic::GlicCuiOutcome>(
              static_cast<int>(MapInvokeErrorToCuiOutcome(result))))
          .SetInvokeError(static_cast<int>(result))
          .SetStoppedTaskType(stopped_task.has_value()
                                  ? static_cast<int>(stopped_task.value())
                                  : 0)
          .SetTimeSinceStart(duration.InMilliseconds()));
#endif
}

}  // namespace glic
