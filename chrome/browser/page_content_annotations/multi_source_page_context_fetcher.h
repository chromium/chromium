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
#include "base/types/optional_ref.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "ui/gfx/geometry/size.h"

namespace page_content_annotations {

enum class ScreenshotIframeRedactionScope {
  // No redaction.
  kNone,
  // Redact cross-site iframes.
  kCrossSite,
  // Redact cross-origin iframes.
  kCrossOrigin,
};

struct PaintPreviewOptions {
  // The maximum memory/file bytes used for the capture of a single frame.
  // 0 means no limit.
  size_t max_per_capture_bytes = 0;

  // Whether iframe redaction is enabled, and which scope is used if so.
  ScreenshotIframeRedactionScope iframe_redaction_scope =
      ScreenshotIframeRedactionScope::kNone;
};

class ScreenshotOptions {
 public:
  // Creates options for a full-page screenshot.
  // Full-page screenshots always use the paint preview backend.
  static ScreenshotOptions FullPage(PaintPreviewOptions paint_preview_options) {
    return ScreenshotOptions(/*capture_full_page=*/true, paint_preview_options);
  }

  // Creates options for a viewport-only screenshot.
  static ScreenshotOptions ViewportOnly(
      std::optional<PaintPreviewOptions> paint_preview_options) {
    return ScreenshotOptions(/*capture_full_page=*/false,
                             std::move(paint_preview_options));
  }

  bool capture_full_page() const { return capture_full_page_; }
  bool use_paint_preview() const { return paint_preview_options_.has_value(); }
  base::optional_ref<const PaintPreviewOptions> paint_preview_options() const
      LIFETIME_BOUND {
    return paint_preview_options_;
  }

 private:
  // Private constructor to force object creation through static methods.
  ScreenshotOptions(bool capture_full_page,
                    std::optional<PaintPreviewOptions> paint_preview_options)
      : capture_full_page_(capture_full_page),
        paint_preview_options_(paint_preview_options) {}

  // Whether to capture a full-page screenshot. If false, only the viewport will
  // be captured.
  bool capture_full_page_ = false;
  // This field must be set if capture_full_page_ is true.
  std::optional<PaintPreviewOptions> paint_preview_options_ = std::nullopt;
};

struct FetchPageContextOptions {
  FetchPageContextOptions();
  ~FetchPageContextOptions();

  // Limit defining the number of bytes for inner text returned. A value
  // of 0 indicates no inner text should be returned.
  uint32_t inner_text_bytes_limit = 0;

  // Options for taking a screenshot. If not set, no screenshot will be taken.
  std::optional<ScreenshotOptions> screenshot_options = std::nullopt;

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
  std::vector<uint8_t> screenshot_data;
  std::string mime_type;
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
  base::expected<PageContentResultWithEndTime, std::string>
      annotated_page_content_result;
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

extern const base::FeatureParam<int> kScreenshotQuality;

extern const base::FeatureParam<std::string> kScreenshotImageType;

extern const base::FeatureParam<base::TimeDelta> kScreenshotTimeout;

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
