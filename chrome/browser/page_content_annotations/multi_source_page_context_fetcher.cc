// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/common/constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "pdf/mojom/pdf.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/origin.h"

namespace page_content_annotations {

namespace {

// Controls scaling and quality of tab screenshots.
BASE_FEATURE(kGlicTabScreenshotExperiment,
             "GlicTabScreenshotExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxScreenshotWidthParam{
    &kGlicTabScreenshotExperiment, "max_screenshot_width", 1024};

const base::FeatureParam<int> kMaxScreenshotHeightParam{
    &kGlicTabScreenshotExperiment, "max_screenshot_height", 1024};

const base::FeatureParam<int> kScreenshotJpegQuality{
    &kGlicTabScreenshotExperiment, "screenshot_jpeg_quality", 40};

const base::FeatureParam<base::TimeDelta> kScreenshotTimeout{
    &kGlicTabScreenshotExperiment, "screenshot_timeout_ms", base::Seconds(1)};

gfx::Size GetScreenshotSize(content::RenderWidgetHostView* view) {
  // By default, no scaling.
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return gfx::Size();
  }

  // If either width or height is 0, or the view is empty, no scaling.
  gfx::Size original_size = view->GetViewBounds().size();
  int max_width = kMaxScreenshotWidthParam.Get();
  int max_height = kMaxScreenshotHeightParam.Get();
  if (max_width == 0 || max_height == 0 || original_size.IsEmpty()) {
    return gfx::Size();
  }

  double aspect_ratio = static_cast<double>(original_size.width()) /
                        static_cast<double>(original_size.height());

  int new_width = original_size.width();
  int new_height = original_size.height();

  // If larger than width or height, scale down while preserving aspect
  // ratio.
  if (new_width > max_width) {
    new_width = max_width;
    new_height = static_cast<int>(max_width / aspect_ratio);
  }
  if (new_height > max_height) {
    new_height = max_height;
    new_width = static_cast<int>(max_height * aspect_ratio);
  }

  return gfx::Size(new_width, new_height);
}

int GetScreenshotJpegQuality() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return 100;
  }
  // Must be an int from 0 to 100.
  return std::max(0, std::min(100, kScreenshotJpegQuality.Get()));
}

// Combination of tracked states for when a PDF contents request is made.
// Must be kept in sync with PdfRequestStates in
// src/tools/metrics/histograms/metadata/glic/enums.xml.
enum class PdfRequestStates {
  kPdfMainDoc_PdfFound = 0,
  kPdfMainDoc_PdfNotFound = 1,
  kNonPdfMainDoc_PdfFound = 2,
  kNonPdfMainDoc_PdfNotFound = 3,
  kMaxValue = kNonPdfMainDoc_PdfNotFound,
};

void RecordPdfRequestState(bool is_pdf_document, bool pdf_found) {
  PdfRequestStates state;
  if (is_pdf_document) {
    state = pdf_found ? PdfRequestStates::kPdfMainDoc_PdfFound
                      : PdfRequestStates::kPdfMainDoc_PdfNotFound;
  } else {
    state = pdf_found ? PdfRequestStates::kNonPdfMainDoc_PdfFound
                      : PdfRequestStates::kNonPdfMainDoc_PdfNotFound;
  }
  UMA_HISTOGRAM_ENUMERATION("Glic.TabContext.PdfContentsRequested", state);
}

// Coordinates fetching multiple types of page context.
class PageContextFetcher : public content::WebContentsObserver {
 public:
  PageContextFetcher() = default;
  ~PageContextFetcher() override = default;

  void FetchStart(content::WebContents& aweb_contents,
                  const FetchPageContextOptions& options,
                  FetchPageContextResultCallback callback) {
    pending_result_ = std::make_unique<FetchPageContextResult>();
    DCHECK(aweb_contents.GetPrimaryMainFrame());
    CHECK_EQ(web_contents(),
             nullptr);  // Ensure Fetch is called only once per instance.
    Observe(&aweb_contents);
    // TODO(crbug.com/391851902): implement kSensitiveContentAttribute error
    // checking and signaling.
    callback_ = std::move(callback);

    if (options.include_viewport_screenshot) {
      GetTabScreenshot(*web_contents());
    } else {
      screenshot_done_ = true;
    }

    inner_text_bytes_limit_ = options.inner_text_bytes_limit;
    if (options.inner_text_bytes_limit > 0) {
      content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
      // This could be more efficient if GetInnerText
      // supported a max length. Instead, we truncate after generating the full
      // text.
      GetInnerText(
          *frame,
          /*node_id=*/std::nullopt,
          base::BindOnce(&PageContextFetcher::ReceivedInnerText, GetWeakPtr()));
    } else {
      inner_text_done_ = true;
    }

    pdf_done_ = true;  // Will not fetch PDF contents by default.
    if (options.pdf_size_limit > 0) {
      bool is_pdf_document =
          web_contents()->GetContentsMimeType() == pdf::kPDFMimeType;
      pdf::PDFDocumentHelper* pdf_helper =
          pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
      RecordPdfRequestState(is_pdf_document,
                            /*pdf_found=*/pdf_helper != nullptr);
      // GetPdfBytes() is not safe before IsDocumentLoadComplete() = true.
      if (is_pdf_document && pdf_helper &&
          pdf_helper->IsDocumentLoadComplete()) {
        const url::Origin& pdf_origin =
            pdf_helper->render_frame_host().GetLastCommittedOrigin();
        pdf_helper->GetPdfBytes(
            options.pdf_size_limit,
            base::BindOnce(&PageContextFetcher::ReceivedPdfBytes, GetWeakPtr(),
                           pdf_origin, options.pdf_size_limit));
        pdf_done_ = false;  // Will fetch PDF contents.
      }
    }

    if (options.annotated_page_content_options) {
      blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
          options.annotated_page_content_options.Clone();
      ai_page_content_options->on_critical_path = true;
      optimization_guide::GetAIPageContent(
          web_contents(), std::move(ai_page_content_options),
          base::BindOnce(&PageContextFetcher::ReceivedAnnotatedPageContent,
                         GetWeakPtr()));
    } else {
      annotated_page_content_done_ = true;
    }

    // Note: initialization_done_ guards against processing
    // `RunCallbackIfComplete()` until we reach this point.
    initialization_done_ = true;
    RunCallbackIfComplete();
  }

  void ReceivedPdfBytes(const url::Origin& pdf_origin,
                        uint32_t pdf_size_limit,
                        pdf::mojom::PdfListener::GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes,
                        uint32_t page_count) {
    pdf_done_ = true;

    // Warning!: `pdf_bytes_` can be larger than pdf_size_limit.
    // `pdf_size_limit` applies to the original PDF size, but the PDF is
    // re-serialized and returned, so it is not identical to the original.
    bool size_limit_exceeded =
        status ==
            pdf::mojom::PdfListener_GetPdfBytesStatus::kSizeLimitExceeded ||
        pdf_bytes.size() > pdf_size_limit;

    if (size_limit_exceeded) {
      pending_result_->pdf_result.emplace(pdf_origin);
    } else {
      pending_result_->pdf_result.emplace(pdf_origin, pdf_bytes);
    }
    RunCallbackIfComplete();
  }

  void GetTabScreenshot(content::WebContents& web_contents) {
    auto* view = web_contents.GetRenderWidgetHostView();
    auto finish_error_callback =
        base::BindOnce(&PageContextFetcher::RecievedJpegScreenshot,
                       GetWeakPtr(), std::nullopt);

    if (!view || !view->IsSurfaceAvailableForCopy()) {
      DLOG(WARNING) << "Could not retrieve RenderWidgetHostView.";
      std::move(finish_error_callback).Run();
      return;
    }

    capture_count_lock_ = web_contents.IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/false,
        /*is_activity=*/false);

    // Fetching the screenshot sometimes hangs. Quit early if it's taking too
    // long. b/431837630.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(finish_error_callback), kScreenshotTimeout.Get());

    view->CopyFromSurface(
        gfx::Rect(),  // Copy entire surface area.
        GetScreenshotSize(view),
        base::BindOnce(&PageContextFetcher::ReceivedViewportBitmap,
                       GetWeakPtr()));
  }

  void ReceivedViewportBitmap(const SkBitmap& bitmap) {
    // Early exit if the timeout has fired.
    if (screenshot_done_) {
      return;
    }
    pending_result_->screenshot_result.emplace(
        gfx::SkISizeToSize(bitmap.dimensions()));
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetScreenshot",
                            elapsed_timer_.Elapsed());
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            [](const SkBitmap& bitmap) {
              return gfx::JPEGCodec::Encode(bitmap, GetScreenshotJpegQuality());
            },
            bitmap),
        base::BindOnce(&PageContextFetcher::RecievedJpegScreenshot,
                       GetWeakPtr()));
  }

  // content::WebContentsObserver impl.
  void PrimaryPageChanged(content::Page& page) override {
    primary_page_changed_ = true;
    RunCallbackIfComplete();
  }

  void RecievedJpegScreenshot(
      std::optional<std::vector<uint8_t>> screenshot_jpeg_data) {
    // This function can be called multiple times, for timeout behavior. Early
    // exit if it's already been called.
    if (screenshot_done_) {
      return;
    }
    auto elapsed = elapsed_timer_.Elapsed();
    screenshot_done_ = true;
    capture_count_lock_ = {};
    if (screenshot_jpeg_data) {
      pending_result_->screenshot_result.value().jpeg_data =
          std::move(*screenshot_jpeg_data);
      base::UmaHistogramTimes("Glic.PageContextFetcher.GetEncodedScreenshot",
                              elapsed);
    } else {
      base::UmaHistogramTimes(
          "Glic.PageContextFetcher.GetEncodedScreenshot.Failure", elapsed);
    }
    RunCallbackIfComplete();
  }

  void ReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result) {
    // Get trimmed text without copying.
    std::string trimmed_text = std::move(result->inner_text);
    size_t truncated_size =
        base::TruncateUTF8ToByteSize(trimmed_text, inner_text_bytes_limit_)
            .length();
    bool truncated = false;
    if (truncated_size < trimmed_text.length()) {
      truncated = true;
      trimmed_text.resize(truncated_size);
    }

    pending_result_->inner_text_result.emplace(
        std::move(trimmed_text), std::move(result->node_offset), truncated);
    inner_text_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetInnerText",
                            elapsed_timer_.Elapsed());
    RunCallbackIfComplete();
  }

  void ReceivedAnnotatedPageContent(
      std::optional<optimization_guide::AIPageContentResult> content) {
    pending_result_->annotated_page_content_result = std::move(content);
    annotated_page_content_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetAnnotatedPageContent",
                            elapsed_timer_.Elapsed());
    RunCallbackIfComplete();
  }

  void RunCallbackIfComplete() {
    if (!initialization_done_) {
      return;
    }

    // Continue only if the primary page changed or work is complete.
    bool work_complete = (screenshot_done_ && inner_text_done_ &&
                          annotated_page_content_done_ && pdf_done_) ||
                         primary_page_changed_;
    if (!work_complete) {
      return;
    }
    base::UmaHistogramTimes("Glic.PageContextFetcher.Total",
                            elapsed_timer_.Elapsed());

    if (primary_page_changed_ || !web_contents() ||
        !web_contents()->GetPrimaryMainFrame()) {
      std::move(callback_).Run(
          base::unexpected<std::string>("web contents changed"));
      return;
    }

    std::move(callback_).Run(base::ok(std::move(pending_result_)));
  }

 private:
  base::WeakPtr<PageContextFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  FetchPageContextResultCallback callback_;

  uint32_t inner_text_bytes_limit_ = 0;

  // Intermediate results:

  // Whether work is complete for each task, does not imply success.
  bool initialization_done_ = false;
  bool screenshot_done_ = false;
  bool inner_text_done_ = false;
  bool pdf_done_ = false;
  bool annotated_page_content_done_ = false;
  // Whether the primary page has changed since context fetching began.
  bool primary_page_changed_ = false;
  std::unique_ptr<FetchPageContextResult> pending_result_;
  base::ElapsedTimer elapsed_timer_;
  base::ScopedClosureRunner capture_count_lock_;

  base::WeakPtrFactory<PageContextFetcher> weak_ptr_factory_{this};
};

}  // namespace

FetchPageContextOptions::FetchPageContextOptions() = default;

FetchPageContextOptions::~FetchPageContextOptions() = default;

FetchPageContextResult::FetchPageContextResult() = default;

FetchPageContextResult::~FetchPageContextResult() = default;

PdfResult::PdfResult(url::Origin origin, std::vector<uint8_t> bytes)
    : origin(std::move(origin)), bytes(std::move(bytes)) {}

PdfResult::PdfResult(url::Origin origin)
    : origin(std::move(origin)), size_exceeded(true) {}

PdfResult::~PdfResult() = default;

ScreenshotResult::ScreenshotResult(gfx::Size dimensions)
    : dimensions(std::move(dimensions)) {}

ScreenshotResult::~ScreenshotResult() = default;

InnerTextResultWithTruncation::InnerTextResultWithTruncation(
    std::string inner_text,
    std::optional<unsigned> node_offset,
    bool truncated)
    : InnerTextResult(std::move(inner_text), node_offset),
      truncated(truncated) {}

InnerTextResultWithTruncation::~InnerTextResultWithTruncation() = default;

void FetchPageContext(content::WebContents& web_contents,
                      const FetchPageContextOptions& options,
                      FetchPageContextResultCallback callback) {
  CHECK(callback);
  auto self = std::make_unique<PageContextFetcher>();
  auto* raw_self = self.get();
  raw_self->FetchStart(web_contents, options,
                       base::BindOnce(
                           // Bind `fetcher` to the callback to keep it in scope
                           // until it returns.
                           [](std::unique_ptr<PageContextFetcher> fetcher,
                              FetchPageContextResultCallback callback,
                              FetchPageContextResultCallbackArg result) {
                             std::move(callback).Run(std::move(result));
                           },
                           std::move(self), std::move(callback)));
}

}  // namespace page_content_annotations
