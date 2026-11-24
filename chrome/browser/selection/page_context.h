// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_PAGE_CONTEXT_H_
#define CHROME_BROWSER_SELECTION_PAGE_CONTEXT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/selection/area_of_interest.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace selection {

// Page-level snapshot captured for Smart Selection, shared across one or more
// `AreaOfInterest` selections on the same page.
struct PageContext {
  struct EncodedScreenshot {
    EncodedScreenshot();
    EncodedScreenshot(const EncodedScreenshot&);
    EncodedScreenshot& operator=(const EncodedScreenshot&);
    EncodedScreenshot(EncodedScreenshot&&);
    EncodedScreenshot& operator=(EncodedScreenshot&&);
    ~EncodedScreenshot();

    friend bool operator==(const EncodedScreenshot&,
                           const EncodedScreenshot&) = default;

    std::vector<uint8_t> data;
    std::string mime_type;
    gfx::Size size;
  };

  struct PdfDocumentData {
    PdfDocumentData();
    PdfDocumentData(const PdfDocumentData&);
    PdfDocumentData& operator=(const PdfDocumentData&);
    PdfDocumentData(PdfDocumentData&&);
    PdfDocumentData& operator=(PdfDocumentData&&);
    ~PdfDocumentData();

    friend bool operator==(const PdfDocumentData&,
                           const PdfDocumentData&) = default;

    url::Origin origin;
    // Raw PDF bytes; nullopt when `size_limit_exceeded` is true.
    std::optional<std::vector<uint8_t>> bytes;
    bool size_limit_exceeded = false;
  };

  PageContext();
  PageContext(const PageContext&);
  PageContext& operator=(const PageContext&);
  PageContext(PageContext&&);
  PageContext& operator=(PageContext&&);
  ~PageContext();

  optimization_guide::proto::AnnotatedPageContent apc;
  blink::mojom::PageMetadataPtr metadata;
  SkBitmap screenshot;
  // Optional pre-encoded version of `screenshot` (e.g. redacted JPEG produced
  // during page context capture) to avoid re-encoding on prompt submission.
  std::optional<EncodedScreenshot> encoded_screenshot;
  // Optional PDF document snapshot if the active page is or contains a PDF.
  std::optional<PdfDocumentData> pdf_data;
};

// Pairs a page-level snapshot with a selection on that page.
struct PageContextWithSelection {
  PageContextWithSelection();
  PageContextWithSelection(PageContext page_context,
                           AreaOfInterest area_of_interest);
  PageContextWithSelection(const PageContextWithSelection&);
  PageContextWithSelection& operator=(const PageContextWithSelection&);
  PageContextWithSelection(PageContextWithSelection&&);
  PageContextWithSelection& operator=(PageContextWithSelection&&);
  ~PageContextWithSelection();

  PageContext page_context;
  AreaOfInterest area_of_interest;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_PAGE_CONTEXT_H_
