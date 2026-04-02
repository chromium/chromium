// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"

class BrowserWindowInterface;
class OptimizationGuideKeyedService;
class TabListInterface;

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace contextual_cueing {

enum class ContextualCueingDecision {
  kUnspecified = 0,
  // Tab was not active when the page was classified.
  kNoLongerActiveTabAfterCategoryClassification = 1,
  // Tab was active but the page was not classified as a vertical we support.
  kFailedCategoryClassification = 2,
  // Model execution service is unavailable.
  kModelExecutionUnavailable = 3,
  // Model execution failed.
  kModelExecutionFailed = 4,
  // Model execution response failed to parse.
  kModelExecutionResponseFailedToParse = 5,
  // Contextual cue was shown to the user.
  kSuccess = 6,

  kMaxValue = kSuccess,
};

class ContextualCueingController
    : public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  explicit ContextualCueingController(
      BrowserWindowInterface* browser_window_interface,
      TabListInterface* tab_list_interface);
  ContextualCueingController(const ContextualCueingController&) = delete;
  ContextualCueingController& operator=(const ContextualCueingController&) =
      delete;
  ~ContextualCueingController() override;

  // page_content_annotations::PageContentAnnotationsService::
  // PageContentAnnotationsServiceObserver:
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

 private:
  // Initiates a model execution request to MES for the current window state.
  void InitiateModelExecutionRequest();

  // Callback for when the model execution response is received.
  void OnModelExecutionResponseReceived(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Not owned. Guaranteed to outlive `this`.
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<TabListInterface> tab_list_interface_;
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  base::WeakPtrFactory<ContextualCueingController> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
