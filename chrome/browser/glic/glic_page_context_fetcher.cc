// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_page_context_fetcher.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
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

}  // namespace

GlicPageContextFetcher::GlicPageContextFetcher() = default;

GlicPageContextFetcher::~GlicPageContextFetcher() = default;

void GlicPageContextFetcher::Fetch(
    content::WebContents* aweb_contents,
    const mojom::GetTabContextOptions& options,
    glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  // Fetch() should be called only once.
  CHECK_EQ(web_contents(), nullptr);
  Observe(aweb_contents);

  callback_ = std::move(callback);

  if (options.include_viewport_screenshot) {
    GetTabScreenshot(*web_contents());
  } else {
    screenshot_done_ = true;
  }

  if (options.include_inner_text) {
    content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
    // TODO(crbug.com/378937313): Finish this provisional implementation.
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

  if (!view) {
    std::move(callback).Run({});
    DLOG(WARNING) << "Could not retrieve RenderWidgetHostView.";
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(),  // Copy entire surface area.
      gfx::Size(),  // Empty output_size means no down scaling.
      base::BindOnce(&GlicPageContextFetcher::ReceivedViewportBitmap,
                     GetWeakPtr()));
}

void GlicPageContextFetcher::ReceivedViewportBitmap(const SkBitmap& bitmap) {
  screenshot_dimensions_ = bitmap.dimensions();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const SkBitmap& bitmap) {
            return gfx::JPEGCodec::Encode(bitmap, /*quality=*/100);
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
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::ReceivedInnerText(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  inner_text_result_ = std::move(result);
  inner_text_done_ = true;
  RunCallbackIfComplete();
}

void GlicPageContextFetcher::ReceivedAnnotatedPageContent(
    std::optional<optimization_guide::proto::AnnotatedPageContent> content) {
  annotated_page_content_ = std::move(content);
  annotated_page_content_done_ = true;
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
  mojom::GetContextResultPtr result;
  if (web_contents() && web_contents()->GetPrimaryMainFrame() &&
      !primary_page_changed_) {
    auto tab_context = mojom::TabContext::New();
    tab_context->tab_data = CreateTabData(web_contents());
    // TODO(crbug.com/379773651): Clean up logspam when it's no longer useful.
    LOG(WARNING) << "GlicPageContextFetcher: Returning context for "
                 << tab_context->tab_data->url;
    if (inner_text_result_) {
      tab_context->web_page_data =
          mojom::WebPageData::New(mojom::DocumentData::New(
              web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              std::move(inner_text_result_->inner_text)));
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
