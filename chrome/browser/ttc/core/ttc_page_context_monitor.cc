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
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/page_context_util.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/pdf/common/constants.h"
#include "components/ttc/app/public/page_context_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "url/gurl.h"

namespace ttc {

namespace {

// Builds the result for APC that PCES has extracted, gated on its eligibility
// for server upload. Shares `page_content` rather than copying it.
PageContextResult BuildPageContextResult(
    page_content_annotations::RefCountedAnnotatedPageContentPtr page_content,
    bool is_eligible_for_server_upload) {
  if (!page_content) {
    return base::unexpected(PageContextError::FetchFailed());
  }
  if (!is_eligible_for_server_upload) {
    return base::unexpected(
        PageContextError::Restricted(PageRestrictionReason::kSafetyPolicy));
  }
  return PageContext(std::move(page_content));
}

blink::mojom::AIPageContentOptionsPtr CreateActionableOptions() {
  auto options = optimization_guide::ActionableAIPageContentOptions(
      page_content_annotations::features::
          IsAnnotatedPageContentOnCriticalPath());
  if (page_content_annotations::features::
          ShouldAnnotatedPageContentExcludeAdRelated()) {
    options->non_salient_content_config =
        blink::mojom::NonSalientContentConfig::New();
    options->non_salient_content_config->exclude_ad_related = true;
  }
  return options;
}

bool IsPdf(const content::Page& page) {
  return page.GetContentsMimeType() == pdf::kPDFMimeType;
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
  if (!web_contents()) {
    return;
  }

  pending_cached_notification_.Cancel();
  fetch_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  content::Page& page = web_contents()->GetPrimaryPage();
  if (MaybePostImmediatePageContext(page)) {
    return;
  }

  const GURL& url = page.GetMainDocument().GetLastCommittedURL();
  if (kTtcPageContentMode.Get() == TtcPageContentMode::kActionable) {
    optimization_guide::GetAIPageContent(
        web_contents(), CreateActionableOptions(),
        base::BindOnce(
            &TtcPageContextMonitor::OnActionablePageContentFetched,
            fetch_weak_ptr_factory_.GetWeakPtr(),
            GetJournal().BeginAsyncEvent("TtcPageContextRefresh",
                                         actor::JournalDetailsBuilder()
                                             .Add("url", url)
                                             .Add("kind", "actionable")
                                             .Build()),
            page.GetWeakPtr()));
    return;
  }
  // As in FetchPageContext(), PCES notifies OnPageContentExtracted() when this
  // refresh extracts new APC, which forwards it to `on_page_context_fetched_`.
  // `OnFetchCompleted` only tracks the in-flight request and closes the async
  // journal event.
  extraction_service_->RefreshExtractedPageContentAndEligibilityForPage(
      page,
      base::BindOnce(&TtcPageContextMonitor::OnFetchCompleted,
                     fetch_weak_ptr_factory_.GetWeakPtr(),
                     GetJournal().BeginAsyncEvent("TtcPageContextRefresh",
                                                  actor::JournalDetailsBuilder()
                                                      .Add("url", url)
                                                      .Add("kind", "default")
                                                      .Build())));
}

void TtcPageContextMonitor::FetchPageContext() {
  CHECK(web_contents());

  content::Page& page = web_contents()->GetPrimaryPage();
  if (MaybePostImmediatePageContext(page)) {
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
    PostPageContextFetched(
        page, BuildPageContextResult(std::move(extracted->page_content),
                                     extracted->is_eligible_for_server_upload));
  } else {
    const GURL& url = page.GetMainDocument().GetLastCommittedURL();
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

  // Deliberately not gated on `GetPageRestrictionForUrl()`, so that
  // observers learn that context for the previous page no longer applies.
  pending_cached_notification_.Cancel();
  fetch_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  on_page_context_invalidated_.Run();

  MaybePostImmediatePageContext(current_page);
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
  // Synchronously restricted pages are already reported in `FetchPageContext()`
  // and `OnPageContentReset()`.
  if (GetPageRestrictionForUrlSync(page.GetMainDocument().GetLastCommittedURL(),
                                   enterprise_policy_checker_)
          .has_value()) {
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
  // until it's reloaded. Report it as restricted rather than checking whether
  // its URL is supported: the crashed main document no longer has one.
  pending_cached_notification_.Cancel();
  fetch_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  NotifyPageContextFetched(web_contents()->GetPrimaryPage().GetWeakPtr(),
                           base::unexpected(PageContextError::Restricted(
                               PageRestrictionReason::kRendererUnavailable)));
}

void TtcPageContextMonitor::OnSpeakerChanged(Speaker speaker) {
  if (speaker == Speaker::kUser) {
    RefreshPageContext();
  }
}

bool TtcPageContextMonitor::IsMonitoredPage(const content::Page& page) const {
  return web_contents() && &page == &web_contents()->GetPrimaryPage();
}

bool TtcPageContextMonitor::MaybePostImmediatePageContext(content::Page& page) {
  if (web_contents()->IsCrashed()) {
    PostPageContextFetched(page,
                           base::unexpected(PageContextError::Restricted(
                               PageRestrictionReason::kRendererUnavailable)));
    return true;
  }

  const GURL& url = page.GetMainDocument().GetLastCommittedURL();
  if (std::optional<PageRestrictionReason> restriction =
          GetPageRestrictionForUrlSync(url, enterprise_policy_checker_)) {
    PostPageContextFetched(
        page, base::unexpected(PageContextError::Restricted(*restriction)));
    return true;
  }

  // `OnPageContentExtracted()` ignores PDF text, and we do not extract APC for
  // a PDF. Report it like a fetch failure so tab metadata can still be shared
  // if the URL is allowed.
  if (IsPdf(page)) {
    GetJournal().Log("TtcPageContextPdf",
                     actor::JournalDetailsBuilder().Add("url", url).Build());
    PostPageContextFetched(page,
                           base::unexpected(PageContextError::FetchFailed()));
    return true;
  }

  return false;
}

void TtcPageContextMonitor::PostPageContextFetched(content::Page& page,
                                                   PageContextResult result) {
  fetch_weak_ptr_factory_.InvalidateWeakPtrs();
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Unretained is safe: `this` owns `pending_cached_notification_`, which
  // cancels the callback on destruction.
  pending_cached_notification_.Reset(base::BindOnce(
      &TtcPageContextMonitor::MaybeNotifyPageContextFetched,
      base::Unretained(this), page.GetWeakPtr(), std::move(result)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, pending_cached_notification_.callback());
}

void TtcPageContextMonitor::MaybeNotifyPageContextFetched(
    base::WeakPtr<content::Page> weak_page,
    PageContextResult result) {
  if (!weak_page || !IsMonitoredPage(*weak_page)) {
    return;
  }
  if (!result.has_value() && result.error().restriction.has_value()) {
    NotifyPageContextFetched(std::move(weak_page), std::move(result));
    return;
  }
  const GURL& url = weak_page->GetMainDocument().GetLastCommittedURL();
  GetPageRestrictionForUrl(
      url, enterprise_policy_checker_,
      base::BindOnce(
          [](PageContextResult result,
             std::optional<PageRestrictionReason> url_restriction)
              -> PageContextResult {
            if (url_restriction.has_value()) {
              return base::unexpected(
                  PageContextError::Restricted(*url_restriction));
            }
            return result;
          },
          std::move(result))
          .Then(base::BindOnce(&TtcPageContextMonitor::NotifyPageContextFetched,
                               weak_ptr_factory_.GetWeakPtr(), weak_page)));
}

void TtcPageContextMonitor::NotifyPageContextFetched(
    base::WeakPtr<content::Page> weak_page,
    PageContextResult result) {
  // The URL restriction check may complete asynchronously, by which time the
  // monitored page may have changed.
  if (!weak_page || !IsMonitoredPage(*weak_page)) {
    return;
  }
  const GURL& url = weak_page->GetMainDocument().GetLastCommittedURL();
  if (!result.has_value() && result.error().restriction.has_value()) {
    GetJournal().Log("TtcPageContextBlocked",
                     actor::JournalDetailsBuilder()
                         .Add("url", url)
                         .Add("reason", *result.error().restriction)
                         .Build());
    on_page_context_fetched_.Run(*weak_page, result);
    return;
  }
  GetJournal().Log("TtcPageContextUpdated",
                   actor::JournalDetailsBuilder().Add("url", url).Build());
  on_page_context_fetched_.Run(*weak_page, result);
}

void TtcPageContextMonitor::OnFetchCompleted(
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_event,
    std::optional<page_content_annotations::ExtractedPageContentResult>
        result) {
  journal_event->EndEntry(actor::JournalDetailsBuilder()
                              .Add("success", result.has_value())
                              .Build());
}

void TtcPageContextMonitor::OnActionablePageContentFetched(
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_event,
    base::WeakPtr<content::Page> weak_page,
    optimization_guide::AIPageContentResultOrError actionable_result) {
  journal_event->EndEntry(actor::JournalDetailsBuilder()
                              .Add("success", actionable_result.has_value())
                              .Build());
  if (!weak_page || !IsMonitoredPage(*weak_page)) {
    return;
  }
  if (!actionable_result.has_value() &&
      !web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    return;
  }

  pending_cached_notification_.Cancel();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!actionable_result.has_value()) {
    MaybeNotifyPageContextFetched(
        weak_page, base::unexpected(PageContextError::FetchFailed()));
    return;
  }

  MaybeNotifyPageContextFetched(
      weak_page,
      PageContext(base::MakeRefCounted<
                  page_content_annotations::RefCountedAnnotatedPageContent>(
          std::move(actionable_result->proto))));
}

}  // namespace ttc
