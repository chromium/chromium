// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/service/glic_invoke_task.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "components/metrics/structured/buildflags/buildflags.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#endif

namespace glic {

namespace {

constexpr char kInvokeResultHistogramName[] = "Glic.InvokeResult2";
constexpr char kInvokeSourceHistogramName[] = "Glic.Invoke.InvocationSource";
constexpr char kInvokeDurationHistogramName[] = "Glic.Invoke.Duration";
constexpr char kInvokeTimeoutStageHistogramName[] = "Glic.Invoke.TimeoutStage";

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
// Translates the browser-side enum into the structured metrics enum generated
// from structured.xml. Written as an exhaustive switch with no default so that
// adding an EmbedderType without adding the matching structured.xml variant is
// a compile error rather than a silently mislabelled metric.
metrics::structured::events::v2::glic::GlicEmbedderType
ToStructuredEmbedderType(EmbedderType embedder_type) {
  using StructuredEmbedderType =
      metrics::structured::events::v2::glic::GlicEmbedderType;
  switch (embedder_type) {
    case EmbedderType::kSidePanel:
      return StructuredEmbedderType::SIDE_PANEL;
    case EmbedderType::kFloaty:
      return StructuredEmbedderType::FLOATY;
    case EmbedderType::kTab:
      return StructuredEmbedderType::TAB;
    case EmbedderType::kUnknown:
      return StructuredEmbedderType::UNKNOWN;
  }
}
#endif

}  // namespace

glic::GlicCuiOutcome MapInvokeErrorToCuiOutcome(GlicInvokeError error) {
  switch (error) {
    case GlicInvokeError::kTimeout:
      return glic::GlicCuiOutcome::kFailedLatency;
    case GlicInvokeError::kCancelled:
      return glic::GlicCuiOutcome::kUnknownCancel;
    case GlicInvokeError::kTabClosed:
    case GlicInvokeError::kInstanceDestroyedBlankInstanceClosed:
    case GlicInvokeError::kInstanceDestroyedUnbound:
    case GlicInvokeError::kInstanceDestroyedArchived:
    case GlicInvokeError::kInstanceDestroyedSignedOut:
    case GlicInvokeError::kInstanceDestroyedShutdown:
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

void GlicInvokeMetrics::RecordStarted() const {
#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  metrics::structured::events::v2::glic::InvokeStarted event;
  event.SetInvocationId(invocation_id_)
      .SetInvocationSource(static_cast<int>(source_));
  if (feature_mode_.has_value()) {
    event.SetFeatureMode(static_cast<int>(*feature_mode_));
  }
  if (embedder_type_.has_value()) {
    event.SetEmbedderType(ToStructuredEmbedderType(*embedder_type_));
  }
  metrics::structured::StructuredMetricsClient::Record(std::move(event));
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

  RecordTerminated(/*error=*/std::nullopt, final_task_type, duration,
                   /*in_progress_invocation_id=*/std::nullopt);
}

void GlicInvokeMetrics::RecordError(
    GlicInvokeError result,
    std::optional<GlicTaskType> stopped_task) const {
  RecordErrorInternal(result, stopped_task,
                      /*in_progress_invocation_id=*/std::nullopt);
}

void GlicInvokeMetrics::RecordInvokeInProgressError(
    uint64_t in_progress_invocation_id) const {
  RecordErrorInternal(GlicInvokeError::kInvokeInProgress,
                      /*stopped_task=*/std::nullopt, in_progress_invocation_id);
}

void GlicInvokeMetrics::RecordErrorInternal(
    GlicInvokeError result,
    std::optional<GlicTaskType> stopped_task,
    std::optional<uint64_t> in_progress_invocation_id) const {
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
  if (result == GlicInvokeError::kTimeout) {
    RecordTimeoutStage(stopped_task);
  }

  RecordTerminated(result, stopped_task, duration, in_progress_invocation_id);
}

void GlicInvokeMetrics::RecordTerminated(
    std::optional<GlicInvokeError> error,
    std::optional<GlicTaskType> task_type,
    base::TimeDelta duration,
    std::optional<uint64_t> in_progress_invocation_id) const {
#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
  const glic::GlicCuiOutcome outcome = error
                                           ? MapInvokeErrorToCuiOutcome(*error)
                                           : glic::GlicCuiOutcome::kSuccess;
  metrics::structured::events::v2::glic::InvokeTerminated event;
  event.SetInvocationId(invocation_id_)
      .SetInvocationSource(static_cast<int>(source_))
      .SetOutcome(
          static_cast<metrics::structured::events::v2::glic::GlicCuiOutcome>(
              static_cast<int>(outcome)))
      .SetStoppedTaskType(
          task_type.has_value() ? static_cast<int>(task_type.value()) : 0)
      .SetTimeSinceStart(duration.InMilliseconds());
  if (error.has_value()) {
    event.SetInvokeError(static_cast<int>(*error));
  }
  if (feature_mode_.has_value()) {
    event.SetFeatureMode(static_cast<int>(*feature_mode_));
  }
  if (embedder_type_.has_value()) {
    event.SetEmbedderType(ToStructuredEmbedderType(*embedder_type_));
  }
  if (in_progress_invocation_id.has_value()) {
    event.SetInProgressInvocationId(*in_progress_invocation_id);
  }
  metrics::structured::StructuredMetricsClient::Record(std::move(event));
#endif
}

void GlicInvokeMetrics::RecordTimeoutStage(
    std::optional<GlicTaskType> stage) const {
  GlicTaskType stage_to_record = stage.value_or(GlicTaskType::kUnknown);
  base::UmaHistogramEnumeration(kInvokeTimeoutStageHistogramName,
                                stage_to_record);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.%s", kInvokeTimeoutStageHistogramName,
                         GetInvocationSourceString(source_)),
      stage_to_record);
}

}  // namespace glic
