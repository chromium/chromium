// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace ttc {

namespace {

// Builds the result for APC that PCES has extracted, gated on its eligibility
// for server upload. Shares `page_content` rather than copying it.
PageContextResult BuildPageContextResult(
    page_content_annotations::RefCountedAnnotatedPageContentPtr page_content,
    bool is_eligible_for_server_upload) {
  if (!page_content) {
    return base::unexpected(
        page_content_annotations::FetchPageContextError::kUnknown);
  }
  if (!is_eligible_for_server_upload) {
    return base::unexpected(page_content_annotations::FetchPageContextError::
                                kPageContextNotEligible);
  }
  return PageContext(std::move(page_content));
}

}  // namespace

TtcPageContextMonitor::TtcPageContextMonitor(
    content::WebContents& web_contents,
    PageContextInvalidatedCallback on_page_context_invalidated,
    PageContextFetchedCallback on_page_context_fetched)
    : web_contents_(web_contents.GetWeakPtr()),
      extraction_service_(CHECK_DEREF(
          page_content_annotations::PageContentExtractionServiceFactory::
              GetForProfile(Profile::FromBrowserContext(
                  web_contents.GetBrowserContext())))),
      on_page_context_invalidated_(std::move(on_page_context_invalidated)),
      on_page_context_fetched_(std::move(on_page_context_fetched)) {
  CHECK(on_page_context_invalidated_);
  CHECK(on_page_context_fetched_);
  extraction_service_observation_.Observe(&extraction_service_.get());

  // Extract or retrieve cached context for the currently displayed page,
  // since PCES may have fired a completed page fetch before this object is
  // created.
  FetchPageContext();
}

TtcPageContextMonitor::~TtcPageContextMonitor() = default;

void TtcPageContextMonitor::FetchPageContext() {
  CHECK(web_contents_);

  content::Page& page = web_contents_->GetPrimaryPage();
  // PCES may have a cached extraction for this page. If so, use it; if not,
  // fetch one async.
  if (std::optional<page_content_annotations::ExtractedPageContentResult>
          extracted =
              extraction_service_->GetExtractedPageContentAndEligibilityForPage(
                  page)) {
    // Notify async so callbacks are never invoked synchronously during
    // construction.
    CHECK(pending_cached_notification_.IsCancelled());
    pending_cached_notification_.Reset(base::BindOnce(
        &TtcPageContextMonitor::NotifyPageContextFetched,
        base::Unretained(this),
        BuildPageContextResult(std::move(extracted->page_content),
                               extracted->is_eligible_for_server_upload)));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, pending_cached_notification_.callback());
  } else {
    // PCES will call its own fetch method, triggering OnPageContentExtracted,
    // when this async method actually extracts new APC.
    extraction_service_->GetExtractedPageContentAndEligibilityForPageAsync(
        page, base::DoNothing(), /*trigger_if_not_cached=*/true);
  }
}

void TtcPageContextMonitor::OnPageContentReset(content::Page& current_page,
                                               bool is_same_document) {
  if (!IsMonitoredPage(current_page)) {
    return;
  }

  pending_cached_notification_.Cancel();
  on_page_context_invalidated_.Run();
}

void TtcPageContextMonitor::OnPageContentExtracted(
    content::Page& page,
    page_content_annotations::PageContent page_content) {
  // PCES fetches page content for multiple tabs. Check that this is the
  // right page before notifying.
  if (!IsMonitoredPage(page)) {
    return;
  }
  // PDF text is not supported as page context.
  if (!page_content_annotations::IsAnnotatedPageContentPtr(page_content)) {
    return;
  }

  pending_cached_notification_.Cancel();
  // PCES caches the eligibility of non-PDF content before notifying observers,
  // so the synchronous lookup reflects `page_content`.
  NotifyPageContextFetched(BuildPageContextResult(
      page_content_annotations::GetAnnotatedPageContentPtrFromPageContent(
          std::move(page_content)),
      extraction_service_->GetServerUploadEligibilityForPage(page).value_or(
          false)));
}

bool TtcPageContextMonitor::IsMonitoredPage(const content::Page& page) const {
  return web_contents_ && &page == &web_contents_->GetPrimaryPage();
}

void TtcPageContextMonitor::NotifyPageContextFetched(PageContextResult result) {
  on_page_context_fetched_.Run(result);
}

}  // namespace ttc
