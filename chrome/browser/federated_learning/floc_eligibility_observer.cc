// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_eligibility_observer.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/federated_learning/features/features.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace federated_learning {

namespace {

history::HistoryService* GetHistoryService(content::WebContents* web_contents) {
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return nullptr;

  return HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

}  // namespace

FlocEligibilityObserver::~FlocEligibilityObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FlocEligibilityObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // At this point the add-page-to-history decision should have been made,
  // because history is added in HistoryTabHelper::DidFinishNavigation, and this
  // OnEligibleCommit method is invoked in the same broadcasting family through
  // MetricsWebContentsObserver::DidFinishNavigation.

  // TODO(yaoxia): Perhaps we want an explicit signal for "the page was added
  // to history or was ineligible". This way we don't need to count on the above
  // relation, and can also stop observing if the history was not added.

  // If the IP was not publicly routable, the navigation history is not eligible
  // for floc. We can stop observing now.
  if (!navigation_handle->GetSocketAddress().address().IsPubliclyRoutable())
    return ObservePolicy::STOP_OBSERVING;

  // If the interest-cohort permissions policy in the main document disallows
  // the floc inclusion, the navigation history is not eligible for floc. We can
  // stop observing now.
  if (!navigation_handle->GetRenderFrameHost()->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kInterestCohort)) {
    return ObservePolicy::STOP_OBSERVING;
  }

  DCHECK(!eligible_commit_);
  eligible_commit_ = true;

  return ObservePolicy::CONTINUE_OBSERVING;
}

void FlocEligibilityObserver::OnAdResource() {
  if (!base::FeatureList::IsEnabled(
          kFlocPagesWithAdResourcesDefaultIncludedInFlocComputation)) {
    return;
  }

  OnOptInSignalObserved();
}

void FlocEligibilityObserver::OnInterestCohortApiUsed() {
  OnOptInSignalObserved();
}

FlocEligibilityObserver::FlocEligibilityObserver(content::RenderFrameHost* rfh)
    : web_contents_(content::WebContents::FromRenderFrameHost(rfh)) {}

void FlocEligibilityObserver::OnOptInSignalObserved() {
  if (!eligible_commit_ || observed_opt_in_signal_)
    return;

  if (history::HistoryService* hs = GetHistoryService(web_contents_)) {
    hs->SetFlocAllowed(
        history::ContextIDForWebContents(web_contents_),
        web_contents_->GetController().GetLastCommittedEntry()->GetUniqueID(),
        web_contents_->GetLastCommittedURL());
  }

  observed_opt_in_signal_ = true;
}

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(FlocEligibilityObserver)

}  // namespace federated_learning
