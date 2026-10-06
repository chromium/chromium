// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace contextual_search {
namespace {

GURL UrlWithoutQueryAndRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // namespace

DEFINE_USER_DATA(ContextualSearchCueTabState);

ContextualSearchCueTabState::ContextualSearchCueTabState(
    tabs::TabInterface& tab)
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
ContextualSearchCueTabState* ContextualSearchCueTabState::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

ContextualSearchCueTabState::~ContextualSearchCueTabState() {
  CancelPendingCheck();
  if (annotation_service_) {
    annotation_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
}

void ContextualSearchCueTabState::DidFinishNavigation(
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

void ContextualSearchCueTabState::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  if (UrlWithoutQueryAndRef(visit.url) !=
      UrlWithoutQueryAndRef(last_committed_url_)) {
    CUEING_LOG(base::StringPrintf(
        "ContextualSearchCueTabState::OnPageContentAnnotated URL mismatch: %s "
        "vs %s",
        visit.url.spec(), last_committed_url_.spec()));
    return;
  }

  CUEING_LOG(base::StringPrintf(
      "ContextualSearchCueTabState::OnPageContentAnnotated received annotation "
      "for %s",
      visit.url.spec()));
  cached_result_ = result;
  ResolvePendingCheck();
}

void ContextualSearchCueTabState::CheckEligibility(
    contextual_cueing::CueIntrusiveness intrusiveness,
    contextual_cueing::CueTarget::EligibilityCallback callback,
    base::WeakPtr<contextual_cueing::CueTarget> target) {
  if (!annotation_service_) {
    CUEING_LOG(
        "ContextualSearchCueTabState::CheckEligibility failed: No annotation "
        "service.");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false,
                       contextual_cueing::CueTarget::ContentGenerator()));
    return;
  }

  if (cached_result_.has_value()) {
    const bool eligible =
        target && target->IsPageEligible(*cached_result_, web_contents());
    CUEING_LOG(base::StringPrintf(
        "ContextualSearchCueTabState::CheckEligibility using cached result: "
        "eligible=%d",
        eligible));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), eligible,
                       contextual_cueing::CueTarget::ContentGenerator()));
    return;
  }

  CUEING_LOG(
      "ContextualSearchCueTabState::CheckEligibility waiting for annotation.");
  CancelPendingCheck();

  pending_check_ = PendingCheck{
      .intrusiveness = intrusiveness,
      .callback = std::move(callback),
      .target = std::move(target),
  };
  annotation_timeout_timer_.Start(
      FROM_HERE, contextual_cueing::kAnnotationTimeout.Get(), this,
      &ContextualSearchCueTabState::OnAnnotationTimeout);
}

void ContextualSearchCueTabState::CancelPendingCheck() {
  if (pending_check_.has_value()) {
    CUEING_LOG(
        "ContextualSearchCueTabState::CancelPendingCheck: cancelling pending "
        "check.");
    annotation_timeout_timer_.Stop();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending_check_->callback), false,
                       contextual_cueing::CueTarget::ContentGenerator()));
    pending_check_.reset();
  }
}

void ContextualSearchCueTabState::ResolvePendingCheck() {
  if (!pending_check_.has_value() || !cached_result_.has_value()) {
    CUEING_LOG(
        "ContextualSearchCueTabState::ResolvePendingCheck: no pending check or "
        "cached result.");
    return;
  }

  annotation_timeout_timer_.Stop();
  contextual_cueing::CueTarget::EligibilityCallback callback =
      std::move(pending_check_->callback);
  base::WeakPtr<contextual_cueing::CueTarget> target = pending_check_->target;
  pending_check_.reset();

  const bool eligible =
      target && target->IsPageEligible(*cached_result_, web_contents());
  CUEING_LOG(base::StringPrintf(
      "ContextualSearchCueTabState::ResolvePendingCheck resolved pending "
      "check: eligible=%d",
      eligible));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), eligible,
                     contextual_cueing::CueTarget::ContentGenerator()));
}

void ContextualSearchCueTabState::OnAnnotationTimeout() {
  CUEING_LOG(
      "ContextualSearchCueTabState::OnAnnotationTimeout: annotation timed "
      "out.");
  CancelPendingCheck();
}

}  // namespace contextual_search
