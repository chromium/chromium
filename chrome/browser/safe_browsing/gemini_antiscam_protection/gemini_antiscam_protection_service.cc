// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/features/gemini_antiscam_protection.pb.h"

namespace {

optimization_guide::proto::GeminiAntiscamProtectionRequest
BuildGeminiAntiscamProtectionRequest(GURL url, std::string page_inner_text) {
  optimization_guide::proto::GeminiAntiscamProtectionRequest request;
  request.set_url(url.spec());
  request.set_page_content(page_inner_text);
  return request;
}

}  // namespace

namespace safe_browsing {

GeminiAntiscamProtectionService::GeminiAntiscamProtectionService(
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    history::HistoryService* history_service)
    : optimization_guide_keyed_service_(optimization_guide_keyed_service),
      history_service_(history_service) {}

GeminiAntiscamProtectionService::~GeminiAntiscamProtectionService() = default;

void GeminiAntiscamProtectionService::MaybeStartAntiscamProtection(
    GURL url,
    ClientSideDetectionType request_type,
    bool did_match_high_confidence_allowlist,
    bool should_show_scam_warning,
    bool is_phishing,
    std::string page_inner_text) {
  // If the page already matches the allowlist or is already determined to be
  // scam or phishing, we don't need to run Gemini to determine scamminess.
  if (did_match_high_confidence_allowlist || should_show_scam_warning ||
      is_phishing) {
    return;
  }
  // Only run Gemini for CSD checks triggered via force request.
  if (request_type != ClientSideDetectionType::FORCE_REQUEST) {
    return;
  }

  // Query history service to determine if the URL has been visited before.
  // If the URL has not been visited before, run Gemini.
  history_service_->GetVisibleVisitCountToHost(
      url,
      base::BindOnce(&GeminiAntiscamProtectionService::DidGetVisibleVisitCount,
                     weak_factory_.GetWeakPtr(), url, page_inner_text),
      &task_tracker_);
}

void GeminiAntiscamProtectionService::DidGetVisibleVisitCount(
    GURL url,
    std::string page_inner_text,
    history::VisibleVisitCountToHostResult result) {
  if (!result.success) {
    // If the history service was not able to determine the number of visits,
    // we should not run Gemini.
    return;
  }
  if (result.count > 1) {
    // If the URL has been visited before, we don't need to run Gemini.
    return;
  }

  // Query server-side model.
  optimization_guide::proto::GeminiAntiscamProtectionRequest request =
      BuildGeminiAntiscamProtectionRequest(url, page_inner_text);
  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kGeminiAntiscamProtection,
      request, {},
      base::BindOnce(&GeminiAntiscamProtectionService::OnModelResponse,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void GeminiAntiscamProtectionService::OnModelResponse(
    base::TimeTicks start_time,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  if (!result.response.has_value()) {
    base::UmaHistogramTimes(
        "SafeBrowsing.GeminiAntiscamProtection.FailedEmptyResponse.Latency",
        latency);
    return;
  }
  std::optional<optimization_guide::proto::GeminiAntiscamProtectionResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::GeminiAntiscamProtectionResponse>(
          result.response.value());
  if (!response) {
    base::UmaHistogramTimes(
        "SafeBrowsing.GeminiAntiscamProtection.FailedParsingError.Latency",
        latency);
    return;
  }
  base::UmaHistogramTimes(
      "SafeBrowsing.GeminiAntiscamProtection.Success.Latency", latency);
  // TODO(crbug.com/467358093): Process the model response.
}

}  // namespace safe_browsing
