// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_
#define CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/core/page_context.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "content/public/browser/web_contents_observer.h"

namespace actor {
class EnterprisePolicyChecker;
}

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace ttc {

class SessionJournal;

// Monitors the primary page of the given WebContents via
// PageContentExtractionService (PCES), invoking its callbacks when the page
// changes in ways that invalidate the page context, and when fresh page
// context is extracted for it.
//
// Registering as a PCES observer enables automatic page content extraction for
// every tab in the profile (not just the monitored one) for as long as this
// object exists.
//
// Extracted page context is never emitted for a page whose URL is not
// supported (see IsUrlSupportedForPageContext()). Instead, a
// kPageContextNotEligible error is emitted.
// TODO(b/555804152): Monitor for additional non-navigation, in-page changes.
class TtcPageContextMonitor
    : public page_content_annotations::PageContentExtractionService::Observer,
      public content::WebContentsObserver {
 public:
  // Invoked whenever the page shown in the monitored tab changes in a way
  // that invalidates previously obtained page context.
  using PageContextInvalidatedCallback = base::RepeatingClosure;

  // Invoked when fresh page context is extracted for the monitored page, or
  // with an error when page context can't be provided for it, e.g. because its
  // renderer crashed or its URL isn't supported.
  using PageContextFetchedCallback =
      base::RepeatingCallback<void(const PageContextResult&)>;

  TtcPageContextMonitor(
      content::WebContents& web_contents,
      SessionJournal& journal,
      const actor::EnterprisePolicyChecker* enterprise_policy_checker,
      PageContextInvalidatedCallback on_page_context_invalidated,
      PageContextFetchedCallback on_page_context_fetched);
  ~TtcPageContextMonitor() override;

  TtcPageContextMonitor(const TtcPageContextMonitor&) = delete;
  TtcPageContextMonitor& operator=(const TtcPageContextMonitor&) = delete;

  // page_content_annotations::PageContentExtractionService::Observer:
  void OnPageContentReset(content::Page& current_page,
                          bool is_same_document) override;
  void OnPageContentExtracted(
      content::Page& page,
      page_content_annotations::PageContent page_content) override;

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  // Requests page context for the monitored page on construction. If PCES
  // already has content cached for the page, `on_page_context_fetched_` is
  // invoked asynchronously (if the page's URL is supported); otherwise an
  // extraction is triggered and `on_page_context_fetched_` is invoked once it
  // completes.
  void FetchPageContext();

  // Returns whether `page` is the primary page of the monitored WebContents.
  bool IsMonitoredPage(const content::Page& page) const;

  // Checks whether `page` has a supported URL and, if so and `page` is still
  // the monitored page, invokes `on_page_context_fetched_` with `result`.
  void MaybeNotifyPageContextFetched(base::WeakPtr<content::Page> page,
                                     PageContextResult result);
  void NotifyPageContextFetched(base::WeakPtr<content::Page> page,
                                PageContextResult result,
                                bool is_url_supported);

  const raw_ref<SessionJournal> journal_;

  const raw_ptr<const actor::EnterprisePolicyChecker>
      enterprise_policy_checker_;

  const raw_ref<page_content_annotations::PageContentExtractionService>
      extraction_service_;
  base::ScopedObservation<
      page_content_annotations::PageContentExtractionService,
      page_content_annotations::PageContentExtractionService::Observer>
      extraction_service_observation_{this};

  PageContextInvalidatedCallback on_page_context_invalidated_;
  PageContextFetchedCallback on_page_context_fetched_;

  // Holds the posted task for delivering already-cached page content on
  // construction. Cancelled if a navigation or fresh extraction supersedes it.
  base::CancelableOnceClosure pending_cached_notification_;

  base::WeakPtrFactory<TtcPageContextMonitor> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_
