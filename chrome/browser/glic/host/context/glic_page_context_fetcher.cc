// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/glic/host/context/glic_page_context_eligibility_observer.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/media/glic_media_integration.h"
#include "chrome/common/chrome_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/common/constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "pdf/mojom/pdf.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/origin.h"

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

// Coordinates fetching multiple types of page context.
class GlicPageContextFetcher : public content::WebContentsObserver {
 public:
  GlicPageContextFetcher() = default;
  ~GlicPageContextFetcher() override = default;

  void FetchStart(
      FocusedTabData focused_tab_data,
      const mojom::GetTabContextOptions& options,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback
          callback) {
    base::expected<content::WebContents*, std::string_view> focus =
        focused_tab_data.GetFocus();
    if (!focus.has_value()) {
      std::move(callback).Run(
          mojom::GetContextResult::NewErrorReason(std::string(focus.error())));
      return;
    }
    options_ = options;

    content::WebContents* aweb_contents = focus.value();
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
      RecordPdfRequestState(is_pdf_document,
                            /*pdf_found=*/pdf_helper != nullptr);
      // GetPdfBytes() is not safe before IsDocumentLoadComplete() = true.
      if (is_pdf_document && pdf_helper &&
          pdf_helper->IsDocumentLoadComplete()) {
        pdf_origin_ = pdf_helper->render_frame_host().GetLastCommittedOrigin();
        pdf_helper->GetPdfBytes(
            options_.pdf_size_limit,
            base::BindOnce(&GlicPageContextFetcher::ReceivedPdfBytes,
                           GetWeakPtr()));
        pdf_done_ = false;  // Will fetch PDF contents.
      }
    }

    if (options.include_annotated_page_content) {
      blink::mojom::AIPageContentOptionsPtr ai_page_content_options;
      ai_page_content_options =
          optimization_guide::DefaultAIPageContentOptions();
      ai_page_content_options->include_geometry = false;
      ai_page_content_options->on_critical_path = true;
      ai_page_content_options->include_hidden_searchable_content = true;
      ai_page_content_options->max_meta_elements = options.max_meta_tags;
      // TODO(crbug.com/409564704): Move actor page content extraction to the
      // actor coordinator.
      if (base::FeatureList::IsEnabled(features::kGlicActor)) {
        ai_page_content_options->include_geometry = true;
        ai_page_content_options->enable_experimental_actionable_data = true;
      }
      optimization_guide::GetAIPageContent(
          web_contents(), std::move(ai_page_content_options),
          base::BindOnce(&GlicPageContextFetcher::ReceivedAnnotatedPageContent,
                         GetWeakPtr()));
    } else {
      annotated_page_content_done_ = true;
    }

    // Will only fetch context eligibility if we can observe it.
    context_eligibility_check_done_ =
        !(GlicPageContextEligibilityObserver::MaybeGetEligibilityForWebContents(
            web_contents(),
            base::BindOnce(&GlicPageContextFetcher::ReceivedContextEligibility,
                           GetWeakPtr())));

    // Note: initialization_done_ guards against processing
    // `RunCallbackIfComplete()` until we reach this point.
    initialization_done_ = true;
    RunCallbackIfComplete();
  }

  void ReceivedPdfBytes(pdf::mojom::PdfListener::GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes,
                        uint32_t page_count) {
    pdf_done_ = true;
    pdf_status_ = status;
    pdf_bytes_ = pdf_bytes;
    RunCallbackIfComplete();
  }

  void GetTabScreenshot(content::WebContents& web_contents) {
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

  void ReceivedViewportBitmap(const SkBitmap& bitmap) {
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

  // content::WebContentsObserver impl.
  void PrimaryPageChanged(content::Page& page) override {
    primary_page_changed_ = true;
    RunCallbackIfComplete();
  }

  void RecievedJpegScreenshot(
      std::optional<std::vector<uint8_t>> screenshot_jpeg_data) {
    if (screenshot_jpeg_data) {
      screenshot_ = glic::mojom::Screenshot::New(
          screenshot_dimensions_.width(), screenshot_dimensions_.height(),
          std::move(*screenshot_jpeg_data), "image/jpeg",
          // TODO(crbug.com/380495633): Finalize and implement image
          // annotations.
          glic::mojom::ImageOriginAnnotations::New());
    }
    screenshot_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetEncodedScreenshot",
                            base::TimeTicks::Now() - start_time_);
    RunCallbackIfComplete();
  }

  void ReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result) {
    inner_text_result_ = std::move(result);
    inner_text_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetInnerText",
                            base::TimeTicks::Now() - start_time_);
    RunCallbackIfComplete();
  }

  void ReceivedAnnotatedPageContent(
      std::optional<optimization_guide::AIPageContentResult> content) {
    annotated_page_content_result_ = std::move(content);
    annotated_page_content_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetAnnotatedPageContent",
                            base::TimeTicks::Now() - start_time_);
    RunCallbackIfComplete();
  }

  void ReceivedContextEligibility(bool is_eligible) {
    context_eligible_ = is_eligible;
    context_eligibility_check_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetContextEligibility",
                            base::TimeTicks::Now() - start_time_);
    base::UmaHistogramBoolean("Glic.PageContextFetcher.PageContextEligible",
                              *context_eligible_);
    RunCallbackIfComplete();
  }

  void RunCallbackIfComplete() {
    if (!initialization_done_) {
      return;
    }

    // Continue only if the primary page changed or work is complete.
    bool work_complete =
        (screenshot_done_ && inner_text_done_ && annotated_page_content_done_ &&
         pdf_done_ && context_eligibility_check_done_) ||
        primary_page_changed_;
    if (!work_complete) {
      return;
    }
    base::UmaHistogramTimes("Glic.PageContextFetcher.Total",
                            base::TimeTicks::Now() - start_time_);

    mojom::GetContextResultPtr result;
    if (primary_page_changed_) {
      result = mojom::GetContextResult::NewErrorReason("web contents changed");
      std::move(callback_).Run(std::move(result));
      return;
    }

    if (!web_contents() || !web_contents()->GetPrimaryMainFrame()) {
      result = mojom::GetContextResult::NewErrorReason("web contents changed");
      std::move(callback_).Run(std::move(result));
      return;
    }

    if (!context_eligible_.value_or(true)) {
      result =
          mojom::GetContextResult::NewErrorReason("page context ineligible");
      std::move(callback_).Run(std::move(result));
      return;
    }

    auto tab_context = mojom::TabContext::New();
    tab_context->tab_data = CreateTabData(web_contents());
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

      // Warning!: `pdf_bytes_` can be larger than pdf_size_limit.
      // `pdf_size_limit` applies to the original PDF size, but the PDF is
      // re-serialized and returned, so it is not identical to the original.
      pdf_document_data->size_limit_exceeded =
          *pdf_status_ ==
              pdf::mojom::PdfListener_GetPdfBytesStatus::kSizeLimitExceeded ||
          pdf_bytes_.size() > options_.pdf_size_limit;
      if (!pdf_document_data->size_limit_exceeded) {
        pdf_document_data->pdf_data = std::move(pdf_bytes_);
      }

      tab_context->pdf_document_data = std::move(pdf_document_data);
    }

    if (annotated_page_content_result_) {
      auto annotated_page_data = mojom::AnnotatedPageData::New();

      if (auto* media_integration =
              GlicMediaIntegration::GetFor(web_contents())) {
        optimization_guide::proto::ContentNode* media_node =
            annotated_page_content_result_->proto.mutable_root_node()
                ->add_children_nodes();

        media_integration->AppendContext(web_contents(), media_node);
      }

      annotated_page_data->annotated_page_content =
          mojo_base::ProtoWrapper(annotated_page_content_result_->proto);

      annotated_page_data->metadata =
          std::move(annotated_page_content_result_->metadata);

      tab_context->annotated_page_data = std::move(annotated_page_data);
    }

    result = mojom::GetContextResult::NewTabContext(std::move(tab_context));
    std::move(callback_).Run(std::move(result));
  }

 private:
  base::WeakPtr<GlicPageContextFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback_;

  mojom::GetTabContextOptions options_;

  // Intermediate results:

  // Whether work is complete for each task, does not imply success.
  bool initialization_done_ = false;
  bool screenshot_done_ = false;
  bool inner_text_done_ = false;
  bool pdf_done_ = false;
  bool annotated_page_content_done_ = false;
  bool context_eligibility_check_done_ = false;
  // Whether the primary page has changed since context fetching began.
  bool primary_page_changed_ = false;
  url::Origin pdf_origin_;
  std::optional<std::vector<uint8_t>> screenshot_jpeg_data_;
  SkISize screenshot_dimensions_;
  glic::mojom::ScreenshotPtr screenshot_;
  std::unique_ptr<content_extraction::InnerTextResult> inner_text_result_;
  std::vector<uint8_t> pdf_bytes_;
  std::optional<pdf::mojom::PdfListener_GetPdfBytesStatus> pdf_status_;
  std::optional<optimization_guide::AIPageContentResult>
      annotated_page_content_result_;
  std::optional<bool> context_eligible_;
  base::TimeTicks start_time_;

  base::WeakPtrFactory<GlicPageContextFetcher> weak_ptr_factory_{this};
};

}  // namespace

void FetchPageContext(
    FocusedTabData focused_tab_data,
    const mojom::GetTabContextOptions& options,
    glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  CHECK(callback);
  auto self = std::make_unique<GlicPageContextFetcher>();
  auto* raw_self = self.get();
  raw_self->FetchStart(
      focused_tab_data, options,
      base::BindOnce(
          // Bind `fetcher` to the callback to keep it in scope until it
          // returns.
          [](std::unique_ptr<glic::GlicPageContextFetcher> fetcher,
             mojom::WebClientHandler::GetContextFromFocusedTabCallback callback,
             mojom::GetContextResultPtr result) {
            std::move(callback).Run(std::move(result));
          },
          std::move(self), std::move(callback)));
}

}  // namespace glic
