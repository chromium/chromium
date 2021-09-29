// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ELIGIBILITY_OBSERVER_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ELIGIBILITY_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/render_document_host_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace federated_learning {

// This observer monitors page-level (i.e. main document) signals to determine
// whether the navigation's associated history entry is eligible for floc
// computation. The history entry is eligible for floc computation if all of the
// following conditions hold:
// 1) the IP of the navigation was publicly routable.
// 2) the interest-cohort permissions policy in the main document allows the
// floc history inclusion.
// 3) either the page has an ad resource, or the document.interestCohort API is
// used in the page.
//
// When the page is considered eligible for floc computation, a corresponding
// HistoryService API will be called to persistently set the eligibility bit.
class FlocEligibilityObserver
    : public content::RenderDocumentHostUserData<FlocEligibilityObserver> {
 public:
  using ObservePolicy =
      page_load_metrics::PageLoadMetricsObserver::ObservePolicy;

  ~FlocEligibilityObserver() override;

  // Called when the navigation has committed. Returns whether it should
  // continue observing floc related signals.
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle);

  // Called when an ad resource is seen in the page.
  void OnAdResource();

  // Called when the document.interestCohort API is used in the page.
  void OnInterestCohortApiUsed();

 private:
  explicit FlocEligibilityObserver(content::RenderFrameHost* rfh);

  friend class content::RenderDocumentHostUserData<FlocEligibilityObserver>;

  void OnOptInSignalObserved();

  content::WebContents* web_contents_;

  // |eligible_commit_| means all the commit time prerequisites are met
  // (i.e. IP was publicly routable AND permissions policy is "allow"). It can
  // only be set to true at commit time. When it's set, it also implies that the
  // add-page-to-history decision has been made, i.e. either the page has been
  // added to history, or has been skipped.
  bool eligible_commit_ = false;

  bool observed_opt_in_signal_ = false;

  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ELIGIBILITY_OBSERVER_H_
