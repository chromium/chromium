// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_

#include <cstdint>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
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
  base::TimeTicks end_time;
};

struct InnerTextResultWithTruncation
    : public content_extraction::InnerTextResult {
  InnerTextResultWithTruncation(std::string inner_text,
                                std::optional<unsigned> node_offset,
                                bool truncated);
  ~InnerTextResultWithTruncation();
  bool truncated = false;
};

struct PageContentResultWithEndTime
    : public optimization_guide::AIPageContentResult {
  explicit PageContentResultWithEndTime(
      optimization_guide::AIPageContentResult&& result);
  base::TimeTicks end_time;
};

struct FetchPageContextResult {
  FetchPageContextResult();
  ~FetchPageContextResult();
  base::expected<ScreenshotResult, std::string> screenshot_result;
  std::optional<InnerTextResultWithTruncation> inner_text_result;
  std::optional<PdfResult> pdf_result;
  std::optional<PageContentResultWithEndTime> annotated_page_content_result;
};

enum class FetchPageContextError {
  kUnknown,
  kWebContentsChanged,
  // The context is not eligible for sharing.
  kPageContextNotEligible,
};

std::string ToString(FetchPageContextError error);

// TODO(bokan): message is redundant with error_code. Replace usage with
// ToString.
struct FetchPageContextErrorDetails {
  FetchPageContextError error_code = FetchPageContextError::kUnknown;
  std::string message;
};
using FetchPageContextResultCallbackArg =
    base::expected<std::unique_ptr<FetchPageContextResult>,
                   FetchPageContextErrorDetails>;

// Controls scaling and quality of tab screenshots.
BASE_DECLARE_FEATURE(kGlicTabScreenshotExperiment);

extern const base::FeatureParam<int> kMaxScreenshotWidthParam;

extern const base::FeatureParam<int> kMaxScreenshotHeightParam;

extern const base::FeatureParam<int> kScreenshotJpegQuality;

extern const base::FeatureParam<base::TimeDelta> kScreenshotTimeout;

// Enables the Paint Preview backend for taking screenshots.
BASE_DECLARE_FEATURE(kGlicTabScreenshotPaintPreviewBackend);

enum class ScreenshotIframeRedactionScope {
  // No redaction.
  kNone,
  // Redact cross-site iframes.
  kCrossSite,
  // Redact cross-origin iframes.
  kCrossOrigin,
};

// Controls whether iframe redaction is enabled, and which scope is used if so.
extern const base::FeatureParam<ScreenshotIframeRedactionScope>
    kScreenshotIframeRedaction;

// Controls the maximum memory/file bytes used for the capture of a single
// frame. 0 means no maximum.
extern const base::FeatureParam<size_t> kScreenshotMaxPerCaptureBytes;

// Enables page context eligibility checks.
BASE_DECLARE_FEATURE(kGlicPageContextEligibility);

// Callback used for relaying progress.
class FetchPageProgressListener {
 public:
  virtual ~FetchPageProgressListener() = default;
  virtual void BeginScreenshot() {}
  virtual void EndScreenshot(std::optional<std::string> error) {}
  virtual void BeginAPC() {}
  virtual void EndAPC(std::optional<std::string> error) {}
};

using FetchPageContextResultCallback =
    base::OnceCallback<void(FetchPageContextResultCallbackArg)>;
void FetchPageContext(
    content::WebContents& web_contents,
    const FetchPageContextOptions& options,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    FetchPageContextResultCallback callback);

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_MULTI_SOURCE_PAGE_CONTEXT_FETCHER_H_
