// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_
#define CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/core/page_context.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace ttc {

// Monitors the primary page of the given WebContents via
// PageContentExtractionService (PCES), invoking its callbacks when the page
// changes in ways that invalidate the page context, and when fresh page
// context is extracted for it.
//
// Registering as a PCES observer enables automatic page content extraction for
// every tab in the profile (not just the monitored one) for as long as this
// object exists.
// TODO(b/555804152): Monitor for additional non-navigation, in-page changes.
class TtcPageContextMonitor
    : public page_content_annotations::PageContentExtractionService::Observer {
 public:
  // Invoked whenever the page shown in the monitored tab changes in a way
  // that invalidates previously obtained page context.
  using PageContextInvalidatedCallback = base::RepeatingClosure;

  // Invoked when fresh page context is extracted for the monitored page.
  using PageContextFetchedCallback =
      base::RepeatingCallback<void(const PageContextResult&)>;

  TtcPageContextMonitor(
      content::WebContents& web_contents,
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

 private:
  // Requests page context for the monitored page on construction. If PCES
  // already has content cached for the page, `on_page_context_fetched_` is
  // invoked asynchronously; otherwise an extraction is triggered and
  // `on_page_context_fetched_` is invoked once it completes. No-op if the
  // page's URL is not supported (see `ttc::IsUrlSupportedForPageContext`);
  void FetchPageContext();

  // Returns whether `page` is the primary page of the monitored WebContents.
  bool IsMonitoredPage(const content::Page& page) const;

  void NotifyPageContextFetched(PageContextResult result);

  // Weak because the monitored WebContents may be destroyed before the owner
  // of this object learns about it and destroys this object.
  const base::WeakPtr<content::WebContents> web_contents_;

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
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_PAGE_CONTEXT_MONITOR_H_
