// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"

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
                     weak_factory_.GetWeakPtr()),
      &task_tracker_);
}

void GeminiAntiscamProtectionService::DidGetVisibleVisitCount(
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
  // TODO(crbug.com/467358093): Run Gemini to determine scamminess.
}

}  // namespace safe_browsing
