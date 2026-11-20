// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_RECORD_REPLAY_MES_CLIENT_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_RECORD_REPLAY_MES_CLIENT_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

namespace optimization_guide {
class ModelQualityLogEntry;
class RemoteModelExecutor;
struct OptimizationGuideModelExecutionResult;
namespace proto {
class ContextHubRequest;
}  // namespace proto
}  // namespace optimization_guide

namespace context_hub {

struct RecordingArtifacts;

// Client for executing Model Execution Service (MES) for Record and Replay.
// Instances are not thread-safe and must be created, called, and destroyed on
// the same sequence.
class RecordReplayMesClient {
 public:
  // Maximum number of action screenshots attached to a single MES request.
  // Actions beyond this limit still have their text metadata included in the
  // prompt without attaching image bytes.
  static constexpr size_t kMaxAttachedScreenshots = 100;

  // `model_executor` must be non-null and must outlive `this`.
  explicit RecordReplayMesClient(
      optimization_guide::RemoteModelExecutor* model_executor);
  RecordReplayMesClient(const RecordReplayMesClient&) = delete;
  RecordReplayMesClient& operator=(const RecordReplayMesClient&) = delete;
  ~RecordReplayMesClient();

  using GenerateSkillCallback =
      base::OnceCallback<void(std::optional<std::string> skill_instructions)>;

  // Generates skill instructions from the recorded session `artifacts` via MES.
  // Asynchronously resolves `callback` with `std::nullopt` if `artifacts` is
  // empty, if another skill generation request is already in flight, or if
  // model execution fails. Note that `callback` is resolved synchronously with
  // `std::nullopt` if an in-flight request is cancelled via
  // `CancelPendingRequest()` or `~RecordReplayMesClient()`.
  void GenerateSkillFromRecording(const RecordingArtifacts& artifacts,
                                  GenerateSkillCallback callback);

  // Cancels any in-flight MES request and synchronously resolves its pending
  // callback with `std::nullopt`. Also invoked by `~RecordReplayMesClient()`.
  void CancelPendingRequest();

 private:
  void OnMesRequestBuilt(optimization_guide::proto::ContextHubRequest request);

  void OnModelExecutionComplete(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  const raw_ref<optimization_guide::RemoteModelExecutor> model_executor_;
  GenerateSkillCallback pending_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RecordReplayMesClient> weak_ptr_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_RECORD_REPLAY_MES_CLIENT_H_
