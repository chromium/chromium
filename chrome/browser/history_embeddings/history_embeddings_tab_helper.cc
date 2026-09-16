// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/content/history_embeddings_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"

HistoryEmbeddingsTabHelper::HistoryEmbeddingsTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryEmbeddingsTabHelper>(*web_contents) {}

HistoryEmbeddingsTabHelper::~HistoryEmbeddingsTabHelper() = default;

void HistoryEmbeddingsTabHelper::OnUpdatedHistoryForNavigation(
    int64_t navigation_id,
    bool is_in_primary_main_frame,
    base::Time timestamp,
    const GURL& url) {
  if (!is_in_primary_main_frame ||
      !history_embeddings::IsHistoryEmbeddingsEnabledForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())) ||
      !GetHistoryEmbeddingsService() || !GetPassageEmbedderModelObserver()) {
    return;
  }

  // Invalidate existing weak pointers to cancel any outstanding delayed tasks.
  // Since this is a new navigation, the document may have changed and so
  // the visit time and URL data would not match the web content.
  CancelHistoryLookup();

  // Save data for later use in `DidFinishLoad`.
  history_navigation_id_ = navigation_id;
  history_visit_time_ = timestamp;
  history_url_ = url;

  if (history_embeddings::HistoryEmbeddingsService* service =
          GetHistoryEmbeddingsService()) {
    service->UpdateVisitMetadata(web_contents(), std::nullopt);
  }
}

void HistoryEmbeddingsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (navigation_handle->IsSameDocument() ||
      navigation_handle->IsServedFromBackForwardCache()) {
    // Same-document and BFCache navigations do not trigger a fresh
    // DidFinishLoad, so clear any pending navigation state so it does not
    // linger.
    CancelHistoryLookup();
    history_navigation_id_.reset();
    history_visit_time_.reset();
    history_url_.reset();
    if (history_embeddings::HistoryEmbeddingsService* service =
            GetHistoryEmbeddingsService()) {
      service->UpdateVisitMetadata(web_contents(), std::nullopt);
    }
    return;
  }

  // For cross-document navigations:
  // If this navigation did not update history (e.g. 404 response, network
  // error, or ineligible navigation), clear any stale history data and cancel
  // pending lookups.
  if (!history_navigation_id_.has_value() ||
      *history_navigation_id_ != navigation_handle->GetNavigationId()) {
    CancelHistoryLookup();
    history_navigation_id_.reset();
    history_visit_time_.reset();
    history_url_.reset();
  }

  if (history_embeddings::HistoryEmbeddingsService* service =
          GetHistoryEmbeddingsService()) {
    service->UpdateVisitMetadata(web_contents(), std::nullopt);
  }
}

void HistoryEmbeddingsTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // Invalidate existing weak pointers to cancel any outstanding delayed tasks.
  // Normally, navigation will have already canceled, but in the event that
  // `DidFinishLoad` is called twice without navigation, invalidating here
  // guarantees at most one delayed task is scheduled at a time.
  CancelHistoryLookup();

  if (!history_embeddings::IsHistoryEmbeddingsEnabledForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext())) ||
      !GetHistoryEmbeddingsService() ||
      !GetHistoryEmbeddingsService()->IsEligible(validated_url) ||
      !history_url_.has_value() || *history_url_ != validated_url) {
    history_navigation_id_.reset();
    history_visit_time_.reset();
    history_url_.reset();
    return;
  }

  history::HistoryService* history_service = GetHistoryService();
  if (!history_service) {
    history_navigation_id_.reset();
    history_visit_time_.reset();
    history_url_.reset();
    return;
  }

  // Callback is a member method instead of inline to enable cancellation via
  // weak pointer in `CancelHistoryLookup()`.
  history_service->GetMostRecentVisitsForGurl(
      history_url_.value(), 1, history::VisitQuery404sPolicy::kExclude404s,
      base::BindOnce(
          &HistoryEmbeddingsTabHelper::UpdateEmbeddingsServiceWithHistoryData,
          extraction_weak_ptr_factory_.GetWeakPtr()),
      &task_tracker_);
}

void HistoryEmbeddingsTabHelper::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    CancelHistoryLookup();
    history_navigation_id_.reset();
    history_visit_time_.reset();
    history_url_.reset();
    if (history_embeddings::HistoryEmbeddingsService* service =
            GetHistoryEmbeddingsService()) {
      service->UpdateVisitMetadata(web_contents(), std::nullopt);
    }
  }
}

void HistoryEmbeddingsTabHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CancelHistoryLookup();
  history_navigation_id_.reset();
  history_visit_time_.reset();
  history_url_.reset();
  if (history_embeddings::HistoryEmbeddingsService* service =
          GetHistoryEmbeddingsService()) {
    service->UpdateVisitMetadata(web_contents(), std::nullopt);
  }
}

void HistoryEmbeddingsTabHelper::WebContentsDestroyed() {
  CancelHistoryLookup();
  history_navigation_id_.reset();
  history_visit_time_.reset();
  history_url_.reset();
  if (history_embeddings::HistoryEmbeddingsService* service =
          GetHistoryEmbeddingsService()) {
    service->UpdateVisitMetadata(web_contents(), std::nullopt);
  }
}

void HistoryEmbeddingsTabHelper::SetHistoryTabHelperSubscription(
    base::CallbackListSubscription subscription) {
  history_tab_helper_subscription_ = std::move(subscription);
}

base::WeakPtr<HistoryEmbeddingsTabHelper>
HistoryEmbeddingsTabHelper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryEmbeddingsTabHelper::UpdateEmbeddingsServiceWithHistoryData(
    history::QueryURLAndVisitsResult result) {
  std::optional<base::Time> expected_visit_time = history_visit_time_;
  std::optional<GURL> expected_url = history_url_;

  // Clear the data. It isn't reused and will be set anew by later navigation.
  history_navigation_id_.reset();
  history_visit_time_.reset();
  history_url_.reset();

  // `visits` can be empty for navigations that don't result in a
  // visit being added to the DB, e.g. navigations to
  // "chrome://" URLs.
  if (!result.success || result.visits.empty()) {
    return;
  }
  const history::URLRow& url_row = result.row;
  const history::VisitRow& latest_visit = result.visits[0];
  CHECK(url_row.id());
  CHECK(latest_visit.visit_id);
  CHECK_EQ(url_row.id(), latest_visit.url_id);
  // Make sure the visit we got actually corresponds to the
  // navigation by comparing the visit_times.
  if (!expected_visit_time.has_value() ||
      latest_visit.visit_time != *expected_visit_time) {
    return;
  }
  // Make sure the URL from the history query matches the expected URL and
  // the currently committed URL of the WebContents.
  if (!expected_url.has_value() || url_row.url() != *expected_url) {
    return;
  }
  if (web_contents()->GetLastCommittedURL() != url_row.url()) {
    return;
  }
  // Make sure the latest visit (the first one in the array) is
  // a local one. That should almost always be the case, since
  // this gets called just after a local visit happened, but in
  // some rare cases it might not be, e.g. if another device
  // sent us a visit "from the future". If this turns out to be
  // a problem, consider implementing a
  // GetMostRecent*Local*VisitsForURL().
  if (!latest_visit.originator_cache_guid.empty()) {
    return;
  }

  if (history_embeddings::HistoryEmbeddingsService* service =
          GetHistoryEmbeddingsService()) {
    service->UpdateVisitMetadata(
        web_contents(),
        history_embeddings::HistoryEmbeddingsService::VisitMetadata{
            latest_visit.url_id, latest_visit.visit_id,
            latest_visit.visit_time});
  }
}

void HistoryEmbeddingsTabHelper::CancelHistoryLookup() {
  extraction_weak_ptr_factory_.InvalidateWeakPtrs();
  task_tracker_.TryCancelAll();
}

history_embeddings::HistoryEmbeddingsService*
HistoryEmbeddingsTabHelper::GetHistoryEmbeddingsService() {
  CHECK(web_contents());
  return HistoryEmbeddingsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

passage_embeddings::PassageEmbedderModelObserver*
HistoryEmbeddingsTabHelper::GetPassageEmbedderModelObserver() {
  CHECK(web_contents());
  return passage_embeddings::PassageEmbedderModelObserverFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

history::HistoryService* HistoryEmbeddingsTabHelper::GetHistoryService() {
  CHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return HistoryServiceFactory::GetForProfileIfExists(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryEmbeddingsTabHelper);
