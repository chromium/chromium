// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_screenshot_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/screenshot_tool_result.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

namespace {

mojom::ActionResultCode ToActionResultCode(
    page_content_annotations::FetchPageContextError error) {
  using page_content_annotations::FetchPageContextError;
  switch (error) {
    case FetchPageContextError::kWebContentsWentAway:
      return mojom::ActionResultCode::kTabWentAway;
    case FetchPageContextError::kWebContentsChanged:
      return mojom::ActionResultCode::kScreenshotCapturePageChanged;
    case FetchPageContextError::kPageContextNotEligible:
      return mojom::ActionResultCode::kScreenshotCapturePageNotEligible;
    case FetchPageContextError::kUnknown:
      return mojom::ActionResultCode::kScreenshotCaptureFailed;
  }
}

}  // namespace

GetScreenshotTool::GetScreenshotTool(TaskId task_id,
                                     ToolDelegate& tool_delegate,
                                     ActorSurface& actor_surface,
                                     base::WeakPtr<content::Page> target_page)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      target_page_(std::move(target_page)) {}

GetScreenshotTool::~GetScreenshotTool() = default;

void GetScreenshotTool::Validate(DeprecatedToolCallback callback) {
  if (std::optional<mojom::ActionResultCode> error = CheckTargetPage()) {
    PostResponseTask(std::move(callback), MakeResult(*error));
    return;
  }
  PostResponseTask(std::move(callback), MakeOkResult());
}

mojom::ActionResultPtr GetScreenshotTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  if (std::optional<mojom::ActionResultCode> error = CheckTargetPage()) {
    return MakeResult(*error);
  }
  return MakeOkResult(/*requires_page_stabilization=*/false);
}

void GetScreenshotTool::Invoke(ToolResultCallback callback) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  CHECK(actor_surface);
  content::WebContents* web_contents = actor_surface->GetWebContents();
  CHECK(web_contents);

  page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions
      collection_options;
  collection_options.screenshot_image_format =
      page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::kJpeg;
  collection_options.screenshot_compression_quality = page_content_annotations::
      ScreenshotOptions::ScreenshotCompressionQuality::kHigh;

  page_content_annotations::FetchPageContextOptions options;
  options.screenshot_options =
      page_content_annotations::ScreenshotOptions::ViewportOnly(
          /*paint_preview_options=*/std::nullopt, collection_options);

  page_context_fetcher_ =
      std::make_unique<page_content_annotations::PageContextFetcher>(
          /*get_screenshot_service_callback=*/base::NullCallback(),
          /*progress_listener=*/nullptr,
          /*fetch_pdf_content_callback=*/base::NullCallback());
  page_context_fetcher_->FetchStart(
      *web_contents, options,
      base::BindOnce(&GetScreenshotTool::OnScreenshotFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GetScreenshotTool::Cancel() {
  // Destroying the fetcher synchronously runs its pending callback; ignore it.
  weak_ptr_factory_.InvalidateWeakPtrs();
  page_context_fetcher_.reset();
}

std::optional<mojom::ActionResultCode> GetScreenshotTool::CheckTargetPage()
    const {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    return mojom::ActionResultCode::kTabWentAway;
  }
  content::WebContents* web_contents = actor_surface->GetWebContents();
  if (web_contents->IsCrashed()) {
    return mojom::ActionResultCode::kRendererCrashed;
  }
  if (!target_page_ || &*target_page_ != &web_contents->GetPrimaryPage()) {
    return mojom::ActionResultCode::kScreenshotCapturePageChanged;
  }
  return std::nullopt;
}

void GetScreenshotTool::OnScreenshotFetched(
    ToolResultCallback callback,
    base::expected<
        std::unique_ptr<page_content_annotations::FetchPageContextResult>,
        page_content_annotations::FetchPageContextErrorDetails> result) {
  page_context_fetcher_.reset();

  if (std::optional<mojom::ActionResultCode> error = CheckTargetPage()) {
    PostResponseTask(std::move(callback), ToolResult::Error(ToolError(*error)));
    return;
  }

  if (!result.has_value()) {
    PostResponseTask(std::move(callback),
                     ToolResult::Error(ToolError(
                         ToActionResultCode(result.error().error_code),
                         result.error().message)));
    return;
  }

  CHECK(result.value());
  if (!result.value()->screenshot_result.has_value()) {
    PostResponseTask(std::move(callback),
                     ToolResult::Error(ToolError(
                         mojom::ActionResultCode::kScreenshotCaptureFailed,
                         result.value()->screenshot_result.error().message)));
    return;
  }

  page_content_annotations::ScreenshotResult& screenshot_result =
      *result.value()->screenshot_result;
  if (screenshot_result.screenshot_data.empty()) {
    PostResponseTask(std::move(callback),
                     ToolResult::Error(ToolError(
                         mojom::ActionResultCode::kScreenshotCaptureFailed,
                         "Captured screenshot data is empty")));
    return;
  }

  ToolResult tool_result = ToolResult::Success(ScreenshotToolResult(
      std::move(screenshot_result.screenshot_data),
      std::move(screenshot_result.mime_type), screenshot_result.dimensions));
  tool_result.deprecated_action_result()->requires_page_stabilization = false;
  PostResponseTask(std::move(callback), std::move(tool_result));
}

std::string GetScreenshotTool::DebugString() const {
  return "GetScreenshotTool";
}

std::string GetScreenshotTool::JournalEvent() const {
  return "GetScreenshot";
}

GURL GetScreenshotTool::JournalURL() const {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (actor_surface && actor_surface->GetWebContents()) {
    return actor_surface->GetWebContents()->GetLastCommittedURL();
  }
  return GURL();
}

std::unique_ptr<ObservationDelayController>
GetScreenshotTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

ActorSurfaceHandle GetScreenshotTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
