// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class BrowserWindowInterface;
class OptimizationGuideKeyedService;
class TabListInterface;

namespace actions {
class ActionItem;
class ActionInvocationContext;
}  // namespace actions

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace contextual_cueing {

class ContextualCueingService;

// LINT.IfChange(ContextualCueingDecision)
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
  // The response didn't have both anchored_message_text and action_text.
  kMissingAnchoredMessageText = 7,
  // The response didn't match a known target feature.
  kUnknownFulfillmentSurface = 8,
  // The response was for a target feature that didn't register itself.
  kTargetFeatureNotRegistered = 9,
  // The feature reported that its cue shouldn't be shown.
  kTargetFeatureNotEligible = 10,
  // The cue couldn't be shown because the window had no active tab.
  kNoActiveTab = 11,
  // The cue couldn't be shown because the page actions framework wasn't
  // available.
  kNoPageActions = 12,
  // The cue couldn't be shown because the tab for the cue was no longer active.
  kNoLongerActiveTabAfterModelExecution = 13,
  // The cue couldn't be shown because there was a feature promo active.
  kFeaturePromoActive = 14,
  kMaxValue = kFeaturePromoActive,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/enums.xml:ContextualCueingDecision)

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

  // Register a cue type. Feature code provides a CueTarget for reporting the
  // feature's cue eligibility and handling clicks. Calling this function for a
  // CueTargetType that was already registered will destroy the previous target.
  // Once registered, cue types are never unregistered -- features may prevent
  // cues by returning false from IsEligible.
  void RegisterCueTarget(CueTargetType type, std::unique_ptr<CueTarget> target);

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
      optimization_guide::proto::Tab active_tab,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  void ShowCue(CueTargetType cue_type,
               const CueTarget& target,
               optimization_guide::proto::ContextualCueingResponse response);
  void OnCueClicked(CueTargetType cue_type,
                    CueActionData data,
                    actions::ActionItem*,
                    actions::ActionInvocationContext);

  CueTarget* GetTarget(CueTargetType type);

  // Not owned. Guaranteed to outlive `this`.
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<TabListInterface> tab_list_interface_;
  raw_ptr<ContextualCueingService> contextual_cueing_service_;
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
  absl::flat_hash_map<CueTargetType, std::unique_ptr<CueTarget>> cue_targets_;

  base::WeakPtrFactory<ContextualCueingController> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
