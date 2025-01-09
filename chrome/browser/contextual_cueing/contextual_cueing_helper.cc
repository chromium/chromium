// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace contextual_cueing {

ContextualCueingHelper::ContextualCueingHelper(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* decider)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingHelper>(*web_contents),
      optimization_guide_decider_(decider) {
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW});
}

ContextualCueingHelper::~ContextualCueingHelper() = default;

// content::WebContentsObserver
void ContextualCueingHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  optimization_guide::OptimizationMetadata metadata;
  auto decision = optimization_guide_decider_->CanApplyOptimization(
      web_contents()->GetLastCommittedURL(),
      optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW, &metadata);
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      metadata.empty()) {
    return;
  }

  auto parsed = metadata.ParsedMetadata<
      optimization_guide::proto::OptimizationGuideIconViewMetadata>();
  // TODO(crbug.com/388305688): trigger cueing UI.
}

// static
std::unique_ptr<ContextualCueingHelper>
ContextualCueingHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing)) {
    return nullptr;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_decider) {
    return nullptr;
  }

  return base::WrapUnique<ContextualCueingHelper>(
      new ContextualCueingHelper(web_contents, optimization_guide_decider));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingHelper);

}  // namespace contextual_cueing
