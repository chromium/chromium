// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {
class PageContextEligibility;
}  // namespace optimization_guide

namespace page_content_annotations {

// Class for deciding when a page is ready for getting page content, and
// extracts page content.
class AnnotatedPageContentRequest {
 public:
  using FetchPageContextCallback =
      base::RepeatingCallback<void(content::WebContents&,
                                   const FetchPageContextOptions&,
                                   std::unique_ptr<FetchPageProgressListener>,
                                   FetchPageContextResultCallback)>;

  static std::unique_ptr<AnnotatedPageContentRequest> Create(
      content::WebContents* web_contents);

  AnnotatedPageContentRequest(content::WebContents* web_contents,
                              blink::mojom::AIPageContentOptionsPtr request);

  AnnotatedPageContentRequest(const AnnotatedPageContentRequest&) = delete;
  AnnotatedPageContentRequest& operator=(const AnnotatedPageContentRequest&) =
      delete;
  ~AnnotatedPageContentRequest();

  void PrimaryPageChanged();

  void DidFinishNavigation(content::NavigationHandle* navigation_handle);

  void DidStopLoading();

  void OnFirstContentfulPaintInPrimaryMainFrame();

  void OnVisibilityChanged(content::Visibility visibility);

  // Returns the cached APC for `page` and whether it is eligible for
  // server upload. Will return nullopt if not available.
  std::optional<ExtractedPageContentResult> GetCachedContentAndEligibility();

  void SetFetchPageContextCallbackForTesting(FetchPageContextCallback callback);

 private:
  void ResetForNewNavigation();

  void MaybeScheduleExtraction();

  void ExtractPageContent();
  void RequestAnnotatedPageContentSync();

  bool ShouldScheduleExtraction() const;

  void OnPageContextFetched(FetchPageContextResultCallbackArg result);

  void OnInnerTextReceived(
      base::TimeTicks start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result);

#if BUILDFLAG(ENABLE_PDF)
  void RequestPdfPageCount();

  // Invoked when pdf document is loaded, so that the metadata can be queried.
  void OnPdfDocumentLoadComplete();
#endif  // BUILDFLAG(ENABLE_PDF)

  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;
  const raw_ptr<content::WebContents> web_contents_;
  const blink::mojom::AIPageContentOptionsPtr request_;
  const base::TimeDelta delay_;
  const bool include_inner_text_;

  enum class Lifecycle {
    // Indicates that a new navigation occurred and we need to schedule an
    // extraction. This is async because we need to wait for the page to be
    // ready.
    kPending,

    // The extraction has been scheduled and we are waiting on a response from
    // the renderer. The IPC to request the content maybe delayed so the page
    // has reached a stable state.
    kScheduled,

    // The extraction finished after page load.
    kExtractedAtPageLoad,

    // All extraction triggers are handled.
    kFinal
  };
  Lifecycle lifecycle_ = Lifecycle::kFinal;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;
  bool is_hidden_ = false;

  std::optional<ExtractedPageContentResult> cached_content_;

  FetchPageContextCallback fetch_page_context_callback_;

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
