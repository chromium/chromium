// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#endif

namespace contextual_cueing {

ContextualCueingHelper::ContextualCueingHelper(
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* ogks)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingHelper>(*web_contents),
      optimization_guide_keyed_service_(ogks) {
  optimization_guide_keyed_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW});
}

ContextualCueingHelper::~ContextualCueingHelper() = default;

// content::WebContentsObserver
void ContextualCueingHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  last_navigation_cue_label_.clear();
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    return;
  }
  auto* glic_nudge_controller =
      browser->browser_window_features()->glic_nudge_controller();
  if (!glic_nudge_controller) {
    return;
  }

  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_keyed_service_->CanApplyOptimization(
      web_contents()->GetLastCommittedURL(),
      optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW, &metadata);
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue &&
      !metadata.empty()) {
    auto parsed = metadata.ParsedMetadata<
        optimization_guide::proto::OptimizationGuideIconViewMetadata>();
    if (parsed->has_cue_label()) {
      last_navigation_cue_label_ = parsed->cue_label();
    }
  }

  glic_nudge_controller->UpdateNudgeLabel(web_contents(),
                                          last_navigation_cue_label_);
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

  ContextualCueingHelper::CreateForWebContents(
      web_contents, optimization_guide_keyed_service);
#endif  // BUILDFLAG(ENABLE_GLIC)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingHelper);

}  // namespace contextual_cueing
