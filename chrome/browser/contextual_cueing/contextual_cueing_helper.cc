// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#endif

namespace contextual_cueing {

namespace {

// Returns whether the `config` matches all the current cueing condition.
bool DidMatchCueingConditions(
    const optimization_guide::proto::GlicCueingConfiguration& config) {
  for (const auto& condition : config.conditions()) {
    switch (condition.signal()) {
      case optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_UNSPECIFIED:
      case optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT:
      case optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_CONTENT_LENGTH_WORD_COUNT:
        // TODO: crbug.com/389751174 - Implement checking the client signals.
        return false;
    }
  }
  return true;
}

class ScopedNudgeDecisionRecorder {
 public:
  ScopedNudgeDecisionRecorder(
      optimization_guide::proto::OptimizationType optimization_type)
      : optimization_type_(optimization_type) {}
  ~ScopedNudgeDecisionRecorder() {
    CHECK_NE(nudge_decision_, NudgeDecision::kUnknown);
    base::UmaHistogramEnumeration(
        "ContextualCueing.NudgeDecision." +
            optimization_guide::GetStringNameForOptimizationType(
                optimization_type_),
        nudge_decision_);
  }

  void set_nudge_decision(NudgeDecision nudge_decision) {
    nudge_decision_ = nudge_decision;
  }

  NudgeDecision nudge_decision() const { return nudge_decision_; }

 private:
  optimization_guide::proto::OptimizationType optimization_type_;
  NudgeDecision nudge_decision_ = NudgeDecision::kUnknown;
};

}  // namespace

ContextualCueingHelper::ContextualCueingHelper(
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* ogks,
    ContextualCueingService* ccs)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingHelper>(*web_contents),
      optimization_guide_keyed_service_(ogks),
      contextual_cueing_service_(ccs) {
  // LINT.IfChange(OptType)
  optimization_guide_keyed_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::GLIC_CONTEXTUAL_CUEING});
  // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/histograms.xml:OptType)
}

ContextualCueingHelper::~ContextualCueingHelper() = default;

tabs::GlicNudgeController* ContextualCueingHelper::GetGlicNudgeController() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    return nullptr;
  }
  return browser->browser_window_features()->glic_nudge_controller();
}

void ContextualCueingHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  if (PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }
  contextual_cueing_service_->ReportPageLoad(navigation_handle->GetURL());
  auto* glic_nudge_controller = GetGlicNudgeController();
  if (glic_nudge_controller) {
    glic_nudge_controller->UpdateNudgeLabel(web_contents(), std::string(),
                                            base::DoNothing());
  }
}

void ContextualCueingHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  last_navigation_cue_label_.clear();

  auto* glic_nudge_controller = GetGlicNudgeController();
  if (!glic_nudge_controller) {
    return;
  }

  ScopedNudgeDecisionRecorder recorder(
      optimization_guide::proto::GLIC_CONTEXTUAL_CUEING);
  const GURL& url = web_contents()->GetLastCommittedURL();
  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_keyed_service_->CanApplyOptimization(
      url, optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, &metadata);
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue &&
      !metadata.empty()) {
    auto parsed = metadata.ParsedMetadata<
        optimization_guide::proto::GlicContextualCueingMetadata>();
    if (parsed) {
      for (const auto& config : parsed->cueing_configurations()) {
        if (!config.has_cue_label()) {
          continue;
        }

        if (DidMatchCueingConditions(config)) {
          last_navigation_cue_label_ = config.cue_label();
          break;
        }
      }

      recorder.set_nudge_decision(
          last_navigation_cue_label_.empty()
              ? NudgeDecision::kClientConditionsUnmet
              : contextual_cueing_service_->CanShowNudge());
    } else {
      recorder.set_nudge_decision(NudgeDecision::kServerDataMalformed);
    }
  } else {
    recorder.set_nudge_decision(NudgeDecision::kServerDataUnavailable);
  }

  if (recorder.nudge_decision() != NudgeDecision::kSuccess) {
    // Clear out the label since we didn't show it.
    last_navigation_cue_label_.clear();
  }
  glic_nudge_controller->UpdateNudgeLabel(
      web_contents(), last_navigation_cue_label_,
      base::BindRepeating(
          &ContextualCueingService::OnNudgeActivity,
          contextual_cueing_service_->GetWeakPtr(), url,
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()));
}

// static
void ContextualCueingHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing)) {
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!GlicEnabling::IsProfileEligible(profile)) {
    return;
  }

  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service ||
      !optimization_guide_keyed_service->GetModelExecutionFeaturesController()
           ->ShouldModelExecutionBeAllowedForUser()) {
    return;
  }

  auto* contextual_cueing_service =
      ContextualCueingServiceFactory::GetForProfile(profile);
  if (!contextual_cueing_service) {
    return;
  }

  ContextualCueingHelper::CreateForWebContents(web_contents,
                                               optimization_guide_keyed_service,
                                               contextual_cueing_service);
#endif  // BUILDFLAG(ENABLE_GLIC)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingHelper);

}  // namespace contextual_cueing
