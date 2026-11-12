// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/page_context_util.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

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
    SessionControllerImpl& session_controller,
    const actor::EnterprisePolicyChecker* enterprise_policy_checker,
    PageContextInvalidatedCallback on_page_context_invalidated,
    PageContextFetchedCallback on_page_context_fetched)
    : content::WebContentsObserver(&web_contents),
      session_controller_(session_controller),
      enterprise_policy_checker_(enterprise_policy_checker),
      extraction_service_(CHECK_DEREF(
          page_content_annotations::PageContentExtractionServiceFactory::
              GetForProfile(Profile::FromBrowserContext(
                  web_contents.GetBrowserContext())))),
      on_page_context_invalidated_(std::move(on_page_context_invalidated)),
      on_page_context_fetched_(std::move(on_page_context_fetched)) {
  CHECK(on_page_context_invalidated_);
  CHECK(on_page_context_fetched_);
  session_controller_observation_.Observe(&session_controller);
  extraction_service_observation_.Observe(&extraction_service_.get());

  // Extract or retrieve cached context for the currently displayed page,
  // since PCES may have fired a completed page fetch before this object is
  // created.
  FetchPageContext();
}

TtcPageContextMonitor::~TtcPageContextMonitor() = default;

bool TtcPageContextMonitor::IsFetchingForTesting() const {
  return fetch_weak_ptr_factory_.HasWeakPtrs();
}

SessionJournalImpl& TtcPageContextMonitor::GetJournal() {
  return session_controller_->GetJournal();
}

void TtcPageContextMonitor::RefreshPageContext() {
  if (!web_contents() || web_contents()->IsCrashed()) {
    return;
  }

  content::Page& page = web_contents()->GetPrimaryPage();
  const GURL& url = page.GetMainDocument().GetLastCommittedURL();

  if (enterprise_policy_checker_ &&
      enterprise_policy_checker_->Evaluate(url) ==
          actor::EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked) {
    GetJournal().Log("TtcPageContextEnterpriseBlocked",
                     actor::JournalDetailsBuilder().Add("url", url).Build());
    return;
  }

  pending_cached_notification_.Cancel();
  // As in FetchPageContext(), PCES notifies OnPageContentExtracted() when this
  // refresh extracts new APC, which forwards it to `on_page_context_fetched_`.
  // `OnFetchCompleted` only tracks the in-flight request and closes the async
  // journal event.
  extraction_service_->RefreshExtractedPageContentAndEligibilityForPage(
      page, base::BindOnce(
                &TtcPageContextMonitor::OnFetchCompleted,
                fetch_weak_ptr_factory_.GetWeakPtr(),
                GetJournal().BeginAsyncEvent(
                    "TtcPageContextRefresh",
                    actor::JournalDetailsBuilder().Add("url", url).Build())));
}

void TtcPageContextMonitor::FetchPageContext() {
  CHECK(web_contents());

  content::Page& page = web_contents()->GetPrimaryPage();
  const GURL& url = page.GetMainDocument().GetLastCommittedURL();

  // If enterprise policy explicitly blocks the URL, suppress context extraction
  // immediately: do not trigger async extraction, and do not notify cached
  // context.
  if (enterprise_policy_checker_ &&
      enterprise_policy_checker_->Evaluate(url) ==
          actor::EnterprisePolicyChecker::UrlBlockReason::kExplicitlyBlocked) {
    GetJournal().Log("TtcPageContextEnterpriseBlocked",
                     actor::JournalDetailsBuilder().Add("url", url).Build());
    return;
  }
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
        &TtcPageContextMonitor::MaybeNotifyPageContextFetched,
        base::Unretained(this), page.GetWeakPtr(),
        BuildPageContextResult(std::move(extracted->page_content),
                               extracted->is_eligible_for_server_upload)));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, pending_cached_notification_.callback());
  } else {
    // PCES will call its own fetch method, triggering OnPageContentExtracted,
    // when this async method actually extracts new APC.
    extraction_service_->GetExtractedPageContentAndEligibilityForPageAsync(
        page,
        base::BindOnce(
            &TtcPageContextMonitor::OnFetchCompleted,
            fetch_weak_ptr_factory_.GetWeakPtr(),
            GetJournal().BeginAsyncEvent(
                "TtcPageContextFetch",
                actor::JournalDetailsBuilder().Add("url", url).Build())),
        /*trigger_if_not_cached=*/true);
  }
}

void TtcPageContextMonitor::OnPageContentReset(content::Page& current_page,
                                               bool is_same_document) {
  if (!IsMonitoredPage(current_page)) {
    return;
  }

  // Deliberately not gated on IsUrlSupportedForPageContext(), so that
  // observers learn that context for the previous page no longer applies.
  pending_cached_notification_.Cancel();
  weak_ptr_factory_.InvalidateWeakPtrs();
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
  weak_ptr_factory_.InvalidateWeakPtrs();
  // PCES caches the eligibility of non-PDF content before notifying observers,
  // so the synchronous lookup reflects `page_content`.
  MaybeNotifyPageContextFetched(
      page.GetWeakPtr(),
      BuildPageContextResult(
          page_content_annotations::GetAnnotatedPageContentPtrFromPageContent(
              std::move(page_content)),
          extraction_service_->GetServerUploadEligibilityForPage(page).value_or(
              false)));
}

void TtcPageContextMonitor::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // The page's content is gone, and PCES won't report anything for the page
  // until it's reloaded. Report it as not eligible, which only shares its
  // origin, rather than checking whether its URL is supported: the crashed
  // main document no longer has one.
  GetJournal().Log("TtcPageContextRendererGone", {});
  pending_cached_notification_.Cancel();
  fetch_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  on_page_context_fetched_.Run(
      base::unexpected(page_content_annotations::FetchPageContextError::
                           kPageContextNotEligible));
}

void TtcPageContextMonitor::OnSpeakerChanged(Speaker speaker) {
  if (speaker == Speaker::kUser) {
    RefreshPageContext();
  }
}

bool TtcPageContextMonitor::IsMonitoredPage(const content::Page& page) const {
  return web_contents() && &page == &web_contents()->GetPrimaryPage();
}

void TtcPageContextMonitor::MaybeNotifyPageContextFetched(
    base::WeakPtr<content::Page> weak_page,
    PageContextResult result) {
  if (!weak_page || !IsMonitoredPage(*weak_page)) {
    return;
  }
  IsUrlSupportedForPageContext(
      weak_page->GetMainDocument().GetLastCommittedURL(),
      enterprise_policy_checker_,
      base::BindOnce(&TtcPageContextMonitor::NotifyPageContextFetched,
                     weak_ptr_factory_.GetWeakPtr(), weak_page,
                     std::move(result)));
}

void TtcPageContextMonitor::NotifyPageContextFetched(
    base::WeakPtr<content::Page> weak_page,
    PageContextResult result,
    bool is_url_supported) {
  // The URL support check may complete asynchronously, by which time the
  // monitored page may have changed.
  if (!weak_page || !IsMonitoredPage(*weak_page)) {
    return;
  }
  const GURL& url = weak_page->GetMainDocument().GetLastCommittedURL();
  if (!is_url_supported) {
    GetJournal().Log("TtcPageContextBlocked",
                     actor::JournalDetailsBuilder().Add("url", url).Build());
    // TODO(b/555804152): Figure out how to send specific error messages to
    // the server.
    on_page_context_fetched_.Run(
        base::unexpected(page_content_annotations::FetchPageContextError::
                             kPageContextNotEligible));
    return;
  }
  GetJournal().Log("TtcPageContextUpdated",
                   actor::JournalDetailsBuilder().Add("url", url).Build());
  on_page_context_fetched_.Run(result);
}

void TtcPageContextMonitor::OnFetchCompleted(
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_event,
    std::optional<page_content_annotations::ExtractedPageContentResult>
        result) {
  journal_event->EndEntry(actor::JournalDetailsBuilder()
                              .Add("success", result.has_value())
                              .Build());
}

}  // namespace ttc
