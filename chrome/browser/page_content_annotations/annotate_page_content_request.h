// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_

#include "base/scoped_observation.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace page_content_annotations {

// Class for deciding when a page is ready for getting page content, and
// extracts page content.
class AnnotatedPageContentRequest
#if BUILDFLAG(ENABLE_PDF)
    : public pdf::PDFDocumentHelper::Observer
#endif  // BUILDFLAG(ENABLE_PDF)
{
 public:
  static std::unique_ptr<AnnotatedPageContentRequest> MaybeCreate(
      content::WebContents* web_contents);

  AnnotatedPageContentRequest(content::WebContents* web_contents,
                              blink::mojom::AIPageContentOptionsPtr request);

  AnnotatedPageContentRequest(const AnnotatedPageContentRequest&) = delete;
  AnnotatedPageContentRequest& operator=(const AnnotatedPageContentRequest&) =
      delete;
#if BUILDFLAG(ENABLE_PDF)
  ~AnnotatedPageContentRequest() override;
#else
  ~AnnotatedPageContentRequest();
#endif  // BUILDFLAG(ENABLE_PDF)

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

  // pdf::PDFDocumentHelper::Observer:
  void OnDocumentLoadComplete() override;
#endif  // BUILDFLAG(ENABLE_PDF)

  const raw_ptr<content::WebContents> web_contents_;
  const blink::mojom::AIPageContentOptionsPtr request_;
  const base::TimeDelta delay_;
  const bool include_inner_text_;

  // Set if a new page was committed and querying it's content is pending.
  bool page_content_pending_ = false;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;

#if BUILDFLAG(ENABLE_PDF)
  base::ScopedObservation<pdf::PDFDocumentHelper,
                          pdf::PDFDocumentHelper::Observer>
      pdf_load_obseration_{this};
#endif  // BUILDFLAG(ENABLE_PDF)

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
