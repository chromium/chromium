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
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/common/redaction_params.h"
#include "components/pdf/common/constants.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif

namespace page_content_annotations {

namespace {

template <typename T, typename E>
// Conditionally emits to a given timing histogram, given the start_time.
base::expected<T, E> EmitTimingHistogram(const std::string& histogram_name,
                                         base::ElapsedTimer timer,
                                         base::expected<T, E> result) {
  if (result.has_value()) {
    base::UmaHistogramTimes(histogram_name, timer.Elapsed());
  }
  return std::move(result);
}

gfx::Size GetScreenshotSize(const gfx::Size& original_size) {
  // By default, no scaling.
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return gfx::Size();
  }

  // If either width or height is 0, or the view is empty, no scaling.
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

double GetScreenshotScaleFactor(const gfx::Size& original_size,
                                const gfx::Size& new_size) {
  if (new_size.IsEmpty()) {
    // When the new size is empty, that means no scaling.
    return 1.0;
  }
  // The aspect ratio was preserved by GetScreenshotSize, so the ratio of the
  // new width to old width should be the same as the ratio of new height to old
  // height. WLOG, we'll use the widths.
  return new_size.width() / original_size.width();
}

int GetScreenshotJpegQuality() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return 40;
  }
  // Must be an int from 0 to 100.
  return std::max(0, std::min(100, kScreenshotQuality.Get()));
}

int GetScreenshotWebPQuality() {
  return GetScreenshotJpegQuality();
}

// Png only has two modes exposed, so we use the quality to determine if it is
// low quality or not by checking if it is 50 or lower.
bool ShouldPngScreenshotBeLowQuality() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return false;
  }
  return kScreenshotQuality.Get() < 50;
}

enum class ScreenshotImageType {
  kUnknown = 0,
  kJpeg = 1,
  kPng = 2,
  kWebp = 3,
  kMaxValue = kWebp,
};

ScreenshotImageType GetScreenshotImageType() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return ScreenshotImageType::kJpeg;
  }
  if (kScreenshotImageType.Get() == "jpeg") {
    return ScreenshotImageType::kJpeg;
  }
  if (kScreenshotImageType.Get() == "png") {
    return ScreenshotImageType::kPng;
  }
  if (kScreenshotImageType.Get() == "webp") {
    return ScreenshotImageType::kWebp;
  }
  return ScreenshotImageType::kJpeg;
}

base::expected<paint_preview::RedactionParams, std::string> GetRedactionParams(
    content::WebContents& web_contents,
    ScreenshotIframeRedactionScope screenshot_iframe_redaction_scope) {
  auto* frame = web_contents.GetPrimaryMainFrame();
  if (!frame) {
    return base::unexpected("Could not get primary main frame.");
  }

  switch (screenshot_iframe_redaction_scope) {
    case ScreenshotIframeRedactionScope::kNone:
      return paint_preview::RedactionParams();
    case ScreenshotIframeRedactionScope::kCrossSite:
      return paint_preview::RedactionParams(
          /*allowed_origins=*/{},
          /*allowed_sites=*/{
              net::SchemefulSite(frame->GetLastCommittedOrigin())});
    case ScreenshotIframeRedactionScope::kCrossOrigin:
      return paint_preview::RedactionParams(
          /*allowed_origins=*/{frame->GetLastCommittedOrigin()},
          /*allowed_sites=*/{});
  }
  NOTREACHED();
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

#if !BUILDFLAG(IS_ANDROID)
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
#endif

// Coordinates fetching multiple types of page context.
class PageContextFetcher : public content::WebContentsObserver {
 public:
  explicit PageContextFetcher(
      std::unique_ptr<FetchPageProgressListener> progress_listener)
      : progress_listener_(std::move(progress_listener)) {}
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

    if (options.screenshot_options) {
      GetTabScreenshot(*web_contents(), options.screenshot_options.value());
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
#if !BUILDFLAG(IS_ANDROID)
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
#endif

    if (options.annotated_page_content_options) {
      blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
          options.annotated_page_content_options.Clone();
      ai_page_content_options->on_critical_path = true;
      if (progress_listener_) {
        progress_listener_->BeginAPC();
      }
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

  // TODO: Enable pdf fetching for Android.
#if !BUILDFLAG(IS_ANDROID)
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
#endif

  void GetTabScreenshot(content::WebContents& web_contents,
                        const ScreenshotOptions& screenshot_options) {
    auto* view = web_contents.GetRenderWidgetHostView();
    if (progress_listener_) {
      progress_listener_->BeginScreenshot();
    }

    if (!view || !view->IsSurfaceAvailableForCopy()) {
      ReceivedEncodedScreenshot(
          base::unexpected("Could not retrieve RenderWidgetHostView."));
      return;
    }

    gfx::Size view_size = view->GetViewBounds().size();

    if (screenshot_options.use_paint_preview()) {
      PageContentScreenshotService* service =
          PageContentScreenshotServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents.GetBrowserContext()));
      if (!service) {
        ReceivedEncodedScreenshot(
            base::unexpected("Could not get PageContentScreenshotService."));
        return;
      }

      ASSIGN_OR_RETURN(
          paint_preview::RedactionParams redaction_params,
          GetRedactionParams(web_contents,
                             screenshot_options.paint_preview_options()
                                 ->iframe_redaction_scope),
          [&](std::string error) {
            ReceivedEncodedScreenshot(base::unexpected(std::move(error)));
            return;
          });

      SetCaptureCountLock(web_contents);
      ScheduleScreenshotTimeout();

      gfx::Rect clip_rect = gfx::Rect(view_size);
      paint_preview::mojom::ClipCoordOverride clip_coord_override =
          paint_preview::mojom::ClipCoordOverride::kScrollOffset;

      if (screenshot_options.capture_full_page()) {
        clip_rect = gfx::Rect();
        clip_coord_override = paint_preview::mojom::ClipCoordOverride::kNone;
        view_size = web_contents.GetPrimaryMainFrame()->GetFrameSize().value_or(
            gfx::Size());
      }
      PageContentScreenshotService::RequestParams request_params = {
          .clip_rect = clip_rect,
          .scale_factor =
              GetScreenshotScaleFactor(view_size, GetScreenshotSize(view_size)),
          .clip_x_coord_override = clip_coord_override,
          .clip_y_coord_override = clip_coord_override,
          .redaction_params = std::move(redaction_params),
          .max_per_capture_bytes =
              screenshot_options.paint_preview_options()->max_per_capture_bytes,
      };
      service->RequestScreenshot(
          &web_contents, std::move(request_params),
          base::BindOnce(
              EmitTimingHistogram<const SkBitmap*, std::string>,
              "Glic.PageContextFetcher.GetScreenshot.TimeoutAgnostic",
              elapsed_timer_)
              .Then(base::BindOnce(
                  &PageContextFetcher::ReceivedViewportBitmapOrError,
                  GetWeakPtr())));
    } else {
      SetCaptureCountLock(web_contents);
      ScheduleScreenshotTimeout();

      view->CopyFromSurface(
          gfx::Rect(),  // Copy entire surface area.
          GetScreenshotSize(view_size),
          base::BindOnce(&PageContextFetcher::ReceivedViewportBitmap,
                         GetWeakPtr()));
    }
  }

  void SetCaptureCountLock(content::WebContents& web_contents) {
    capture_count_lock_ = web_contents.IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/false,
        /*is_activity=*/false);
  }

  void ScheduleScreenshotTimeout() {
    // Fetching the screenshot sometimes hangs. Quit early if it's taking too
    // long. b/431837630.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PageContextFetcher::ReceivedEncodedScreenshot,
                       GetWeakPtr(), base::unexpected("ScreenshotTimeout")),
        kScreenshotTimeout.Get());
  }

  void ReceivedViewportBitmap(const viz::CopyOutputBitmapWithMetadata& result) {
    ReceivedViewportBitmapOrError(&result.bitmap);
  }

  void ReceivedViewportBitmapOrError(
      base::expected<const SkBitmap*, std::string> bitmap_result) {
    // Early exit if the timeout has fired.
    if (screenshot_done_) {
      return;
    }
    if (bitmap_result.has_value()) {
      const SkBitmap* bitmap = bitmap_result.value();
      pending_result_->screenshot_result.emplace(
          gfx::SkISizeToSize(bitmap->dimensions()));
      base::UmaHistogramTimes("Glic.PageContextFetcher.GetScreenshot",
                              elapsed_timer_.Elapsed());
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(
              [](const SkBitmap& bitmap) {
                std::optional<std::vector<uint8_t>> encoded;
                switch (GetScreenshotImageType()) {
                  case ScreenshotImageType::kJpeg:
                    encoded = gfx::JPEGCodec::Encode(
                        bitmap, GetScreenshotJpegQuality());
                    break;
                  case ScreenshotImageType::kPng:
                    if (ShouldPngScreenshotBeLowQuality()) {
                      encoded = gfx::PNGCodec::FastEncodeBGRASkBitmap(
                          bitmap, /*discard_transparency=*/true);
                    } else {
                      encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
                          bitmap, /*discard_transparency=*/true);
                    }
                    break;
                  case ScreenshotImageType::kWebp:
                    encoded = gfx::WebpCodec::Encode(
                        bitmap, GetScreenshotWebPQuality());
                    break;
                  default:
                    break;
                }
                base::expected<std::vector<uint8_t>, std::string> reply;
                if (encoded) {
                  reply.emplace(std::move(encoded.value()));
                } else {
                  reply = base::unexpected("JPEGCodec failed to encode");
                }
                return reply;
              },
              *bitmap),
          base::BindOnce(
              EmitTimingHistogram<std::vector<uint8_t>, std::string>,
              "Glic.PageContextFetcher.GetEncodedScreenshot.TimeoutAgnostic",
              elapsed_timer_)
              .Then(
                  base::BindOnce(&PageContextFetcher::ReceivedEncodedScreenshot,
                                 GetWeakPtr())));
    } else {
      ReceivedEncodedScreenshot(base::unexpected(bitmap_result.error()));
    }
  }

  // content::WebContentsObserver impl.
  void PrimaryPageChanged(content::Page& page) override {
    primary_page_changed_ = true;
    RunCallbackIfComplete();
  }

  void ReceivedEncodedScreenshot(
      base::expected<std::vector<uint8_t>, std::string> screenshot_data) {
    // This function can be called multiple times, for timeout behavior. Early
    // exit if it's already been called.
    if (screenshot_done_) {
      return;
    }
    auto elapsed = elapsed_timer_.Elapsed();
    screenshot_done_ = true;
    capture_count_lock_ = {};
    if (screenshot_data.has_value()) {
      pending_result_->screenshot_result.value().screenshot_data =
          std::move(screenshot_data.value());
      switch (GetScreenshotImageType()) {
        case ScreenshotImageType::kJpeg:
          pending_result_->screenshot_result.value().mime_type = "image/jpeg";
          break;
        case ScreenshotImageType::kPng:
          pending_result_->screenshot_result.value().mime_type = "image/png";
          break;
        case ScreenshotImageType::kWebp:
          pending_result_->screenshot_result.value().mime_type = "image/webp";
          break;
        default:
          NOTREACHED();
      }
      base::UmaHistogramTimes("Glic.PageContextFetcher.GetEncodedScreenshot",
                              elapsed);
      if (progress_listener_) {
        progress_listener_->EndScreenshot(std::nullopt);
      }
    } else {
      pending_result_->screenshot_result =
          base::unexpected(screenshot_data.error());
      base::UmaHistogramTimes(
          "Glic.PageContextFetcher.GetEncodedScreenshot.Failure", elapsed);
      if (progress_listener_) {
        progress_listener_->EndScreenshot(screenshot_data.error());
      }
    }
    if (pending_result_->screenshot_result.has_value()) {
      pending_result_->screenshot_result.value().end_time =
          base::TimeTicks::Now();
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
      optimization_guide::AIPageContentResultOrError content) {
    const bool has_result = content.has_value();
    if (has_result) {
      pending_result_->annotated_page_content_result.emplace(
          std::move(content.value()));
    } else {
      pending_result_->annotated_page_content_result =
          base::unexpected(content.error());
    }
    annotated_page_content_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetAnnotatedPageContent",
                            elapsed_timer_.Elapsed());
    if (progress_listener_) {
      if (has_result) {
        progress_listener_->EndAPC(std::nullopt);
      } else {
        progress_listener_->EndAPC(
            absl::StrFormat("Failed: %s", content.error()));
      }
    }

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
      std::move(callback_).Run(base::unexpected(FetchPageContextErrorDetails{
          FetchPageContextError::kWebContentsChanged, "web contents changed"}));
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

  std::unique_ptr<FetchPageProgressListener> progress_listener_;

  base::WeakPtrFactory<PageContextFetcher> weak_ptr_factory_{this};
};

}  // namespace

std::string ToString(FetchPageContextError error) {
  switch (error) {
    case FetchPageContextError::kUnknown:
      return "kUnknown";
    case FetchPageContextError::kWebContentsChanged:
      return "kWebContentsChanged";
    case FetchPageContextError::kPageContextNotEligible:
      return "kPageContextNotEligible";
  }
}

BASE_FEATURE(kGlicTabScreenshotExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxScreenshotWidthParam{
    &kGlicTabScreenshotExperiment, "max_screenshot_width", 0};

const base::FeatureParam<int> kMaxScreenshotHeightParam{
    &kGlicTabScreenshotExperiment, "max_screenshot_height", 0};

const base::FeatureParam<int> kScreenshotQuality{&kGlicTabScreenshotExperiment,
                                                 "screenshot_quality", 40};

const base::FeatureParam<std::string> kScreenshotImageType{
    &kGlicTabScreenshotExperiment, "screenshot_image_type", "jpeg"};

const base::FeatureParam<base::TimeDelta> kScreenshotTimeout{
    &kGlicTabScreenshotExperiment, "screenshot_timeout_ms", base::Seconds(5)};

FetchPageContextOptions::FetchPageContextOptions() = default;

FetchPageContextOptions::~FetchPageContextOptions() = default;

FetchPageContextResult::FetchPageContextResult()
    : screenshot_result(base::unexpected("Uninitialized")),
      annotated_page_content_result(base::unexpected("Uninitialized")) {}

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

PageContentResultWithEndTime::PageContentResultWithEndTime(
    optimization_guide::AIPageContentResult&& result)
    : optimization_guide::AIPageContentResult(std::move(result)),
      end_time(base::TimeTicks::Now()) {}

void FetchPageContext(
    content::WebContents& web_contents,
    const FetchPageContextOptions& options,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    FetchPageContextResultCallback callback) {
  CHECK(callback);
  auto self =
      std::make_unique<PageContextFetcher>(std::move(progress_listener));
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
