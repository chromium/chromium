// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_tab_context_extraction_runner.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/geic/geic_browser_host_impl.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace geic {

namespace {

constexpr int kJpegQuality = 80;

std::optional<std::vector<uint8_t>> EncodeBitmapToJpeg(SkBitmap bitmap) {
  return gfx::JPEGCodec::Encode(bitmap, kJpegQuality);
}

}  // namespace

TabContextExtractionRunner::TabContextExtractionRunner(
    base::WeakPtr<GeicBrowserHostImpl> host,
    content::WebContents* contents,
    mojom::TabMetadataPtr metadata,
    mojom::TabContextOptions options,
    mojom::GeicBrowserHost::GetContextFromFocusedTabCallback callback)
    : host_(std::move(host)),
      options_(std::move(options)),
      callback_(std::move(callback)) {
  data_ = mojom::TabContextData::New();
  data_->metadata = std::move(metadata);
  if (contents && contents->GetPrimaryMainFrame()) {
    document_ptr_ = contents->GetPrimaryMainFrame()->GetWeakDocumentPtr();
  }
}

TabContextExtractionRunner::~TabContextExtractionRunner() {
  if (callback_) {
    std::move(callback_).Run(
        base::unexpected(mojom::GetTabContextError::kTabClosed));
  }
}

void TabContextExtractionRunner::Run() {
  content::RenderFrameHost* primary_main_frame =
      document_ptr_.AsRenderFrameHostIfValid();
  if (!primary_main_frame) {
    std::move(callback_).Run(
        base::unexpected(mojom::GetTabContextError::kNavigationInProgress));
    return;
  }

  const bool include_inner_text = options_.include_inner_text;
  const bool include_annotated_page_content =
      options_.include_annotated_page_content.value_or(false);
  const bool include_screenshot = options_.include_screenshot;

  const size_t num_extractions =
      include_inner_text + include_annotated_page_content + include_screenshot;

  if (num_extractions == 0) {
    std::move(callback_).Run(std::move(data_));
    return;
  }

  auto barrier_closure = base::BarrierClosure(
      num_extractions,
      base::BindOnce(&TabContextExtractionRunner::OnAllExtractionsComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  if (include_inner_text) {
    GetInnerText(primary_main_frame, barrier_closure);
  }
  if (include_annotated_page_content) {
    GetAnnotatedPageContent(
        content::WebContents::FromRenderFrameHost(primary_main_frame),
        barrier_closure);
  }
  if (include_screenshot) {
    CaptureScreenshot(primary_main_frame, barrier_closure);
  }
}

void TabContextExtractionRunner::GetInnerText(
    content::RenderFrameHost* primary_main_frame,
    base::RepeatingClosure closure) {
  // TODO(crbug.com/539909218): Honour options->inner_text_bytes_limit at DOM
  // extraction time in content_extraction to avoid pulling unconstrained text.
  content_extraction::GetInnerText(
      *primary_main_frame,
      /*node_id=*/std::nullopt,
      base::BindOnce(&TabContextExtractionRunner::DidGetInnerText,
                     weak_ptr_factory_.GetWeakPtr(), closure));
}

void TabContextExtractionRunner::DidGetInnerText(
    base::RepeatingClosure closure,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (result) {
    if (options_.inner_text_bytes_limit > 0 &&
        result->inner_text.size() > options_.inner_text_bytes_limit) {
      base::TruncateUTF8ToByteSize(result->inner_text,
                                   options_.inner_text_bytes_limit,
                                   &result->inner_text);
    }
    data_->inner_text = std::move(result->inner_text);
  }
  closure.Run();
}

void TabContextExtractionRunner::GetAnnotatedPageContent(
    content::WebContents* contents,
    base::RepeatingClosure closure) {
  blink::mojom::AIPageContentOptionsPtr options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);

  // TODO(crbug.com/556213206): Look into whether to use PageContextFetcher
  // for page content fetching instead.
  optimization_guide::GetAIPageContent(
      contents, std::move(options),
      base::BindOnce(&TabContextExtractionRunner::DidGetAnnotatedPageContent,
                     weak_ptr_factory_.GetWeakPtr(), closure));
}

void TabContextExtractionRunner::DidGetAnnotatedPageContent(
    base::RepeatingClosure closure,
    optimization_guide::AIPageContentResultOrError result) {
  if (result.has_value()) {
    data_->annotated_page_data = mojo_base::ProtoWrapper(result->proto);
  }
  closure.Run();
}

// static
gfx::Size TabContextExtractionRunner::CalculateScreenshotSize(
    const gfx::Size& view_size,
    uint32_t max_width,
    uint32_t max_height) {
  if (view_size.IsEmpty() || (max_width == 0 && max_height == 0)) {
    return gfx::Size();
  }

  float scale = 1.0f;
  if (max_width > 0 && view_size.width() > 0) {
    scale = std::min(scale, static_cast<float>(max_width) /
                                static_cast<float>(view_size.width()));
  }
  if (max_height > 0 && view_size.height() > 0) {
    scale = std::min(scale, static_cast<float>(max_height) /
                                static_cast<float>(view_size.height()));
  }

  if (scale >= 1.0f) {
    return gfx::Size();
  }

  gfx::Size scaled_size = gfx::ScaleToFlooredSize(view_size, scale);
  scaled_size.SetToMax(gfx::Size(1, 1));
  return scaled_size;
}

void TabContextExtractionRunner::CaptureScreenshot(
    content::RenderFrameHost* primary_main_frame,
    base::RepeatingClosure closure) {
  content::RenderWidgetHostView* view = primary_main_frame->GetView();
  if (!view) {
    closure.Run();
    return;
  }

  const gfx::Size screenshot_size = CalculateScreenshotSize(
      view->GetVisibleViewportSize(), options_.screenshot_max_width,
      options_.screenshot_max_height);

  view->CopyFromSurface(
      gfx::Rect(), screenshot_size, base::TimeDelta(),
      base::BindOnce(&TabContextExtractionRunner::DidCaptureScreenshot,
                     weak_ptr_factory_.GetWeakPtr(), closure));
}

void TabContextExtractionRunner::DidCaptureScreenshot(
    base::RepeatingClosure closure,
    const content::CopyFromSurfaceResult& result) {
  SkBitmap bitmap = result.value_or(viz::CopyOutputBitmapWithMetadata()).bitmap;
  if (bitmap.drawsNothing()) {
    closure.Run();
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&EncodeBitmapToJpeg, std::move(bitmap)),
      base::BindOnce(&TabContextExtractionRunner::OnScreenshotEncoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(closure)));
}

void TabContextExtractionRunner::OnScreenshotEncoded(
    base::RepeatingClosure closure,
    std::optional<std::vector<uint8_t>> jpeg_data) {
  if (jpeg_data.has_value()) {
    data_->screenshot_data = std::move(*jpeg_data);
    data_->screenshot_mime_type = "image/jpeg";
  }
  closure.Run();
}

void TabContextExtractionRunner::OnAllExtractionsComplete() {
  if (!host_) {
    std::move(callback_).Run(
        base::unexpected(mojom::GetTabContextError::kTabClosed));
    return;
  }

  auto validated = host_->GetValidatedActiveTab();
  // Defend against primary main frame document replacement / navigation during
  // async parallel extraction. We check whether `document_ptr_` is still valid
  // and matches the current active document. Subframe navigations are
  // deliberately ignored because context is extracted from the primary main
  // frame only.
  if (!validated.contents || document_ptr_.AsRenderFrameHostIfValid() !=
                                 validated.contents->GetPrimaryMainFrame()) {
    std::move(callback_).Run(
        base::unexpected(mojom::GetTabContextError::kNavigationInProgress));
    return;
  }

  data_->metadata = std::move(validated.metadata);
  std::move(callback_).Run(std::move(data_));
}

}  // namespace geic
