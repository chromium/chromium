// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_page_context_fetcher.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace glic {

namespace {
glic::mojom::TabDataPtr GetTabData(content::WebContents* web_contents) {
  return glic::mojom::TabData::New(
      sessions::SessionTabHelper::IdForTab(web_contents).id(),
      sessions::SessionTabHelper::IdForWindowContainingTab(web_contents).id(),
      web_contents->GetLastCommittedURL(),
      base::UTF16ToUTF8(web_contents->GetTitle()));
}
}  // namespace

GlicPageContextFetcher::GlicPageContextFetcher() = default;

GlicPageContextFetcher::~GlicPageContextFetcher() = default;

void GlicPageContextFetcher::Fetch(
    content::WebContents* aweb_contents,
    bool include_inner_text,
    bool include_viewport_screenshot,
    glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) {
  // Fetch() should be called only once.
  CHECK_EQ(web_contents(), nullptr);
  Observe(aweb_contents);

  callback_ = std::move(callback);

  if (include_viewport_screenshot) {
    GetTabScreenshot(*web_contents());
  } else {
    screenshot_done_ = true;
  }

  if (include_inner_text) {
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

void GlicPageContextFetcher::RunCallbackIfComplete() {
  // Continue only if the primary page changed or work is complete.
  bool work_complete =
      (screenshot_done_ && inner_text_done_) || primary_page_changed_;
  if (!work_complete) {
    return;
  }
  glic::mojom::TabContextResultPtr result;
  if (web_contents() && web_contents()->GetPrimaryMainFrame() &&
      !primary_page_changed_) {
    result = glic::mojom::TabContextResult::New();
    result->tab_data = GetTabData(web_contents());
    // TODO(crbug.com/379773651): Clean up logspam when it's no longer useful.
    LOG(WARNING) << "GlicPageContextFetcher: Returning context for "
                 << result->tab_data->url;
    if (inner_text_result_) {
      result->web_page_data =
          glic::mojom::WebPageData::New(glic::mojom::DocumentData::New(
              web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
              std::move(inner_text_result_->inner_text)));
    }
    if (screenshot_) {
      result->viewport_screenshot = std::move(screenshot_);
    }
  }
  std::move(callback_).Run(std::move(result));
}

}  // namespace glic
