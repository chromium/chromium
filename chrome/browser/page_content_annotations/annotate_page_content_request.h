// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_

#include "chrome/browser/content_extraction/inner_text.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace page_content_annotations {

// Class for deciding when a page is ready for getting page content, and
// extracts page content.
class AnnotatedPageContentRequest {
 public:
  static std::unique_ptr<AnnotatedPageContentRequest> MaybeCreate(
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

 private:
  void ResetForNewNavigation();

  void RequestContentIfReady();

  void RequestAnnotatedPageContentSync();

  bool Ready() const;

  void OnPageContentReceived(
      std::optional<optimization_guide::proto::AnnotatedPageContent> proto);

  void OnInnerTextReceived(
      base::TimeTicks start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result);

#if BUILDFLAG(ENABLE_PDF)
  void RequestPdfPageCount();

  // Invoked when pdf document is loaded, so that the metadata can be queried.
  void OnPdfDocumentLoadComplete();
#endif  // BUILDFLAG(ENABLE_PDF)

  const raw_ptr<content::WebContents> web_contents_;
  const blink::mojom::AIPageContentOptionsPtr request_;
  const base::TimeDelta delay_;
  const bool include_inner_text_;

  // Set if a new page was committed and querying it's content is pending.
  bool page_content_pending_ = false;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
