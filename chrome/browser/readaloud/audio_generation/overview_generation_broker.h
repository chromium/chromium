// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_AUDIO_GENERATION_OVERVIEW_GENERATION_BROKER_H_
#define CHROME_BROWSER_READALOUD_AUDIO_GENERATION_OVERVIEW_GENERATION_BROKER_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/read_aloud_generate_text.pb.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "url/gurl.h"

class OptimizationGuideKeyedService;

namespace readaloud {

// Manages AI Overview script generation requests for Read Aloud, constructing
// ReadAloudGenerateTextRequest protobufs and executing MES
// kReadAloudGenerateText.
//
// Lifetime & Ownership:
// Owned 1:1 by ReadAloudService on the UI thread. Created during
// ReadAloudService initialization and destroyed with the service.
class OverviewGenerationBroker {
 public:
  // Callback invoked when overview generation completes.
  // Receives raw response payload bytes (mojo_base::BigBuffer) and success
  // status.
  using GenerateOverviewCallback =
      base::OnceCallback<void(mojo_base::BigBuffer response_bytes,
                              bool success)>;

  // Abort reasons when overview generation is aborted prior to model execution.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(OverviewModelExecutionAbortReason)
  enum class AbortReason {
    kNullOptimizationGuideService = 0,
    kEmptyContent = 1,
    kMaxValue = kEmptyContent,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/readaloud/enums.xml:ReadAloudOverviewModelExecutionAbortReason)

  // Server result of overview generation requests to the model execution
  // service. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  // LINT.IfChange(OverviewModelExecutionServerResult)
  enum class ServerResult {
    kSuccess = 0,
    kModelExecutionError = 1,
    kEmptyResponse = 2,
    kMaxValue = kEmptyResponse,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/readaloud/enums.xml:ReadAloudOverviewModelExecutionServerResult)

  OverviewGenerationBroker();
  OverviewGenerationBroker(const OverviewGenerationBroker&) = delete;
  OverviewGenerationBroker& operator=(const OverviewGenerationBroker&) = delete;
  ~OverviewGenerationBroker();

  // Constructs a ReadAloudGenerateTextRequest protobuf from the given page
  // metadata and distilled content. Sanitizes `page_url` by restricting to
  // HTTP/HTTPS and stripping credentials, query, and ref parameters per
  // privacy requirements.
  optimization_guide::proto::ReadAloudGenerateTextRequest
  BuildGenerateTextRequest(std::string_view page_title,
                           std::string_view page_content,
                           const GURL& page_url,
                           std::string_view language_code) const;

  // Issues an asynchronous overview generation request via
  // OptimizationGuideKeyedService
  // (ModelBasedCapabilityKey::kReadAloudGenerateText). Enforces Rule of Two
  // security boundary by returning raw payload bytes in BigBuffer without
  // deserializing ReadAloudGenerateTextResponse in the Browser process.
  // Automatically cancels any pending in-flight generation request.
  void GenerateOverview(OptimizationGuideKeyedService* opt_guide_service,
                        std::string_view page_title,
                        std::string_view page_content,
                        const GURL& page_url,
                        std::string_view language_code,
                        GenerateOverviewCallback callback);

  // Cancels any pending overview generation callbacks.
  void InvalidatePendingRequests();

 private:
  void OnModelExecutionResult(
      GenerateOverviewCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OverviewGenerationBroker> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_AUDIO_GENERATION_OVERVIEW_GENERATION_BROKER_H_
