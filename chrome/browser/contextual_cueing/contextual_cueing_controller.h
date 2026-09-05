// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_

#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_callback.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/contextual_cueing/contextual_cueing_enums.h"
#include "components/favicon_base/favicon_types.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/sessions/core/session_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class OptimizationGuideKeyedService;
class TabListInterface;
class TemplateURLService;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace page_actions {
class PageActionController;
class PageActionObserver;
enum class PageActionPriorityCategory;
}  // namespace page_actions

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_cueing {

class ContextualCueingService;
struct CueTabMetrics;

class ContextualCueingController
    : public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver,
      public TabListInterfaceObserver {
 public:
  explicit ContextualCueingController(tabs::TabInterface* tab);
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

  // TabListInterfaceObserver:
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason reason) override;

  void OnTabNavigated(tabs::TabInterface* tab);

  // V2 multi-source orchestration entry point. Performs shared pre-checks
  // (URL eligibility, quota/backoff), then fans out CheckEligibility calls to
  // all registered targets via base::BarrierCallback.
  void EvaluateCues();

  void OnUrlChanged(const GURL& url);

  // Hide the cue for this tab if it's showing.
  void HideCue();

  // Returns the CueTarget for the given CueTargetType, or nullptr if there is
  // none.
  CueTarget* GetTarget(CueTargetType type);

  void OnCueInteraction(
      ContextualCueingInteraction interaction_type,
      CueTargetType cue_type,
      const optimization_guide::proto::ContextualCue& cue,
      const std::vector<tabs::TabHandle>& tabs_to_show,
      const std::vector<optimization_guide::proto::Tab>& background_tabs,
      const std::string& cuj,
      CueActionData action,
      std::string cue_id);

  // Called when the anchored contextual cue action is invoked.
  void OnActionInvoked();

 private:
  // Initiates a model execution request to MES for the current window state,
  // requesting only surfaces for the winning target.
  void InitiateModelExecutionRequest(CueTargetType winning_target_type);

  // The V1 single-source evaluation path, triggered by page content
  // annotations. Performs URL eligibility checks, category classification
  // (delegated to the registered CueTarget), quota/backoff checks, and then
  // invokes InitiateModelExecutionRequest() if all checks pass.
  //
  // TODO(b/523306363): Remove once the V2 multi-source path is fully launched.
  void RunGlicSingleSourcePath(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result);

  // Result of a single target's CheckEligibility round-trip, collected by the
  // barrier and forwarded to OnAllEligibilityChecksComplete.
  struct EligibilityResult {
    CueTargetType type;
    bool eligible;
    CueTarget::ContentGenerator generator;
  };

  // Called when every registered target has responded to CheckEligibility.
  // Applies UCB scoring to the eligible candidates, selects the winner, and
  // either runs its ContentGenerator or delegates to
  // InitiateModelExecutionRequest().
  void OnAllEligibilityChecksComplete(
      base::WeakPtr<content::WebContents> web_contents,
      GURL url,
      CueIntrusiveness intrusiveness,
      std::vector<EligibilityResult> results);

  // Called when a target's ContentGenerator completes. Shows the cue or
  // records a failure.
  void OnContentGenerated(
      CueTargetType type,
      CueIntrusiveness intrusiveness,
      std::optional<optimization_guide::proto::ContextualCue> cue);

  // Retrieves favicon for a specific web contents.
  void FetchFavicon(tabs::TabInterface* tab,
                    content::WebContents* web_contents);

  // Calculate the amount of time the current cue has been shown, and reset the
  // shown timestamp.
  base::TimeDelta ExtractCueShownDuration();

  // Callback for when the model execution response is received.
  void OnModelExecutionResponseReceived(
      optimization_guide::proto::Tab active_tab,
      std::vector<optimization_guide::proto::Tab> background_tabs,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Whether the URL is eligible for a cue.
  bool IsUrlEligibleForCue(const GURL& url);

  // Returns ContextualCueingDecision::kUnspecified if the cue should be shown
  // to the user, or the specific decision explaining why it is not allowed.
  ContextualCueingDecision IsAllowedToShowCue();

  // Returns true if the user is subject to age restrictions.
  bool IsUserSubjectToAgeRestrictions();

  // Returns the tab's ukm::SourceId, or ukm::kInvalidSourceId if there
  // is no WebContents.
  ukm::SourceId GetTabSourceId() const;

  std::pair<std::vector<tabs::TabHandle>, CueTabMetrics> GetTabsToShow(
      const optimization_guide::proto::ContextualCue& cue);

  void ShowCue(
      CueTargetType cue_type,
      CueIntrusiveness intrusiveness,
      const CueTarget& target,
      const optimization_guide::proto::ContextualCue& cue,
      const std::vector<optimization_guide::proto::Tab>& background_tabs);
#if !BUILDFLAG(IS_ANDROID)
  void MaybeShowTabList(
      page_actions::PageActionController* page_action_controller,
      const std::vector<tabs::TabHandle>& tabs_to_show);
#endif
  struct ActiveCueData {
    ActiveCueData(CueTargetType cue_type,
                  optimization_guide::proto::ContextualCue cue,
                  std::vector<tabs::TabHandle> tabs_to_show,
                  std::vector<optimization_guide::proto::Tab> background_tabs,
                  std::string cuj,
                  CueActionData action_data,
                  std::string cue_id);
    ~ActiveCueData();
    ActiveCueData(const ActiveCueData&);
    ActiveCueData& operator=(const ActiveCueData&);

    CueTargetType cue_type;
    optimization_guide::proto::ContextualCue cue;
    std::vector<tabs::TabHandle> tabs_to_show;
    std::vector<optimization_guide::proto::Tab> background_tabs;
    std::string cuj;
    CueActionData action_data;
    std::string cue_id;
  };

  void OnCueClicked(CueTargetType cue_type,
                    optimization_guide::proto::ContextualCue cue,
                    std::vector<tabs::TabHandle> tabs_to_show,
                    std::vector<optimization_guide::proto::Tab> background_tabs,
                    std::string cuj,
                    CueActionData action,
                    std::string cue_id);
  void OnCueHidden();
  void OnCueFormFactorShown(CueFormFactor form_factor);
  void OnCueFormFactorHidden(CueFormFactor form_factor);
  void OnFaviconAvailable(tabs::TabHandle handle,
                          const favicon_base::FaviconImageResult& image_result);
  void OnShowCueFailed(ContextualCueingDecision decision);

  // Logs and records a contextual cueing decision for this tab to UKM.
  void RecordContextualCueingDecision(ContextualCueingDecision decision);

  void OnSidePanelShown();

  void OnTabActivated(tabs::TabInterface* tab);
  void OnTabDetached(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);
  void OnTabInserted(tabs::TabInterface* tab);
  void ObserveTabList();

  // Starts observing the SidePanelUI to detect when it is shown.
  void ObserveSidePanel();

  // Returns the list of cue surfaces that are currently eligible to show a cue.
  absl::flat_hash_set<optimization_guide::proto::ContextualCueingSurface>
  GetEligibleCueSurfaces();

  // Not owned. Guaranteed to outlive `this`.
  const raw_ptr<tabs::TabInterface> tab_;
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  std::set<SessionID> dependencies_;
  std::optional<ActiveCueData> active_cue_data_;
  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};
  raw_ptr<ContextualCueingService> contextual_cueing_service_;
  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<TemplateURLService> template_url_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<favicon::FaviconService> favicon_service_;
  absl::flat_hash_map<CueTargetType, std::unique_ptr<CueTarget>> cue_targets_;
  base::CallbackListSubscription side_panel_shown_subscription_;
  base::TimeTicks cue_shown_time_;
  base::TimeTicks cue_hidden_time_;

  absl::flat_hash_map<tabs::TabHandle, ui::ImageModel> tab_favicons_;
  base::CancelableTaskTracker cancelable_task_tracker_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<page_actions::PageActionObserver> page_action_observer_;
  std::optional<page_actions::PageActionPriorityCategory>
      current_anchored_message_priority_;
#endif

  GURL last_logged_active_url_;

  base::WeakPtrFactory<ContextualCueingController> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CONTROLLER_H_
