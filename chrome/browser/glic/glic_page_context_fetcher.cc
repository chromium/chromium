// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_page_context_fetcher.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_tab_data.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/common/constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "pdf/mojom/pdf.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace glic {

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

// Checks for no focusable tabs or invalid candidate URLs. Returns nullopt if
// the tab is valid for context extraction. Otherwise, returns an error reason
// specifying why it is not valid.
std::optional<mojom::GetTabContextErrorReason>
IsFocusedTabValidForContextExtraction(FocusedTabData focused_tab_data) {
  std::optional<mojom::NoCandidateTabError> no_candidate_tab_error =
      focused_tab_data.no_candidate_tab_error;
  if (no_candidate_tab_error.has_value()) {
    switch (no_candidate_tab_error.value()) {
      case mojom::NoCandidateTabError::kUnknown:
        return mojom::GetTabContextErrorReason::kUnknown;
      case mojom::NoCandidateTabError::kNoFocusableTabs:
        return mojom::GetTabContextErrorReason::kNoFocusableTabs;
    }
  }
  const std::optional<FocusedTabCandidate>& focused_tab_candidate =
      focused_tab_data.focused_tab_candidate;
  if (focused_tab_candidate.has_value()) {
    glic::mojom::InvalidCandidateError invalid_candidate_error =
        focused_tab_candidate.value().invalid_candidate_error;
    switch (invalid_candidate_error) {
      case mojom::InvalidCandidateError::kUnknown:
        return mojom::GetTabContextErrorReason::kUnknown;
      case mojom::InvalidCandidateError::kUnsupportedUrl:
        return mojom::GetTabContextErrorReason::kUnsupportedUrl;
    }
  }

  if (!focused_tab_data.focused_tab_contents) {
    return mojom::GetTabContextErrorReason::kNoFocusableTabs;
  }
  return std::nullopt;
}

}  // namespace

GlicPageContextFetcher::GlicPageContextFetcher() = default;

GlicPageContextFetcher::~GlicPageContextFetcher() = default;

void GlicPageContextFetcher::Fetch(
    FocusedTabData focused_tab_data,
    const mojom::GetTabContextOptions& options,
    glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  if (std::optional<mojom::GetTabContextErrorReason> error_reason =
          IsFocusedTabValidForContextExtraction(focused_tab_data)) {
    std::move(callback).Run(
        mojom::GetContextResult::NewErrorReason(*error_reason));
    return;
  }
  options_ = options;

  content::WebContents* aweb_contents =
      focused_tab_data.focused_tab_contents.get();
  DCHECK(aweb_contents->GetPrimaryMainFrame());
  CHECK_EQ(web_contents(),
           nullptr);  // Ensure Fetch is called only once per instance.
  Observe(aweb_contents);
  // TODO(crbug.com/391851902): implement kSensitiveContentAttribute error
  // checking and signaling.
  start_time_ = base::TimeTicks::Now();
  callback_ = std::move(callback);

  if (options.include_viewport_screenshot) {
    GetTabScreenshot(*web_contents());
  } else {
    screenshot_done_ = true;
  }

  if (options.include_inner_text) {
    content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
    // This could be more efficient if content_extraction::GetInnerText
    // supported a max length. Instead, we truncate after generating the full
    // text.
    content_extraction::GetInnerText(
        *frame,
        /*node_id=*/std::nullopt,
        base::BindOnce(&GlicPageContextFetcher::ReceivedInnerText,
                       GetWeakPtr()));
  } else {
    inner_text_done_ = true;
  }

  pdf_done_ = true;  // Will not fetch PDF contents by default.
  if (options.include_pdf) {
    bool is_pdf_document =
        web_contents()->GetContentsMimeType() == pdf::kPDFMimeType;
    pdf::PDFDocumentHelper* pdf_helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
    RecordPdfRequestState(is_pdf_document, /*pdf_found=*/pdf_helper != nullptr);
    if (is_pdf_document && pdf_helper) {
      pdf_origin_ = pdf_helper->render_frame_host().GetLastCommittedOrigin();
      pdf_helper->GetPdfBytes(
          options.pdf_size_limit,
          base::BindOnce(&GlicPageContextFetcher::ReceivedPdfBytes,
                         GetWeakPtr()));
      pdf_done_ = false;  // Will fetch PDF contents.
    }
  }

  if (options.include_annotated_page_content) {
    blink::mojom::AIPageContentOptionsPtr ai_page_content_options;
    ai_page_content_options = optimization_guide::DefaultAIPageContentOptions();
    ai_page_content_options->include_geometry = false;
    ai_page_content_options->on_critical_path = true;
    ai_page_content_options->include_hidden_searchable_content = true;
    optimization_guide::GetAIPageContent(
        web_contents(), std::move(ai_page_content_options),
        base::BindOnce(&GlicPageContextFetcher::ReceivedAnnotatedPageContent,
                       GetWeakPtr()));
  } else {
    annotated_page_content_done_ = true;
  }

  RunCallbackIfComplete();
}

void GlicPageContextFetcher::ReceivedPdfBytes(
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& pdf_bytes,
    uint32_t page_count) {
  pdf_done_ = true;
  pdf_status_ = status;
  pdf_bytes_ = pdf_bytes;
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::GetTabScreenshot(
    content::WebContents& web_contents) {
  // TODO(crbug.com/378937313): Finish this provisional implementation.
  auto* view = web_contents.GetRenderWidgetHostView();
  auto callback = base::BindOnce(
      &GlicPageContextFetcher::RecievedJpegScreenshot, GetWeakPtr());

  if (!view || !view->IsSurfaceAvailableForCopy()) {
    std::move(callback).Run({});
    DLOG(WARNING) << "Could not retrieve RenderWidgetHostView.";
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(),  // Copy entire surface area.
      GetScreenshotSize(view),
      base::BindOnce(&GlicPageContextFetcher::ReceivedViewportBitmap,
                     GetWeakPtr()));
}

void GlicPageContextFetcher::ReceivedViewportBitmap(const SkBitmap& bitmap) {
  screenshot_dimensions_ = bitmap.dimensions();
  base::UmaHistogramTimes("Glic.PageContextFetcher.GetScreenshot",
                          base::TimeTicks::Now() - start_time_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const SkBitmap& bitmap) {
            return gfx::JPEGCodec::Encode(bitmap, GetScreenshotJpegQuality());
          },
          std::move(bitmap)),
      base::BindOnce(&GlicPageContextFetcher::RecievedJpegScreenshot,
                     GetWeakPtr()));
}

void GlicPageContextFetcher::PrimaryPageChanged(content::Page& page) {
  primary_page_changed_ = true;
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::RecievedJpegScreenshot(
    std::optional<std::vector<uint8_t>> screenshot_jpeg_data) {
  if (screenshot_jpeg_data) {
    screenshot_ = glic::mojom::Screenshot::New(
        screenshot_dimensions_.width(), screenshot_dimensions_.height(),
        std::move(*screenshot_jpeg_data), "image/jpeg",
        // TODO(crbug.com/380495633): Finalize and implement image annotations.
        glic::mojom::ImageOriginAnnotations::New());
  }
  screenshot_done_ = true;
  base::UmaHistogramTimes("Glic.PageContextFetcher.GetEncodedScreenshot",
                          base::TimeTicks::Now() - start_time_);
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::ReceivedInnerText(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  inner_text_result_ = std::move(result);
  inner_text_done_ = true;
  base::UmaHistogramTimes("Glic.PageContextFetcher.GetInnerText",
                          base::TimeTicks::Now() - start_time_);
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::ReceivedAnnotatedPageContent(
    std::optional<optimization_guide::proto::AnnotatedPageContent> content) {
  annotated_page_content_ = std::move(content);
  annotated_page_content_done_ = true;
  base::UmaHistogramTimes("Glic.PageContextFetcher.GetAnnotatedPageContent",
                          base::TimeTicks::Now() - start_time_);
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::RunCallbackIfComplete() {
  // Continue only if the primary page changed or work is complete.
  bool work_complete = (screenshot_done_ && inner_text_done_ &&
                        annotated_page_content_done_ && pdf_done_) ||
                       primary_page_changed_;
  if (!work_complete) {
    return;
  }
  base::UmaHistogramTimes("Glic.PageContextFetcher.Total",
                          base::TimeTicks::Now() - start_time_);
  mojom::GetContextResultPtr result;
  if (web_contents() && web_contents()->GetPrimaryMainFrame() &&
      !primary_page_changed_) {
    auto tab_context = mojom::TabContext::New();
    tab_context->tab_data = CreateTabData(web_contents());
    // TODO(crbug.com/379773651): Clean up logspam when it's no longer useful.
    LOG(WARNING) << "GlicPageContextFetcher: Returning context for "
                 << tab_context->tab_data->url;
    if (inner_text_result_) {
      // Get trimmed text without copying.
      std::string trimmed_text = std::move(inner_text_result_->inner_text);
      size_t truncated_size = base::TruncateUTF8ToByteSize(
                                  trimmed_text, options_.inner_text_bytes_limit)
                                  .length();
      bool truncated = false;
      if (truncated_size < trimmed_text.length()) {
        truncated = true;
        trimmed_text.resize(truncated_size);
      }

      tab_context->web_page_data =
          mojom::WebPageData::New(mojom::DocumentData::New(
              web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              std::move(trimmed_text), truncated));
    }
    if (screenshot_) {
      tab_context->viewport_screenshot = std::move(screenshot_);
    }

    if (pdf_status_) {
      auto pdf_document_data = mojom::PdfDocumentData::New();
      pdf_document_data->origin = pdf_origin_;
      pdf_document_data->pdf_data = std::move(pdf_bytes_);
      pdf_document_data->size_limit_exceeded =
          *pdf_status_ ==
          pdf::mojom::PdfListener_GetPdfBytesStatus::kSizeLimitExceeded;
      tab_context->pdf_document_data = std::move(pdf_document_data);
    }

    if (annotated_page_content_) {
      auto annotated_page_data = mojom::AnnotatedPageData::New();
      annotated_page_data->annotated_page_content = mojo_base::ProtoWrapper(annotated_page_content_.value());
      tab_context->annotated_page_data = std::move(annotated_page_data);
    }

    result = mojom::GetContextResult::NewTabContext(std::move(tab_context));
  } else {
    result = mojom::GetContextResult::NewErrorReason(
        mojom::GetTabContextErrorReason::kWebContentsChanged);
  }
  std::move(callback_).Run(std::move(result));
}

}  // namespace glic
