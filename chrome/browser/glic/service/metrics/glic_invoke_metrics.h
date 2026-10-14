// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/time/time.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

class GlicCuiTracker;

// This enum works around needing to add a kSuccess to GlicInvokeError solely
// for metrics, which would add confusion. The `0` entry of GlicInvokeError is
// unused so that these enums can be "overlaid".
enum class GlicInvokeResult {
  kSuccess = 0,
  kErrorMaxValue = static_cast<int>(GlicInvokeError::kMaxValue),
  kMaxValue = kErrorMaxValue
};

enum class GlicTaskType;
#include "components/metrics/structured/buildflags/buildflags.h"

// Which GlicInstanceCoordinatorImpl entry point produced an invocation. The
// toggle path (a window entry point opening the panel) is the population of
// the Glic.CUI.WindowEntryPointInvocation.* histograms, so recording this on
// the structured events lets them be compared. Mirrored by
// GlicInvokeEntryPath in //tools/metrics/structured/sync/structured.xml;
// glic_invoke_metrics.cc translates between the two with an exhaustive
// switch, so a value cannot be added here without adding it there.
// LINT.IfChange(GlicInvokeEntryPath)
enum class GlicInvokeEntryPath {
  kUnknown = 0,
  // GlicKeyedService::ShowUI() / ToggleUI(): toolbar and tab strip buttons,
  // OS hotkey and icon, 3-dot menu, IPH, profile picker, ...
  kToggle = 1,
  // GlicKeyedService::Invoke().
  kInvoke = 2,
  // GlicKeyedService::InvokeWithAutoSubmit().
  kInvokeWithAutoSubmit = 3,
};
// LINT.ThenChange(//tools/metrics/structured/sync/structured.xml:GlicInvokeEntryPath)

class GlicInvokeMetrics {
 public:
  explicit GlicInvokeMetrics(mojom::InvocationSource source);
  ~GlicInvokeMetrics();

  GlicInvokeMetrics(const GlicInvokeMetrics&) = delete;
  GlicInvokeMetrics& operator=(const GlicInvokeMetrics&) = delete;

  uint64_t GetInvocationId() const { return invocation_id_; }

  // Set as soon as they are known. They are attached to InvokeStarted and
  // InvokeTerminated, including when the invocation is rejected before
  // RecordStarted() is called.
  void SetFeatureMode(mojom::FeatureMode feature_mode) {
    feature_mode_ = feature_mode;
  }
  void SetEmbedderType(EmbedderType embedder_type) {
    embedder_type_ = embedder_type;
  }
  void SetEntryPath(GlicInvokeEntryPath entry_path) {
    entry_path_ = entry_path;
  }
  // The targeted host's load state when the invocation starts. Attached to
  // InvokeTerminated so that an invocation that fails on a failure left over
  // from an earlier show can be told apart from one that watched it happen.
  void SetClientLoadStateAtStart(ClientLoadState state) {
    client_load_state_at_start_ = state;
  }

  // A CUI tracker to resolve with the invocation's outcome: kSuccess on
  // RecordSuccess() (the tracker itself turns that into kFailedLatency when
  // it took too long) and MapInvokeErrorToCuiOutcome() of the error on
  // RecordError(). This makes the CUI histogram and the InvokeTerminated
  // Outcome agree by construction. An unresolved tracker records
  // kUnknownCancel when `this` is destroyed, which cannot happen for an
  // invocation that terminated.
  void SetCuiTracker(std::unique_ptr<GlicCuiTracker> cui_tracker);

  // Called when the invocation orchestrator starts execution.
  void RecordStarted() const;

  // Called when a sequentially blocking task phase completes.
  void RecordTaskPhaseCompleted(std::optional<GlicTaskType> task_type,
                                base::TimeDelta duration) const;

  void RecordSuccess(
      std::optional<GlicTaskType> final_task_type = std::nullopt) const;
  void RecordError(
      GlicInvokeError result,
      std::optional<GlicTaskType> stopped_task = std::nullopt) const;

  // Records a kInvokeInProgress error. `in_progress_invocation_id` is the
  // invocation ID of the already-running invocation that blocked this one.
  void RecordInvokeInProgressError(uint64_t in_progress_invocation_id) const;

 private:
  void RecordErrorInternal(
      GlicInvokeError result,
      std::optional<GlicTaskType> stopped_task,
      std::optional<uint64_t> in_progress_invocation_id) const;
  // Records the InvokeTerminated structured event. `error` is nullopt on
  // success.
  void RecordTerminated(
      std::optional<GlicInvokeError> error,
      std::optional<GlicTaskType> task_type,
      base::TimeDelta duration,
      std::optional<uint64_t> in_progress_invocation_id) const;
  void RecordTimeoutStage(std::optional<GlicTaskType> stage) const;

  mojom::InvocationSource source_;
  base::TimeTicks invoke_start_time_;
  uint64_t invocation_id_;
  std::optional<mojom::FeatureMode> feature_mode_;
  std::optional<EmbedderType> embedder_type_;
  std::optional<ClientLoadState> client_load_state_at_start_;
  GlicInvokeEntryPath entry_path_ = GlicInvokeEntryPath::kUnknown;
  std::unique_ptr<GlicCuiTracker> cui_tracker_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_GLIC_INVOKE_METRICS_H_
