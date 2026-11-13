// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_context_capturer.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "chrome/browser/ttc/core/page_context_util.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace ttc {

namespace {

using actor::mojom::ActionResultCode;
using page_content_annotations::ScreenshotOptions;

// For a screenshot's maximum width and height: do not scale it down.
constexpr int kNoScaling = 0;

PointerContextCapturer::Result Failure(ActionResultCode code,
                                       std::string message) {
  return base::unexpected(ToolResponse::Error(code, std::move(message)));
}

PointerContextCapturer::Result ToResult(
    float scale_factor,
    page_content_annotations::FetchPageContextResultCallbackArg fetch_result) {
  if (!fetch_result.has_value()) {
    return Failure(ToScreenshotErrorCode(fetch_result.error().error_code),
                   fetch_result.error().message);
  }
  // `PageContextFetcher` only reports success with a non-null result.
  page_content_annotations::FetchPageContextResult& fetched =
      *fetch_result.value();
  if (!fetched.screenshot_result.has_value()) {
    return Failure(ActionResultCode::kScreenshotCaptureFailed,
                   fetched.screenshot_result.error().message);
  }
  if (fetched.screenshot_result->screenshot_data.empty() ||
      !fetched.annotated_page_content_result.has_value()) {
    return Failure(ActionResultCode::kScreenshotCaptureFailed,
                   "Could not capture the page");
  }
  return PointerContext{
      .screenshot = std::move(fetched.screenshot_result->screenshot_data),
      .scale_factor = scale_factor,
      .page_content = std::move(fetched.annotated_page_content_result->proto)};
}

}  // namespace

PointerContextCapturer::PointerContextCapturer(base::TimeDelta timeout)
    : timeout_(timeout) {}

PointerContextCapturer::~PointerContextCapturer() = default;

void PointerContextCapturer::Capture(content::WebContents* web_contents,
                                     Callback callback) {
  CHECK(!callback_);
  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (!view || web_contents->IsCrashed()) {
    std::move(callback).Run(Failure(
        web_contents->IsCrashed() ? ActionResultCode::kRendererCrashed
                                  : ActionResultCode::kScreenshotCaptureFailed,
        "The page has no view to capture"));
    return;
  }
  callback_ = std::move(callback);

  // One fetch for both, so that the screenshot and the page content are of
  // the same moment. The screenshot is neither scaled down, so that one scale
  // factor holds for it and the page content's geometry, nor compressed much,
  // since the image made from it is compressed again.
  ScreenshotOptions::ScreenshotCollectionOptions collection_options;
  collection_options.max_width = kNoScaling;
  collection_options.max_height = kNoScaling;
  collection_options.screenshot_image_format =
      ScreenshotOptions::ScreenshotImageFormat::kJpeg;
  collection_options.screenshot_compression_quality =
      ScreenshotOptions::ScreenshotCompressionQuality::kNone;
  page_content_annotations::FetchPageContextOptions options;
  options.screenshot_options = ScreenshotOptions::ViewportOnly(
      /*paint_preview_options=*/std::nullopt, collection_options);
  // Actionable mode has the geometry, as the actor's observations do.
  options.annotated_page_content_options =
      optimization_guide::ActionableAIPageContentOptions(
          /*on_critical_path=*/true);

  fetcher_ = std::make_unique<page_content_annotations::PageContextFetcher>(
      /*get_screenshot_service_callback=*/base::NullCallback(),
      /*progress_listener=*/nullptr,
      /*fetch_pdf_content_callback=*/base::NullCallback());
  timer_.Start(FROM_HERE, timeout_, this, &PointerContextCapturer::OnTimeout);
  fetcher_->FetchStart(
      *web_contents, options,
      base::BindOnce(&PointerContextCapturer::OnFetched,
                     weak_factory_.GetWeakPtr(), view->GetDeviceScaleFactor()));
}

void PointerContextCapturer::OnFetched(
    float scale_factor,
    page_content_annotations::FetchPageContextResultCallbackArg fetch_result) {
  timer_.Stop();
  fetcher_.reset();
  std::move(callback_).Run(ToResult(scale_factor, std::move(fetch_result)));
}

void PointerContextCapturer::OnTimeout() {
  // Destroying the fetcher makes it report an error; ignore it.
  weak_factory_.InvalidateWeakPtrs();
  fetcher_.reset();
  std::move(callback_).Run(
      Failure(ActionResultCode::kToolTimeout, "Timed out capturing the page"));
}

}  // namespace ttc
