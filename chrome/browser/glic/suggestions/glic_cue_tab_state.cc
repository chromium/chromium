// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_tab_state.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/suggestions/glic_cue_target.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace glic {

DEFINE_USER_DATA(GlicCueTabState);

GlicCueTabState::GlicCueTabState(tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  content::WebContents* web_contents = tab.GetContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (base::FeatureList::IsEnabled(
          contextual_cueing::kContextualCueingV2MultiSource)) {
    annotation_service_ =
        PageContentAnnotationsServiceFactory::GetForProfile(profile);
    if (annotation_service_) {
      annotation_service_->AddObserver(
          page_content_annotations::AnnotationType::kCategoryClassifier, this);
    }
  }
  last_committed_url_ = web_contents->GetLastCommittedURL();
}

// static
GlicCueTabState* GlicCueTabState::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

GlicCueTabState::~GlicCueTabState() {
  CancelPendingCheck();
  if (annotation_service_) {
    annotation_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
}

void GlicCueTabState::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  last_committed_url_ = navigation_handle->GetURL();
  cached_result_ = std::nullopt;

  CancelPendingCheck();
}

void GlicCueTabState::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  if (visit.url != last_committed_url_) {
    CUEING_LOG(base::StringPrintf(
        "GlicCueTabState::OnPageContentAnnotated URL mismatch: %s vs %s",
        visit.url.spec(), last_committed_url_.spec()));
    return;
  }

  CUEING_LOG(base::StringPrintf(
      "GlicCueTabState::OnPageContentAnnotated received annotation for %s",
      visit.url.spec()));
  cached_result_ = result;
  ResolvePendingCheck();
}

void GlicCueTabState::CheckEligibility(
    contextual_cueing::CueIntrusiveness intrusiveness,
    contextual_cueing::CueTarget::EligibilityCallback callback,
    GlicCueTarget* target) {
  if (!annotation_service_) {
    CUEING_LOG(
        "GlicCueTabState::CheckEligibility failed: No annotation service.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false,
                       contextual_cueing::CueTarget::ContentGenerator()));
    return;
  }

  if (cached_result_.has_value()) {
    bool eligible = target->IsPageEligible(*cached_result_, web_contents());
    CUEING_LOG(base::StringPrintf(
        "GlicCueTabState::CheckEligibility using cached result: eligible=%d",
        eligible));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), eligible,
                       contextual_cueing::CueTarget::ContentGenerator()));
    return;
  }

  CUEING_LOG("GlicCueTabState::CheckEligibility waiting for annotation.");
  CancelPendingCheck();

  pending_check_ = PendingCheck{
      .intrusiveness = intrusiveness,
      .callback = std::move(callback),
      .target = target->GetWeakPtr(),
  };
  annotation_timeout_timer_.Start(FROM_HERE,
                                  contextual_cueing::kAnnotationTimeout.Get(),
                                  this, &GlicCueTabState::OnAnnotationTimeout);
}

void GlicCueTabState::CancelPendingCheck() {
  if (pending_check_.has_value()) {
    CUEING_LOG(
        "GlicCueTabState::CancelPendingCheck: cancelling pending check.");
    annotation_timeout_timer_.Stop();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_check_->callback), false,
                       contextual_cueing::CueTarget::ContentGenerator()));
    pending_check_.reset();
  }
}

void GlicCueTabState::ResolvePendingCheck() {
  if (!pending_check_.has_value() || !cached_result_.has_value()) {
    return;
  }

  annotation_timeout_timer_.Stop();
  contextual_cueing::CueTarget::EligibilityCallback callback =
      std::move(pending_check_->callback);
  base::WeakPtr<GlicCueTarget> target = pending_check_->target;
  pending_check_.reset();

  const bool eligible =
      target && target->IsPageEligible(*cached_result_, web_contents());
  CUEING_LOG(base::StringPrintf(
      "GlicCueTabState::ResolvePendingCheck resolved pending check: "
      "eligible=%d",
      eligible));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), eligible,
                     contextual_cueing::CueTarget::ContentGenerator()));
}

void GlicCueTabState::OnAnnotationTimeout() {
  CUEING_LOG("GlicCueTabState::OnAnnotationTimeout: annotation timed out.");
  CancelPendingCheck();
}

}  // namespace glic
