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
class TemplateURLService;

namespace actions {
class ActionItem;
class ActionInvocationContext;
}  // namespace actions

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace page_actions {
class PageActionObserver;
}  // namespace page_actions

namespace syncer {
class SyncService;
}  // namespace syncer

namespace contextual_cueing {

class ContextualCueingService;

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

  // Get contents' browser's ContextualCueingController if it exists.
  static ContextualCueingController* GetForWebContents(
      content::WebContents& contents);

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

  // Hide the cue if it's showing.
  void HideCue();

  // Returns the CueTarget for the given CueTargetType, or nullptr if there is
  // none.
  CueTarget* GetTarget(CueTargetType type);
  // Getter function for CUJ of shown cue
  const std::string& current_cuj() const { return current_cuj_; }

 private:
  // Initiates a model execution request to MES for the current window state.
  void InitiateModelExecutionRequest();

  // Callback for when the model execution response is received.
  void OnModelExecutionResponseReceived(
      optimization_guide::proto::Tab active_tab,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Whether the URL is eligible for a cue.
  bool IsUrlEligibleForCue(const GURL& url);

  // Returns true if the cue should be shown to the user.
  bool IsAllowedToShowCue();

  void ShowCue(CueTargetType cue_type,
               const CueTarget& target,
               optimization_guide::proto::ContextualCueingResponse response);
  void OnCueClicked(CueTargetType cue_type,
                    CueActionData data,
                    actions::ActionItem*,
                    actions::ActionInvocationContext);
  void OnCueHidden();

  // Returns the list of cue surfaces that are currently eligible to show a cue.
  absl::flat_hash_set<optimization_guide::proto::ContextualCueingSurface>
  GetEligibleCueSurfaces();

  // Not owned. Guaranteed to outlive `this`.
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<TabListInterface> tab_list_interface_;
  raw_ptr<ContextualCueingService> contextual_cueing_service_;
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<TemplateURLService> template_url_service_;
  absl::flat_hash_map<CueTargetType, std::unique_ptr<CueTarget>> cue_targets_;
  std::string current_cuj_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<page_actions::PageActionObserver> page_action_observer_;
#endif

  base::WeakPtrFactory<ContextualCueingController> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
