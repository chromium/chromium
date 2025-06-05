// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_

#include "chrome/browser/content_extraction/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace page_content_annotations {

// Class for deciding when a page is ready for getting page content, and
// extracts page content.
class AnnotatedPageContentRequest {
 public:
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

 private:
  void ResetForNewNavigation();

  void MaybeScheduleExtraction();

  void RequestAnnotatedPageContentSync();

  bool ShouldScheduleExtraction() const;

  void OnPageContentReceived(
      std::optional<optimization_guide::AIPageContentResult> page_content);

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

  enum class Lifecycle {
    // Indicates that a new navigation occurred and we need to schedule an
    // extraction. This is async because we need to wait for the page to be
    // ready.
    kPending,

    // The extraction has been scheduled and we are waiting on a response from
    // the renderer. The IPC to request the content maybe delayed so the page
    // has reached a stable state.
    kScheduled,

    // The content for the last committed navigation has been extracted.
    kDone
  };
  Lifecycle lifecycle_ = Lifecycle::kDone;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_PAGE_CONTENT_ANNOTATIONS_ANNOTATE_PAGE_CONTENT_REQUEST_H_
