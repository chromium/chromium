// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

class OptimizationGuideKeyedService;

namespace content {
class WebContents;
}  // namespace content

namespace history {
class HistoryService;
}  // namespace history

namespace safe_browsing {

class GeminiAntiscamProtectionService : public KeyedService {
 public:
  explicit GeminiAntiscamProtectionService(
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      history::HistoryService* history_service);
  ~GeminiAntiscamProtectionService() override;

  // Use the |request_type|, |did_match_high_confidence_allowlist|,
  // |should_show_scam_warning|, and |is_phishing| parameter values to determine
  // whether to trigger a history service check with the
  // |DidGetVisibleVisitCount| method as a callback.
  void MaybeStartAntiscamProtection(GURL url,
                                    ClientSideDetectionType request_type,
                                    bool did_match_high_confidence_allowlist,
                                    bool should_show_scam_warning,
                                    bool is_phishing,
                                    std::string page_inner_text);

 private:
  // Callback for querying the history service. Depending on the value of
  // |result|, maybe trigger a call to Gemini to determine scamminess using
  // |url| and |page_inner_text|.
  void DidGetVisibleVisitCount(GURL url,
                               std::string page_inner_text,
                               history::VisibleVisitCountToHostResult result);

  void OnModelResponse(
      base::TimeTicks start_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;

  raw_ptr<history::HistoryService> history_service_;

  // Task tracker used for querying URLs in the history service.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<GeminiAntiscamProtectionService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_H_
