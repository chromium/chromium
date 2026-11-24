// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/gemini_prompt_endpoint.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace glic {
namespace {

GlicKeyedService* GetAllowedGlicService(tabs::TabInterface& tab) {
  GlicKeyedService* service = GlicKeyedService::Get(tab.GetProfile());
  return service && service->enabling().IsAllowed() ? service : nullptr;
}

}  // namespace

GeminiPromptEndpoint::GeminiPromptEndpoint(
    InvokeWithAutoSubmitPasskey auto_submit_passkey)
    : auto_submit_passkey_(auto_submit_passkey) {}

GeminiPromptEndpoint::~GeminiPromptEndpoint() = default;

// static
mojom::TabContextResultPtr GeminiPromptEndpoint::BuildTabContextResult(
    mojom::TabDataPtr tab_data,
    ::selection::PageContext::EncodedScreenshot encoded_screenshot,
    ::selection::PageContext page_context) {
  auto tab_context = mojom::TabContextResult::New();
  tab_context->tab_data = std::move(tab_data);
  tab_context->viewport_screenshot = mojom::Screenshot::New(
      encoded_screenshot.size.width(), encoded_screenshot.size.height(),
      std::move(encoded_screenshot.data),
      std::move(encoded_screenshot.mime_type),
      mojom::ImageOriginAnnotations::New(),
      mojom::ScreenshotEncryptionScheme::kNone);
  if (page_context.apc.ByteSizeLong() > 0 || page_context.metadata) {
    auto annotated_page_data = mojom::AnnotatedPageData::New();
    if (page_context.apc.ByteSizeLong() > 0) {
      annotated_page_data->annotated_page_content =
          mojo_base::ProtoWrapper(page_context.apc);
    }
    annotated_page_data->metadata = std::move(page_context.metadata);
    tab_context->annotated_page_data = std::move(annotated_page_data);
  }
  optimization_guide::proto::ScreenshotInfo screenshot_info;
  if (page_context.apc.has_gemini_in_chrome_page_metadata() &&
      page_context.apc.gemini_in_chrome_page_metadata().has_screenshot_info()) {
    screenshot_info =
        page_context.apc.gemini_in_chrome_page_metadata().screenshot_info();
  }
  screenshot_info.set_has_selection_region_in_screenshot(true);
  tab_context->screenshot_info = mojo_base::ProtoWrapper(screenshot_info);
  if (page_context.pdf_data.has_value()) {
    auto pdf_document_data = mojom::PdfDocumentData::New();
    pdf_document_data->origin = std::move(page_context.pdf_data->origin);
    pdf_document_data->pdf_data = std::move(page_context.pdf_data->bytes);
    pdf_document_data->pdf_size_limit_exceeded =
        page_context.pdf_data->size_limit_exceeded;
    tab_context->pdf_document_data = std::move(pdf_document_data);
  }
  return tab_context;
}

::selection::mojom::PromptEndpointId GeminiPromptEndpoint::GetId() const {
  return ::selection::mojom::PromptEndpointId::kGemini;
}

bool GeminiPromptEndpoint::IsAvailable(tabs::TabInterface& tab) const {
  return GetAllowedGlicService(tab) != nullptr;
}

std::u16string GeminiPromptEndpoint::GetDisplayName() const {
  return l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_ENDPOINT_GEMINI);
}

std::u16string GeminiPromptEndpoint::GetPlaceholder() const {
  return l10n_util::GetStringUTF16(IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL);
}

void GeminiPromptEndpoint::Prepare(tabs::TabInterface& tab) {
  if (GlicKeyedService* service = GetAllowedGlicService(tab);
      service && !service->GetInstanceForTab(&tab)) {
    service->TryPreload(GlicWarmingTrigger::kSelectionOverlay);
  }
}

void GeminiPromptEndpoint::CancelPendingRequests() {
  weak_factory_.InvalidateWeakPtrs();
}

void GeminiPromptEndpoint::Submit(
    tabs::TabInterface& tab,
    std::string prompt,
    ::selection::PageContextWithSelection context) {
  CancelPendingRequests();

  GlicKeyedService* service = GetAllowedGlicService(tab);
  if (!service) {
    return;
  }

  ::selection::PageContext& page_context = context.page_context;
  const ::selection::AreaOfInterest& area_of_interest =
      context.area_of_interest;

  CapturedRegions captured_regions;
  if (!area_of_interest.is_text_selection &&
      !page_context.screenshot.drawsNothing()) {
    const int width = page_context.screenshot.width();
    const int height = page_context.screenshot.height();
    if (const auto* rect = std::get_if<gfx::Rect>(&area_of_interest.bounds)) {
      if (!rect->IsEmpty()) {
        gfx::Rect clamped =
            gfx::IntersectRects(*rect, gfx::Rect(width, height));
        if (!clamped.IsEmpty()) {
          captured_regions.emplace_back(
              base::UnguessableToken::Create(),
              mojom::CapturedRegion::NewRect(clamped));
        }
      }
    } else if (const auto* points = std::get_if<std::vector<gfx::Point>>(
                   &area_of_interest.bounds)) {
      if (!points->empty() &&
          base::FeatureList::IsEnabled(features::kGlicRegionSelectionLine) &&
          std::ranges::all_of(*points, [width, height](const gfx::Point& pt) {
            return pt.x() >= 0 && pt.x() < width && pt.y() >= 0 &&
                   pt.y() < height;
          })) {
        captured_regions.emplace_back(
            base::UnguessableToken::Create(),
            mojom::CapturedRegion::NewPolyline(*points));
      }
    }
  }

  if (captured_regions.empty()) {
    GlicInvokeOptions options(glic::Target(tab),
                              mojom::InvocationSource::kTextSelectionWidget);
    InvokeGlic(*service, std::move(prompt), std::move(options));
    return;
  }

  // Reuse the pre-encoded viewport screenshot when available so submission
  // stays synchronous without re-encoding on a worker thread.
  if (page_context.encoded_screenshot.has_value() &&
      !page_context.encoded_screenshot->data.empty() &&
      !page_context.encoded_screenshot->size.IsEmpty()) {
    ::selection::PageContext::EncodedScreenshot encoded =
        std::move(*page_context.encoded_screenshot);
    page_context.encoded_screenshot.reset();
    GlicInvokeOptions options(glic::Target(tab),
                              mojom::InvocationSource::kTextSelectionWidget);
    mojom::TabContextResultPtr tab_context = BuildTabContextResult(
        CreateTabData(&tab), std::move(encoded), std::move(page_context));
    options.additional_context = AdditionalTabContext(
        CreateRegionSelectionContext(tab, std::move(tab_context),
                                     std::move(captured_regions)),
        content::GlobalRenderFrameHostId(), PolicyCheck::kNone);
    InvokeGlic(*service, std::move(prompt), std::move(options));
    return;
  }

  // Fallback for callers that only supply an un-encoded `SkBitmap` screenshot
  // (e.g. without `PageContext::encoded_screenshot`).
  SkBitmap screenshot = std::move(page_context.screenshot);
  const gfx::Size screenshot_size(screenshot.width(), screenshot.height());
  page_content_annotations::ScreenshotOptions::ScreenshotCollectionOptions
      encode_options;
  encode_options.screenshot_image_format =
      page_content_annotations::ScreenshotOptions::ScreenshotImageFormat::kJpeg;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &page_content_annotations::EncodeScreenshot, std::move(screenshot),
          std::optional<page_content_annotations::ScreenshotOptions::
                            ScreenshotCollectionOptions>(encode_options)),
      base::BindOnce(&GeminiPromptEndpoint::OnScreenshotEncoded,
                     weak_factory_.GetWeakPtr(), tab.GetWeakPtr(),
                     std::move(prompt), screenshot_size,
                     std::move(page_context), std::move(captured_regions)));
}

void GeminiPromptEndpoint::InvokeGlic(GlicKeyedService& service,
                                      std::string prompt,
                                      GlicInvokeOptions options) const {
  if (!prompt.empty()) {
    options.prompts.push_back(std::move(prompt));
    service.InvokeWithAutoSubmit(auto_submit_passkey_, std::move(options));
  } else {
    service.Invoke(std::move(options));
  }
}

void GeminiPromptEndpoint::OnScreenshotEncoded(
    base::WeakPtr<tabs::TabInterface> weak_tab,
    std::string prompt,
    gfx::Size screenshot_size,
    ::selection::PageContext page_context,
    CapturedRegions captured_regions,
    std::optional<std::vector<uint8_t>> encoded_bytes) {
  if (!weak_tab) {
    return;
  }
  GlicKeyedService* service = GetAllowedGlicService(*weak_tab);
  if (!service) {
    return;
  }

  GlicInvokeOptions options(glic::Target(*weak_tab),
                            mojom::InvocationSource::kTextSelectionWidget);
  if (encoded_bytes.has_value()) {
    ::selection::PageContext::EncodedScreenshot encoded;
    encoded.data = std::move(*encoded_bytes);
    encoded.mime_type = "image/jpeg";
    encoded.size = screenshot_size;
    mojom::TabContextResultPtr tab_context =
        BuildTabContextResult(CreateTabData(weak_tab.get()), std::move(encoded),
                              std::move(page_context));
    options.additional_context = AdditionalTabContext(
        CreateRegionSelectionContext(*weak_tab, std::move(tab_context),
                                     std::move(captured_regions)),
        content::GlobalRenderFrameHostId(), PolicyCheck::kNone);
  }
  InvokeGlic(*service, std::move(prompt), std::move(options));
}

}  // namespace glic
