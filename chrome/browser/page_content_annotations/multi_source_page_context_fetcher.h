// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_

#include <cstdint>
#include <vector>

#include "base/types/expected.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "ui/gfx/geometry/size.h"

namespace page_content_annotations {

struct FetchPageContextOptions {
  FetchPageContextOptions();
  ~FetchPageContextOptions();

  // Limit defining the number of bytes for inner text returned. A value
  // of 0 indicates no inner text should be returned.
  uint32_t inner_text_bytes_limit = 0;
  bool include_viewport_screenshot = false;
  blink::mojom::AIPageContentOptionsPtr annotated_page_content_options;

  // Limit defining number of bytes for PDF data that should be returned.
  // A value of 0 indicates no pdf data should be returned.
  uint32_t pdf_size_limit = 0;
};

struct PdfResult {
  explicit PdfResult(url::Origin origin);
  PdfResult(url::Origin origin, std::vector<uint8_t> bytes);
  ~PdfResult();
  url::Origin origin;
  std::vector<uint8_t> bytes;
  bool size_exceeded = false;
};

struct ScreenshotResult {
  explicit ScreenshotResult(gfx::Size dimensions);
  ~ScreenshotResult();
  std::vector<uint8_t> jpeg_data;
  gfx::Size dimensions;
};

struct InnerTextResultWithTruncation
    : public content_extraction::InnerTextResult {
  InnerTextResultWithTruncation(std::string inner_text,
                                std::optional<unsigned> node_offset,
                                bool truncated);
  ~InnerTextResultWithTruncation();
  bool truncated = false;
};

struct FetchPageContextResult {
  FetchPageContextResult();
  ~FetchPageContextResult();
  std::optional<ScreenshotResult> screenshot_result;
  std::optional<InnerTextResultWithTruncation> inner_text_result;
  std::optional<PdfResult> pdf_result;
  std::optional<optimization_guide::AIPageContentResult>
      annotated_page_content_result;
};

using FetchPageContextResultCallbackArg =
    base::expected<std::unique_ptr<FetchPageContextResult>, std::string>;
using FetchPageContextResultCallback =
    base::OnceCallback<void(FetchPageContextResultCallbackArg)>;
void FetchPageContext(content::WebContents& web_contents,
                      const FetchPageContextOptions& options,
                      FetchPageContextResultCallback callback);

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_
