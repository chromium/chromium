// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/tab_context_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace context_hub {

namespace {

base::TimeDelta GetDurationOfCurrentOrLastVisit(
    content::WebContents* web_contents) {
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    if (contextual_tasks::ContextualTasksTabVisitTracker* tracker =
            contextual_tasks::ContextualTasksTabVisitTracker::From(tab)) {
      return tracker->GetDurationOfCurrentOrLastVisit();
    }
  }
  return base::TimeDelta();
}

}  // namespace

TabContextFetcher::TabContextFetcher(
    page_content_annotations::PageContentExtractionService& extraction_service,
    content::WebContents* web_contents,
    TabContextCallback callback)
    : content::WebContentsObserver(web_contents),
      extraction_service_(extraction_service),
      callback_(std::move(callback)) {
  CHECK(callback_);
}

TabContextFetcher::~TabContextFetcher() = default;

void TabContextFetcher::Start() {
  if (started_) {
    return;
  }
  started_ = true;

  if (!web_contents()) {
    Finish(std::nullopt, std::nullopt);
    return;
  }

  web_contents()->GetController().LoadIfNecessary();

  // If the tab is currently navigating, wait for the navigation in the primary
  // main frame to commit before requesting extraction so that
  // `GetPrimaryPage()` refers to the newly committed page.
  // PageContentExtractionService handles waiting for post-commit loading and
  // page settling.
  if (web_contents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    timer_.Start(FROM_HERE, features::kTabLoadTimeout.Get(),
                 base::BindOnce(&TabContextFetcher::OnTimeout,
                                weak_factory_.GetWeakPtr()));
  } else {
    ExtractPageContent();
  }
}

void TabContextFetcher::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the navigation is not in the primary main frame (e.g. subframe) or did
  // not commit (e.g. aborted/failed), do not stop the timer or extract content.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  timer_.Stop();
  ExtractPageContent();
}

void TabContextFetcher::WebContentsDestroyed() {
  Finish(std::nullopt, std::nullopt);
}

void TabContextFetcher::OnTimeout() {
  if (!web_contents()) {
    Finish(std::nullopt, std::nullopt);
    return;
  }

  // If loading or navigation has not finished within the timeout, do not
  // attempt extraction because APC service requires a loaded page and will
  // no-op. Complete with tab metadata and nullopt page context.
  if (web_contents()->IsLoading() ||
      web_contents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    Finish(GetTabData(), std::nullopt);
    return;
  }

  ExtractPageContent();
}

void TabContextFetcher::ExtractPageContent() {
  if (extraction_started_) {
    return;
  }
  extraction_started_ = true;
  timer_.Stop();

  if (!web_contents()) {
    Finish(std::nullopt, std::nullopt);
    return;
  }

  content::Page& primary_page = web_contents()->GetPrimaryPage();
  base::WeakPtr<content::Page> page_weak_ptr = primary_page.GetWeakPtr();
  extraction_service_->GetExtractedPageContentAndEligibilityForPageAsync(
      primary_page,
      base::BindOnce(&TabContextFetcher::OnExtractionComplete,
                     weak_factory_.GetWeakPtr(), std::move(page_weak_ptr)),
      /*trigger_if_not_cached=*/true);
}

void TabContextFetcher::OnExtractionComplete(
    base::WeakPtr<content::Page> page,
    std::optional<page_content_annotations::ExtractedPageContentResult>
        extracted_result) {
  if (!web_contents() || !page || !page->IsPrimary()) {
    Finish(std::nullopt, std::nullopt);
    return;
  }

  std::optional<TabData> tab = GetTabData();
  std::optional<optimization_guide::proto::PageContext> page_context;
  if (extracted_result && extracted_result->page_content) {
    page_context.emplace();
    *page_context->mutable_annotated_page_content() =
        extracted_result->page_content->data;
  }
  Finish(std::move(tab), std::move(page_context));
}

std::optional<TabData> TabContextFetcher::GetTabData() const {
  if (!web_contents()) {
    return std::nullopt;
  }
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  if (!session_id.is_valid()) {
    return std::nullopt;
  }
  GURL url = web_contents()->GetLastCommittedURL();
  if (!url.is_valid()) {
    return std::nullopt;
  }
  TabData tab;
  tab.id = session_id.id();
  tab.title = base::UTF16ToUTF8(web_contents()->GetTitle());
  tab.url = std::move(url);
  tab.last_active_time = web_contents()->GetLastActiveTime();
  tab.last_foreground_duration =
      GetDurationOfCurrentOrLastVisit(web_contents());
  return tab;
}

void TabContextFetcher::Finish(
    std::optional<TabData> tab,
    std::optional<optimization_guide::proto::PageContext> page_context) {
  timer_.Stop();
  Observe(nullptr);
  weak_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(
      std::make_pair(std::move(tab), std::move(page_context)));
}

}  // namespace context_hub
