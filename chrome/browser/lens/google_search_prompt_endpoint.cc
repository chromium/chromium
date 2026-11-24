// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/google_search_prompt_endpoint.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/pdf/common/constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace lens {

BASE_FEATURE(kSmartSelectionGoogleSearch, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

using SearchUrlType =
    contextual_search::ContextualSearchContextController::SearchUrlType;
using CreateSearchUrlRequestInfo = contextual_search::
    ContextualSearchContextController::CreateSearchUrlRequestInfo;

// TODO(b/587425707): Evaluate adding dedicated `ChromeAimEntryPoint` and
// `LensOverlayInvocationSource` enum values so metrics can distinguish Smart
// Selection Google Search prompt submissions from Lens Overlay opens
// (`EntrySource::kLensOverlay`) and `LensSuggestionTool`
// (`LensOverlayInvocationSource::kSelectionSuggestion`).
constexpr omnibox::ChromeAimEntryPoint kSmartSelectionSearchEntryPoint =
    omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT;

ImageEncodingOptions GetViewportEncodingOptions() {
  const auto& image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  return ImageEncodingOptions{
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

}  // namespace

GoogleSearchPromptEndpoint::GoogleSearchPromptEndpoint() = default;

GoogleSearchPromptEndpoint::~GoogleSearchPromptEndpoint() = default;

// static
mojom::CenterRotatedBoxPtr
GoogleSearchPromptEndpoint::ConvertBoundsToCenterRotatedBox(
    const std::variant<gfx::Rect, std::vector<gfx::Point>>& bounds,
    const gfx::Size& image_size) {
  if (image_size.width() <= 0 || image_size.height() <= 0) {
    return nullptr;
  }
  const gfx::Rect viewport(image_size);

  if (const auto* rect = std::get_if<gfx::Rect>(&bounds)) {
    const gfx::Rect clipped = gfx::IntersectRects(*rect, viewport);
    if (clipped.IsEmpty()) {
      return nullptr;
    }
    return GetCenterRotatedBoxFromTabViewAndImageBounds(viewport, viewport,
                                                        clipped);
  }

  if (const auto* points = std::get_if<std::vector<gfx::Point>>(&bounds)) {
    if (points->empty()) {
      return nullptr;
    }
    int min_x = points->front().x();
    int max_x = min_x;
    int min_y = points->front().y();
    int max_y = min_y;
    for (const gfx::Point& pt : *points) {
      min_x = std::min(min_x, pt.x());
      max_x = std::max(max_x, pt.x());
      min_y = std::min(min_y, pt.y());
      max_y = std::max(max_y, pt.y());
    }
    // Ensure in-bounds collinear (e.g. horizontal or vertical) polylines retain
    // a non-zero bounding box so region cropping succeeds. Out-of-bounds
    // collinear polylines are left empty so clamping yields nullptr.
    if (min_x == max_x) {
      if (min_x == image_size.width()) {
        --min_x;
      } else if (min_x >= 0 && min_x < image_size.width()) {
        ++max_x;
      }
    }
    if (min_y == max_y) {
      if (min_y == image_size.height()) {
        --min_y;
      } else if (min_y >= 0 && min_y < image_size.height()) {
        ++max_y;
      }
    }
    min_x = std::clamp(min_x, 0, image_size.width());
    max_x = std::clamp(max_x, 0, image_size.width());
    min_y = std::clamp(min_y, 0, image_size.height());
    max_y = std::clamp(max_y, 0, image_size.height());
    const gfx::Rect clipped(min_x, min_y, max_x - min_x, max_y - min_y);
    if (clipped.IsEmpty()) {
      return nullptr;
    }
    return GetCenterRotatedBoxFromTabViewAndImageBounds(viewport, viewport,
                                                        clipped);
  }

  return nullptr;
}

// static
std::unique_ptr<ContextualInputData>
GoogleSearchPromptEndpoint::BuildContextualInputData(
    tabs::TabInterface& tab,
    const ::selection::PageContext& page_context) {
  auto input_data = std::make_unique<ContextualInputData>();
  input_data->viewport_screenshot = page_context.screenshot;
  input_data->is_implicit_upload = true;
  input_data->has_lens_usage_intent = true;
  input_data->is_page_context_eligible = true;
  input_data->upload_type = LensOverlayContextualInputUploadType::
      CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;

  Profile* profile = tab.GetProfile();
  CHECK(profile);
  content::WebContents* web_contents = tab.GetContents();
  // Only attach PDF bytes for top-level PDF documents. When embedded PDF
  // extraction (`kGlicEmbeddedPdfBytesExtraction`) is enabled, `pdf_data` may
  // come from an embedded (potentially cross-origin) iframe; attaching those
  // bytes here would misattribute them to the host page's `page_url` and
  // shadow the host page's APC. Likewise, do not fall back to Blink's wrapper
  // APC on a top-level PDF when PDF bytes are absent (e.g. size limit
  // exceeded).
  const bool is_top_level_pdf =
      web_contents && web_contents->GetContentsMimeType() == pdf::kPDFMimeType;
  const bool has_pdf_bytes = is_top_level_pdf &&
                             page_context.pdf_data.has_value() &&
                             page_context.pdf_data->bytes.has_value();
  const bool has_apc = !is_top_level_pdf && page_context.apc.ByteSizeLong() > 0;
  const bool has_page_content = has_pdf_bytes || has_apc;
  // TODO(b/587425707): Confirm with privacy/product whether Glic / Smart
  // Selection consent covers sending the viewport screenshot and page content
  // to Google Search without going through the Lens overlay privacy notice
  // (`DidUserGrantLensOverlayNeededPermissions` /
  // `CanSharePageContentWithLensOverlay`). Keep `kSmartSelectionGoogleSearch`
  // disabled by default until resolved.
  const bool include_page_context =
      web_contents && IsLensOverlayContextualSearchboxEnabled(profile) &&
      has_page_content;

  if (!include_page_context) {
    input_data->primary_content_type = MimeType::kImage;
    input_data->context_input = std::vector<ContextualInput>();
    return input_data;
  }

  input_data->tab_session_id =
      sessions::SessionTabHelper::IdForTab(web_contents);
  input_data->page_url = web_contents->GetLastCommittedURL();
  input_data->page_title = base::UTF16ToUTF8(web_contents->GetTitle());

  std::vector<ContextualInput> inputs;
  MimeType primary_mime_type = MimeType::kImage;

  if (has_pdf_bytes) {
    primary_mime_type = MimeType::kPdf;
    inputs.emplace_back(*page_context.pdf_data->bytes, MimeType::kPdf);
  } else if (has_apc) {
    std::vector<uint8_t> serialized_apc(page_context.apc.ByteSizeLong());
    if (page_context.apc.SerializeToArray(
            serialized_apc.data(), static_cast<int>(serialized_apc.size()))) {
      primary_mime_type = MimeType::kAnnotatedPageContent;
      inputs.emplace_back(std::move(serialized_apc),
                          MimeType::kAnnotatedPageContent);
    }
  }

  input_data->primary_content_type = primary_mime_type;
  input_data->context_input = std::move(inputs);
  return input_data;
}

// static
std::unique_ptr<CreateSearchUrlRequestInfo>
GoogleSearchPromptEndpoint::BuildSearchUrlRequestInfo(
    const std::string& prompt,
    const base::UnguessableToken& context_token,
    const SkBitmap& screenshot,
    mojom::CenterRotatedBoxPtr region_box) {
  auto request_info = std::make_unique<CreateSearchUrlRequestInfo>();
  request_info->search_url_type = SearchUrlType::kStandard;
  request_info->query_text = prompt;
  request_info->query_start_time = base::Time::Now();
  if (!context_token.is_empty()) {
    request_info->file_tokens.push_back(context_token);
  }
  request_info->invocation_source =
      LensOverlayInvocationSource::kSelectionSuggestion;
  request_info->aim_entry_point = kSmartSelectionSearchEntryPoint;

  if (region_box && !screenshot.drawsNothing()) {
    auto client_logs = base::MakeRefCounted<RefCountedLensOverlayClientLogs>();
    if (auto crop_and_bitmap = DownscaleAndEncodeBitmapRegionIfNeeded(
            screenshot, std::move(region_box), /*region_bytes=*/std::nullopt,
            client_logs)) {
      request_info->image_crop = std::move(crop_and_bitmap->image_crop);
      request_info->client_logs = client_logs->client_logs();
      request_info->lens_overlay_selection_type =
          prompt.empty() ? REGION_SEARCH : MULTIMODAL_SEARCH;
    }
  }

  const bool has_text = !prompt.empty();
  const bool has_image = request_info->image_crop.has_value();
  AppendLensOverlaySidePanelParams(request_info->additional_params,
                                   base::RandUint64(), has_text, has_image);
  return request_info;
}

::selection::mojom::PromptEndpointId GoogleSearchPromptEndpoint::GetId() const {
  return ::selection::mojom::PromptEndpointId::kGoogleSearch;
}

bool GoogleSearchPromptEndpoint::IsAvailable(tabs::TabInterface& tab) const {
  if (!base::FeatureList::IsEnabled(kSmartSelectionGoogleSearch)) {
    return false;
  }
  Profile* profile = tab.GetProfile();
  CHECK(profile);
  BrowserWindowInterface* bwi = tab.GetBrowserWindowInterface();
  if (!bwi) {
    return false;
  }
  if (!contextual_tasks::ContextualTasksPanelController::From(bwi)) {
    return false;
  }
  if (!search::DefaultSearchProviderIsGoogle(profile)) {
    return false;
  }
  PrefService* prefs = profile->GetPrefs();
  if (!prefs ||
      !contextual_search::ContextualSearchService::IsContextSharingEnabled(
          prefs)) {
    return false;
  }
  auto* eligibility_manager =
      contextual_tasks::ContextualTasksEligibilityManager::GetForProfile(
          profile);
  if (!eligibility_manager || !eligibility_manager->IsSidePanelAvailable()) {
    return false;
  }
  return ContextualSearchServiceFactory::GetForProfile(profile) != nullptr &&
         contextual_tasks::ContextualTasksUiServiceFactory::
                 GetForBrowserContext(profile) != nullptr;
}

std::u16string GoogleSearchPromptEndpoint::GetDisplayName() const {
  return l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_ENDPOINT_GOOGLE_SEARCH);
}

std::u16string GoogleSearchPromptEndpoint::GetPlaceholder() const {
  return l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_ASK_GOOGLE_PLACEHOLDER);
}

void GoogleSearchPromptEndpoint::Prepare(tabs::TabInterface& tab) {
  Profile* profile = tab.GetProfile();
  CHECK(profile);
  if (auto* ui_service = contextual_tasks::ContextualTasksUiServiceFactory::
          GetForBrowserContext(profile)) {
    ui_service->EnsureCookiesSynced();
  }
}

void GoogleSearchPromptEndpoint::CancelPendingRequests() {
  weak_factory_.InvalidateWeakPtrs();
  pending_session_handle_.reset();
  ClosePendingGhostLoaderIfNeeded(
      std::exchange(pending_ghost_loader_tab_, nullptr),
      std::exchange(pending_ghost_loader_task_id_, std::nullopt));
}

// TODO(b/587425707): Deduplicate the ContextualTasks session creation,
// context upload, ghost-loader initialization, and search URL flow with
// `LensQueryFlowRouter::SendInteractionToContextualTasks` so the two paths do
// not drift.
void GoogleSearchPromptEndpoint::Submit(
    tabs::TabInterface& tab,
    std::string prompt,
    ::selection::PageContextWithSelection context) {
  CancelPendingRequests();

  Profile* profile = tab.GetProfile();
  CHECK(profile);
  BrowserWindowInterface* bwi = tab.GetBrowserWindowInterface();
  if (!bwi) {
    return;
  }

  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(profile);
  auto* contextual_tasks_ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile);
  auto* panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(bwi);
  if (!contextual_search_service || !contextual_tasks_ui_service ||
      !panel_controller) {
    return;
  }

  const ::selection::PageContext& page_context = context.page_context;
  const ::selection::AreaOfInterest& area_of_interest =
      context.area_of_interest;

  mojom::CenterRotatedBoxPtr region_box = ConvertBoundsToCenterRotatedBox(
      area_of_interest.bounds, gfx::Size(page_context.screenshot.width(),
                                         page_context.screenshot.height()));

  const bool panel_already_open =
      panel_controller->GetActiveWebContents() &&
      panel_controller->IsPanelOpenForContextualTask();

  contextual_search::ContextualSearchSessionHandle* active_handle = nullptr;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle> new_handle;

  if (panel_already_open) {
    active_handle =
        panel_controller->GetContextualSearchSessionHandleForPanel();
  }

  if (!active_handle) {
    new_handle = contextual_search_service->CreateSession(
        ntp_composebox::CreateQueryControllerConfigParams(),
        contextual_search::ContextualSearchSource::kLens,
        LensOverlayInvocationSource::kSelectionSuggestion);
    if (!new_handle) {
      return;
    }
    if (!new_handle->CheckSearchContentSharingSettings(profile->GetPrefs())) {
      return;
    }
    new_handle->NotifySessionStarted();
    active_handle = new_handle.get();
  } else if (!active_handle->CheckSearchContentSharingSettings(
                 profile->GetPrefs())) {
    return;
  }

  const base::UnguessableToken context_token =
      active_handle->CreateContextToken();
  active_handle->StartTabContextUploadFlow(
      context_token, BuildContextualInputData(tab, page_context),
      GetViewportEncodingOptions());

  auto request_info = BuildSearchUrlRequestInfo(
      prompt, context_token, page_context.screenshot, std::move(region_box));

  base::WeakPtr<contextual_search::ContextualSearchSessionHandle> weak_handle =
      active_handle->AsWeakPtr();

  if (new_handle) {
    if (!panel_already_open) {
      contextual_tasks_ui_service->InitSidePanelWithGhostLoader(
          bwi, &tab, std::move(new_handle), kSmartSelectionSearchEntryPoint);
      if (tab.IsActivated()) {
        if (std::optional<contextual_tasks::ContextualTask> created_task =
                panel_controller->GetCurrentTask();
            created_task.has_value()) {
          pending_ghost_loader_tab_ = tab.GetWeakPtr();
          pending_ghost_loader_task_id_ = created_task->GetTaskId();
        }
      }
    } else {
      pending_session_handle_ = std::move(new_handle);
    }
  }

  if (!weak_handle) {
    ClosePendingGhostLoaderIfNeeded(
        std::exchange(pending_ghost_loader_tab_, nullptr),
        std::exchange(pending_ghost_loader_task_id_, std::nullopt));
    return;
  }

  weak_handle->CreateSearchUrl(
      std::move(request_info),
      base::BindOnce(&GoogleSearchPromptEndpoint::OnSearchUrlCreated,
                     weak_factory_.GetWeakPtr(), tab.GetWeakPtr()));
}

// static
void GoogleSearchPromptEndpoint::ClosePendingGhostLoaderIfNeeded(
    base::WeakPtr<tabs::TabInterface> weak_tab,
    std::optional<base::Uuid> pending_task_id) {
  if (!weak_tab || !pending_task_id.has_value()) {
    return;
  }
  // `ContextualTasksPanelController::GetCurrentTask()`, `Close()`, and
  // `DestroyClosedSidePanel()` operate on the window's active tab. Skip
  // backgrounded tabs to avoid tearing down another tab's panel.
  // TODO(b/587425707): Add a tab-scoped ghost-loader cancellation API on
  // `ContextualTasksUiService` so pending ghost loaders on backgrounded tabs
  // can also be cleaned up cleanly.
  if (!weak_tab->IsActivated()) {
    return;
  }
  Profile* profile = weak_tab->GetProfile();
  CHECK(profile);
  BrowserWindowInterface* bwi = weak_tab->GetBrowserWindowInterface();
  if (!bwi) {
    return;
  }
  auto* panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(bwi);
  auto* contextual_tasks_ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile);
  if (!panel_controller || !contextual_tasks_ui_service) {
    return;
  }
  std::optional<contextual_tasks::ContextualTask> current_task =
      panel_controller->GetCurrentTask();
  if (!current_task.has_value() ||
      current_task->GetTaskId() != *pending_task_id ||
      !contextual_tasks_ui_service->IsTaskWaitingForUrl(
          current_task->GetTaskId())) {
    return;
  }
  if (panel_controller->IsPanelOpenForContextualTask()) {
    panel_controller->Close();
  }
  contextual_tasks_ui_service->DestroyClosedSidePanel(bwi);
}

void GoogleSearchPromptEndpoint::OnSearchUrlCreated(
    base::WeakPtr<tabs::TabInterface> weak_tab,
    GURL url) {
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle = std::move(pending_session_handle_);
  base::WeakPtr<tabs::TabInterface> ghost_loader_tab =
      std::exchange(pending_ghost_loader_tab_, nullptr);
  std::optional<base::Uuid> ghost_loader_task_id =
      std::exchange(pending_ghost_loader_task_id_, std::nullopt);
  if (!weak_tab || url.is_empty()) {
    ClosePendingGhostLoaderIfNeeded(std::move(ghost_loader_tab),
                                    std::move(ghost_loader_task_id));
    return;
  }
  Profile* profile = weak_tab->GetProfile();
  CHECK(profile);
  BrowserWindowInterface* bwi = weak_tab->GetBrowserWindowInterface();
  if (!bwi) {
    ClosePendingGhostLoaderIfNeeded(std::move(ghost_loader_tab),
                                    std::move(ghost_loader_task_id));
    return;
  }
  auto* contextual_tasks_ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile);
  if (!contextual_tasks_ui_service) {
    ClosePendingGhostLoaderIfNeeded(std::move(ghost_loader_tab),
                                    std::move(ghost_loader_task_id));
    return;
  }
  contextual_tasks_ui_service->StartTaskUiInSidePanel(
      bwi, weak_tab.get(), url, std::move(session_handle),
      contextual_tasks::StartTaskUiOptions{
          .entry_point = kSmartSelectionSearchEntryPoint,
      });
}

}  // namespace lens
