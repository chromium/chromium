// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PDF_CONTENT_ANALYSIS_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PDF_CONTENT_ANALYSIS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {
struct FetchPageContextResult;
}  // namespace page_content_annotations

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

using PdfContentAnalysisCallback = base::OnceCallback<void(
    std::unique_ptr<page_content_annotations::FetchPageContextResult>)>;

// Scans the PDF bytes in `page_context`, if any, with the FILE_ATTACHED
// enterprise connector, then runs `callback` with `page_context`. The PDF is
// removed from `page_context` unless the scan allowed it, including when the
// scan is abandoned because `tab` closed or left `primary_page`.
//
// Runs `callback` synchronously if no scan is needed: there are no PDF bytes,
// the connector is not enabled for Glic, or the platform does not support it.
void MaybeScanPdfForFileAttached(
    base::WeakPtr<tabs::TabInterface> tab,
    base::WeakPtr<content::Page> primary_page,
    std::unique_ptr<page_content_annotations::FetchPageContextResult>
        page_context,
    PdfContentAnalysisCallback callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PDF_CONTENT_ANALYSIS_H_
