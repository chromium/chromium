// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/common/input_state.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
lens::LensOverlayVisualSearchInteractionData CreateCropInteractionData(
    const lens::mojom::CenterRotatedBoxPtr& region,
    const SkBitmap& screenshot) {
  lens::LensOverlayVisualSearchInteractionData vsint;
  vsint.set_interaction_type(
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  auto* mutable_zoomed_crop = vsint.mutable_zoomed_crop();
  mutable_zoomed_crop->set_parent_height(screenshot.height());
  mutable_zoomed_crop->set_parent_width(screenshot.width());
  mutable_zoomed_crop->set_zoom(1.0);
  auto* crop = mutable_zoomed_crop->mutable_crop();
  crop->set_coordinate_type(lens::CoordinateType::NORMALIZED);
  if (region->coordinate_type ==
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized) {
    crop->set_center_x(region->box.x());
    crop->set_center_y(region->box.y());
    crop->set_width(region->box.width());
    crop->set_height(region->box.height());
  } else {
    crop->set_center_x(region->box.x() / screenshot.width());
    crop->set_center_y(region->box.y() / screenshot.height());
    crop->set_width(region->box.width() / screenshot.width());
    crop->set_height(region->box.height() / screenshot.height());
  }
  vsint.mutable_log_data()->mutable_filter_data()->set_filter_type(
      lens::AUTO_FILTER);
  vsint.mutable_log_data()->mutable_user_selection_data()->set_selection_type(
      lens::MULTIMODAL_SEARCH);
  vsint.mutable_log_data()->set_is_parent_query(true);
  vsint.mutable_log_data()->set_client_platform(
      lens::CLIENT_PLATFORM_LENS_OVERLAY);
  return vsint;
}
#endif

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(ContextualTasksExtensionHandler);

ContextualTasksExtensionHandler::ContextualTasksExtensionHandler(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<ContextualTasksExtensionHandler>(rfh) {
  if (auto* browser_context = rfh->GetBrowserContext()) {
    if (auto* ui_service = contextual_tasks::ContextualTasksUiServiceFactory::
            GetForBrowserContext(browser_context)) {
      ui_service_observation_.Observe(ui_service);
    }
  }
}

ContextualTasksExtensionHandler::~ContextualTasksExtensionHandler() = default;

void ContextualTasksExtensionHandler::OnLensOverlayStateChanged(
    bool is_showing) {
  if (contextual_tasks_page_) {
    contextual_tasks_page_->OnLensOverlayStateChanged(is_showing);
  }
  if (!is_showing) {
    auto model = GetOrCreateInputStateModel();
    if (model) {
      // RemoveLensCrop() notifies observers via OnInputStateChanged, which
      // emits the reverse-sync unmount message (InjectChromeInput with
      // is_active: false) to AIM to remove the chip.
      model->RemoveLensCrop();
    }
  }
}

void ContextualTasksExtensionHandler::OnPermissionPromptChanged(
    bool is_showing,
    const gfx::Size& prompt_size) {
  if (searchbox_page_) {
    searchbox_page_->OnPermissionPromptChanged(is_showing, prompt_size);
  }
}

void ContextualTasksExtensionHandler::BindComposeboxFactory(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  composebox_factory_receiver_.reset();
  composebox_factory_receiver_.Bind(std::move(receiver));
}

// composebox::mojom::PageHandlerFactory:
void ContextualTasksExtensionHandler::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> receiver,
    mojo::PendingRemote<searchbox::mojom::Page> searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_handler) {
  composebox_handler_receiver_.reset();
  composebox_handler_receiver_.Bind(std::move(receiver));

  searchbox_page_.reset();
  searchbox_page_.Bind(std::move(searchbox_page));
  searchbox_handler_receiver_.reset();
  searchbox_handler_receiver_.Bind(std::move(searchbox_handler));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (web_contents) {
    PermissionPromptObserver::CreateForWebContents(web_contents);
    if (auto* observer =
            PermissionPromptObserver::FromWebContents(web_contents)) {
      permission_prompt_observation_.Reset();
      permission_prompt_observation_.Observe(observer);
    }
  }

  InitializeInputStateModel();
}

void ContextualTasksExtensionHandler::BindContextualTasksFactory(
    mojo::PendingReceiver<contextual_tasks::mojom::ExtensionPageHandlerFactory>
        receiver) {
  contextual_tasks_factory_receiver_.reset();
  contextual_tasks_factory_receiver_.Bind(std::move(receiver));
}

// contextual_tasks::mojom::ExtensionPageHandlerFactory:
void ContextualTasksExtensionHandler::CreateExtensionPageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::ExtensionPage> page,
    mojo::PendingReceiver<contextual_tasks::mojom::ExtensionPageHandler>
        receiver) {
  contextual_tasks_handler_receiver_.reset();
  contextual_tasks_handler_receiver_.Bind(std::move(receiver));
  contextual_tasks_page_.reset();
  contextual_tasks_page_.Bind(std::move(page));
  InitializeInputStateModel();
}

// contextual_tasks::mojom::ExtensionPageHandler:
void ContextualTasksExtensionHandler::SetTaskId(const base::Uuid& uuid) {
  task_id_ = uuid;
  GetOrCreateInputStateModel();
}

void ContextualTasksExtensionHandler::OnWebviewMessage(
    const std::vector<uint8_t>& message) {
  if (!contextual_tasks_page_.is_bound()) {
    return;
  }
  constexpr size_t kMaxWebviewMessageBytes = 1024 * 1024;
  if (message.size() > kMaxWebviewMessageBytes) {
    return;
  }

  // Try parsing SearchToClientMessage first (Search in Chrome protocol).
  lens::SearchToClientMessage search_to_client_message;
  if (search_to_client_message.ParseFromArray(message.data(), message.size())) {
    if (search_to_client_message.has_handshake_response()) {
      size_t auth_user_index = static_cast<size_t>(std::max(
          0, search_to_client_message.handshake_response().auth_user_index()));
      if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
        session_handle->set_auth_user_index(auth_user_index);
      }
      contextual_tasks_page_->OnHandshakeComplete();
      RecordTimeToHandshakeComplete();
      return;
    }
    if (search_to_client_message.has_on_submit_query_request()) {
      HandleOnSubmitQueryRequest();
      return;
    }
  }

  // Fall back to legacy AimToClientMessage.
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    contextual_tasks_page_->OnHandshakeComplete();
    RecordTimeToHandshakeComplete();
  }
}

void ContextualTasksExtensionHandler::RecordTimeToHandshakeComplete() {
  if (auto* wc =
          content::WebContents::FromRenderFrameHost(&render_frame_host())) {
    if (auto* browser = webui::GetBrowserWindowInterface(wc)) {
      if (auto* panel_controller =
              contextual_tasks::ContextualTasksPanelController::From(browser)) {
        panel_controller->RecordTimeToHandshakeComplete(wc);
      }
    }
  }
}

void ContextualTasksExtensionHandler::HandleOnSubmitQueryRequest() {
  lens::ClientToSearchMessage response_message;
  auto* submit_response = response_message.mutable_on_submit_query_response();

  if (auto lens_added_context = GetLensAddedContext()) {
    *submit_response->add_added_contexts() = std::move(*lens_added_context);
  }

  PostSearchMessage(response_message);
}

std::optional<lens::AddedContext>
ContextualTasksExtensionHandler::GetLensAddedContext() {
#if !BUILDFLAG(IS_ANDROID)
  auto model = GetOrCreateInputStateModel();
  if (!model || !model->lens_crop().has_value()) {
    return std::nullopt;
  }

  auto* controller = GetLensSearchController();
  if (!controller || !controller->IsCurrentTabSameOrigin()) {
    return std::nullopt;
  }

  auto* overlay = controller->lens_overlay_controller();
  auto* query_controller = controller->lens_overlay_query_controller();
  if (!overlay || !overlay->HasRegionSelection() || !query_controller) {
    return std::nullopt;
  }

  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return std::nullopt;
  }

  // Identify the context file corresponding to the region crop across uploaded
  // and submitted context files.
  std::optional<base::UnguessableToken> overlay_token = GetLensOverlayToken();
  const contextual_search::FileInfo* file_info = nullptr;
  std::vector<contextual_search::FileInfo> uploaded_files =
      session_handle->GetUploadedContextFileInfos();
  std::vector<contextual_search::FileInfo> submitted_files =
      session_handle->GetSubmittedContextFileInfos();
  uploaded_files.insert(uploaded_files.end(), submitted_files.begin(),
                        submitted_files.end());

  if (overlay_token.has_value()) {
    for (const auto& info : uploaded_files) {
      if (info.file_token == *overlay_token) {
        file_info = &info;
        break;
      }
    }
  }
  if (!file_info) {
    for (const auto& info : uploaded_files) {
      if (info.is_implicit_upload && info.input_data &&
          info.input_data->upload_type ==
              lens::LensOverlayContextualInputUploadType::
                  CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY) {
        file_info = &info;
        break;
      }
    }
  }

  if (!file_info || !file_info->request_id.has_value()) {
    return std::nullopt;
  }

  if (!overlay_token.has_value()) {
    overlay_token = file_info->file_token;
  }

  lens::AddedContext added;

  // Search session ID.
  std::string search_session_id = session_handle->search_session_id();
  if (search_session_id.empty()) {
    search_session_id = query_controller->search_session_id();
  }
  added.set_search_session_id(search_session_id);

  // Request ID.
  *added.mutable_request_id() = *file_info->request_id;
  added.mutable_request_id()->set_media_type(
      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);

  // Visual Search Interaction Data.
  std::optional<lens::LensOverlayVisualSearchInteractionData>
      visual_search_interaction_data;
  if (overlay_token.has_value()) {
    visual_search_interaction_data =
        session_handle->GetVisualSearchInteractionData(*overlay_token,
                                                       std::nullopt);
  }
  if (!visual_search_interaction_data.has_value()) {
    visual_search_interaction_data =
        query_controller->GetVisualSearchInteractionData();
  }
  if (!visual_search_interaction_data.has_value()) {
    visual_search_interaction_data = CreateCropInteractionData(
        overlay->selected_region(), overlay->initial_screenshot());
  }
  if (visual_search_interaction_data.has_value()) {
    *added.mutable_visual_search_interaction_data() =
        std::move(*visual_search_interaction_data);
  }

  // Contextual input upload type.
  lens::LensOverlayContextualInputUploadType upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;
  if (file_info->input_data && file_info->input_data->upload_type.has_value()) {
    upload_type = file_info->input_data->upload_type.value();
  }
  added.set_contextual_input_upload_type(upload_type);

  if (!added.has_request_id()) {
    return std::nullopt;
  }

  return added;
#else
  return std::nullopt;
#endif
}

void ContextualTasksExtensionHandler::GetHandshakeMessage(
    GetHandshakeMessageCallback callback) {
  std::move(callback).Run(
      mojo_base::ProtoWrapper(contextual_tasks::GetHandshakeMessageProto()));
}

void ContextualTasksExtensionHandler::GetLensCropPreview(
    GetLensCropPreviewCallback callback) {
  auto model = GetOrCreateInputStateModel();
  if (!model) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(model->GetLensCrop());
}

void ContextualTasksExtensionHandler::RemoveLensCrop() {
  auto model = GetOrCreateInputStateModel();
  if (!model || !model->lens_crop().has_value()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    if (auto* overlay = controller->lens_overlay_controller()) {
      overlay->ClearRegionSelection();
    }
    controller->CloseLensAsync(
        lens::LensOverlayDismissalSource::kContextualTasksLensChipRemoved);
  }
#endif

  model->RemoveLensCrop();
}

void ContextualTasksExtensionHandler::OnLensThumbnailCreated(
    const std::string& thumbnail_uri) {
  auto model = GetOrCreateInputStateModel();
  if (!model) {
    return;
  }
  model->SetLensCrop(thumbnail_uri);
}

// composebox::mojom::PageHandler stubs:
void ContextualTasksExtensionHandler::FocusChanged(bool focused) {}
void ContextualTasksExtensionHandler::StartPlatformVoiceRecognition() {}
void ContextualTasksExtensionHandler::HandleLensButtonClick() {
#if !BUILDFLAG(IS_ANDROID)
  base::RecordAction(base::UserMetricsAction(
      "ContextualTasks.Composebox.UserAction.LensButtonClicked"));

  if (auto* controller = GetLensSearchController()) {
    controller->SetThumbnailCreatedCallback(base::BindRepeating(
        &ContextualTasksExtensionHandler::OnLensThumbnailCreated,
        weak_ptr_factory_.GetWeakPtr()));
    if (controller->IsShowingUI()) {
      if (controller->invocation_source() ==
          lens::LensOverlayInvocationSource::kContextualTasksComposebox) {
        controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::
                kContextualTasksComposeboxLensButtonClick);
        return;
      } else {
        // If the overlay is showing from a different invocation source, clear
        // the selection and start fresh for a follow-up.
        if (controller->lens_overlay_controller()) {
          controller->lens_overlay_controller()->ClearAllSelections();
        }
        // Set the invocation source to contextual tasks so that any follow-up
        // queries are associated with the contextual tasks session via the
        // query flow router and thumbnails are added appropriately to the
        // composebox. This will work as if the overlay was opened from the
        // contextual tasks composebox in the first place.
        controller->SetInvocationSource(
            lens::LensOverlayInvocationSource::kContextualTasksComposebox);
      }
    }
    controller->OpenLensOverlay(
        lens::LensOverlayInvocationSource::kContextualTasksComposebox);
  }
#endif
}
void ContextualTasksExtensionHandler::HandleFileUpload(bool is_image) {}
void ContextualTasksExtensionHandler::NavigateUrl(const GURL& url) {}
void ContextualTasksExtensionHandler::CloseLensOverlayFromWebUI(
    composebox::mojom::LensOverlayDismissalSource dismissal_source) {}
void ContextualTasksExtensionHandler::SetSmartTabSharingActive(bool active) {}
void ContextualTasksExtensionHandler::GetSmartTabSharingActive(
    GetSmartTabSharingActiveCallback callback) {
  std::move(callback).Run(false);
}
void ContextualTasksExtensionHandler::
    NotifyComposeboxQuerySubmittedWithContext() {}
void ContextualTasksExtensionHandler::CanShowNextboxAnimation(
    CanShowNextboxAnimationCallback callback) {
  std::move(callback).Run(false);
}
void ContextualTasksExtensionHandler::RecordNextboxAnimationImpression(
    bool shown) {}
void ContextualTasksExtensionHandler::OnContextMenuOpened() {}

// searchbox::mojom::PageHandler stubs:
void ContextualTasksExtensionHandler::OnFocusChanged(bool focused) {}
void ContextualTasksExtensionHandler::QueryAutocomplete(
    int32_t query_id,
    std::optional<int32_t> tab_id,
    const std::u16string& input,
    bool prevent_inline_autocomplete,
    uint32_t cursor_position,
    omnibox::SuggestInventory suggest_inventory,
    bool is_on_focus,
    const std::string& keyword,
    searchbox::mojom::InputMethod input_method) {
  DCHECK(!tab_id.has_value())
      << "QueryAutocomplete with tab_id is only supported for the full WebUI "
         "Omnibox.";
}
void ContextualTasksExtensionHandler::StopAutocomplete(bool clear_result) {}
void ContextualTasksExtensionHandler::OpenAutocompleteMatch(
    uint32_t result_sequence_id,
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    uint8_t mouse_button,
    searchbox::mojom::ActionModifiersPtr modifiers,
    bool via_keyboard) {
  NavigateUrl(url);
}
void ContextualTasksExtensionHandler::SetSmartComposeStats(
    searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) {}
void ContextualTasksExtensionHandler::SetPopupSelection(
    searchbox::mojom::OmniboxPopupSelectionPtr selection) {}
void ContextualTasksExtensionHandler::OpenPopupSelection(
    uint32_t result_sequence_id,
    searchbox::mojom::OmniboxPopupSelectionPtr selection,
    WindowOpenDisposition disposition) {}
void ContextualTasksExtensionHandler::OnNavigationLikely(
    uint8_t line,
    const GURL& url,
    omnibox::mojom::NavigationPredictor navigation_predictor) {}
void ContextualTasksExtensionHandler::DeleteAutocompleteMatch(uint8_t line,
                                                              const GURL& url) {
}
void ContextualTasksExtensionHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {}
void ContextualTasksExtensionHandler::ExecuteAction(
    uint8_t line,
    uint8_t action_index,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {}
void ContextualTasksExtensionHandler::GetCyclingPlaceholderConfig(
    GetCyclingPlaceholderConfigCallback callback) {
  std::move(callback).Run(nullptr);
}
void ContextualTasksExtensionHandler::GetRecentTabs(
    GetRecentTabsCallback callback) {
  std::move(callback).Run(ContextualSearchboxHandler::GetRecentTabInfos(
      GetBrowserWindowInterface()));
}
void ContextualTasksExtensionHandler::GetTabPreview(
    int32_t tab_id,
    GetTabPreviewCallback callback) {
  std::move(callback).Run("");
}
void ContextualTasksExtensionHandler::WaitForTabFaviconLoad(
    int32_t tab_id,
    WaitForTabFaviconLoadCallback callback) {
  std::move(callback).Run(std::nullopt);
}
void ContextualTasksExtensionHandler::GetInputState(
    GetInputStateCallback callback) {
  if (!input_state_model_) {
    InitializeInputStateModel();
  }
  if (input_state_model_) {
    std::move(callback).Run(input_state_model_->GetInputState());
  } else {
    std::move(callback).Run(std::nullopt);
  }
}
void ContextualTasksExtensionHandler::NotifySessionStarted() {}
void ContextualTasksExtensionHandler::NotifySessionAbandoned() {}
void ContextualTasksExtensionHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  std::move(callback).Run(base::unexpected(
      contextual_search::ContextUploadErrorType::kBrowserProcessingError));
}
void ContextualTasksExtensionHandler::AddTabContext(
    int32_t tab_id,
    bool delay_upload,
    searchbox::mojom::TabAttachmentSource source,
    AddTabContextCallback callback) {
  std::move(callback).Run(base::ok(base::UnguessableToken::Create()));
}
void ContextualTasksExtensionHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {}
void ContextualTasksExtensionHandler::DeleteTabContext(int32_t tab_id) {}
void ContextualTasksExtensionHandler::ClearFiles(
    bool should_block_auto_suggested_tabs) {}
void ContextualTasksExtensionHandler::SubmitQuery(const std::string& query_text,
                                                  uint8_t mouse_button,
                                                  bool alt_key,
                                                  bool ctrl_key,
                                                  bool meta_key,
                                                  bool shift_key,
                                                  bool is_voice_search) {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return;
  }

  std::optional<base::UnguessableToken> overlay_token = GetLensOverlayToken();

#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    controller->CloseLensSync(
        lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
  }
#endif

  auto request_info = contextual_tasks::PrepareClientToAimRequestInfo(
      query_text, session_handle, this, active_tool_, active_model_,
      GetActiveTabContextId(), overlay_token, is_voice_search);

  contextual_tasks::FinalizeAndSendAimQuery(std::move(request_info),
                                            session_handle, this);
}
void ContextualTasksExtensionHandler::OpenLensSearch() {}
void ContextualTasksExtensionHandler::SetActiveToolMode(omnibox::ToolMode tool,
                                                        bool is_set_by_aim) {
  active_tool_ = tool;
}
void ContextualTasksExtensionHandler::RecordToolSelectionAction(
    omnibox::ToolMode tool) {}
void ContextualTasksExtensionHandler::SetActiveModelMode(
    omnibox::ModelMode model,
    bool is_set_by_aim) {
  active_model_ = model;
}
void ContextualTasksExtensionHandler::RecordModelSelectionAction(
    omnibox::ModelMode model) {}
void ContextualTasksExtensionHandler::ActivateMetricsFunnel(
    const std::string& funnel_name) {}
void ContextualTasksExtensionHandler::GetDriveDisclaimerStatus(
    GetDriveDisclaimerStatusCallback callback) {
  std::move(callback).Run(
      searchbox::mojom::DriveDisclaimerStatus::kNotAccepted);
}
void ContextualTasksExtensionHandler::OnDriveDisclaimerAccepted() {}
void ContextualTasksExtensionHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {
  NOTREACHED();
}
void ContextualTasksExtensionHandler::OpenProfilePicker() {}
void ContextualTasksExtensionHandler::ShowScreenshotMenu(
    const gfx::Rect& anchor_rect) {}
void ContextualTasksExtensionHandler::GetPageClassification(
    GetPageClassificationCallback callback) {
  std::move(callback).Run("INVALID_SPEC");
}
void ContextualTasksExtensionHandler::OnThumbnailRemoved() {}

void ContextualTasksExtensionHandler::PostAimMessage(
    const lens::ClientToAimMessage& message) {
  // Route the message directly to the extension page's bound remote.
  if (contextual_tasks_page_.is_bound()) {
    const size_t size = message.ByteSizeLong();
    std::vector<uint8_t> serialized_message(size);
    message.SerializeToArray(serialized_message.data(), size);
    contextual_tasks_page_->PostAimMessage(std::move(serialized_message));
  }
}

void ContextualTasksExtensionHandler::PostSearchMessage(
    const lens::ClientToSearchMessage& message) {
  // Route the search message directly to the extension page's bound remote.
  if (contextual_tasks_page_.is_bound()) {
    contextual_tasks_page_->PostSearchMessage(mojo_base::ProtoWrapper(message));
  }
}

BrowserWindowInterface*
ContextualTasksExtensionHandler::GetBrowserWindowInterface() const {
  content::WebContents* host_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!host_contents) {
    return nullptr;
  }
  // Retrieve the browser window via TabInterface when hosted in a tab, or via
  // WebUI embedding context when hosted in the side panel.
  auto* tab = tabs::TabInterface::MaybeGetFromContents(host_contents);
  return tab ? tab->GetBrowserWindowInterface()
             : webui::GetBrowserWindowInterface(host_contents);
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksExtensionHandler::GetOrCreateContextualSessionHandle() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return nullptr;
  }

  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  contextual_search::ContextualSearchSessionHandle* existing_session =
      task_id_.has_value() ? helper->GetSessionForTask(task_id_.value())
                           : helper->session_handle();
  if (existing_session) {
    return existing_session;
  }

  if (!task_id_) {
    auto* browser_context = render_frame_host().GetBrowserContext();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile);
    if (contextual_search_service) {
      auto session_handle = contextual_search_service->CreateSession(
          contextual_tasks::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kContextualTasks,
          lens::LensOverlayInvocationSource::kContextualTasksComposebox);
      session_handle->CheckSearchContentSharingSettings(profile->GetPrefs());
      helper->SetTaskSession(std::nullopt, std::move(session_handle),
                             /*input_state_model=*/nullptr);
      return helper->session_handle();
    }
  }

  return existing_session;
}

std::optional<int64_t>
ContextualTasksExtensionHandler::GetActiveTabContextId() {
  auto* contextual_session_handle = GetOrCreateContextualSessionHandle();
  if (!contextual_session_handle) {
    return std::nullopt;
  }

  auto* browser_window_interface = GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return std::nullopt;
  }
  auto* active_tab = browser_window_interface->GetActiveTabInterface();
  if (!active_tab || !active_tab->GetContents()) {
    return std::nullopt;
  }
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  if (!active_tab_id.is_valid()) {
    return std::nullopt;
  }

  auto file_infos = contextual_session_handle->GetUploadedContextFileInfos();
  auto submitted_file_infos =
      contextual_session_handle->GetSubmittedContextFileInfos();
  file_infos.insert(file_infos.end(), submitted_file_infos.begin(),
                    submitted_file_infos.end());
  for (const auto& file_info : file_infos) {
    if (file_info.tab_session_id &&
        file_info.tab_session_id->id() == active_tab_id.id()) {
      return file_info.GetContextId();
    }
  }
  return std::nullopt;
}

std::optional<base::UnguessableToken>
ContextualTasksExtensionHandler::GetLensOverlayToken() {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    auto* overlay = controller->lens_overlay_controller();
    if (!overlay || !overlay->HasRegionSelection()) {
      return std::nullopt;
    }
    if (auto* router = controller->query_router()) {
      return router->overlay_tab_context_file_token();
    }
  }
#endif
  return std::nullopt;
}

#if !BUILDFLAG(IS_ANDROID)
LensSearchController* ContextualTasksExtensionHandler::GetLensSearchController()
    const {
  auto* browser_window_interface = GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return nullptr;
  }
  auto* active_tab = browser_window_interface->GetActiveTabInterface();
  if (!active_tab || !active_tab->GetContents()) {
    return nullptr;
  }
  return LensSearchController::FromTabWebContents(active_tab->GetContents());
}
#endif

void ContextualTasksExtensionHandler::InitializeInputStateModel() {
  input_state_model_ = nullptr;
  is_lens_crop_mounted_ = false;
  auto model = GetOrCreateInputStateModel();
  if (model) {
    auto* browser_context = render_frame_host().GetBrowserContext();
    if (auto* profile = Profile::FromBrowserContext(browser_context)) {
      model->SetPrefService(profile->GetPrefs());
    }
    model->Initialize();
  }
}

void ContextualTasksExtensionHandler::OnInputStateChanged(
    const omnibox::InputState& state) {
  if (searchbox_page_) {
    searchbox_page_->OnInputStateChanged(state);
  }

  // Reverse sync and state propagation to AIM via PostSearchMessage.
  // Only the frame hosting lens_button (which relays messages up to AIM via
  // window.parent.postMessage) should emit search communication messages.
  if (!contextual_tasks_page_.is_bound() || !input_state_model_) {
    return;
  }

  // TODO(crbug.com/549306496): Ensure only one message is sent per page via one
  // of the extension iframes if lens_button is not present.
  if (render_frame_host().GetLastCommittedURL().ExtractFileName() ==
      "lens_chip.html") {
    return;
  }

  const auto& crop = input_state_model_->lens_crop();
  bool has_crop = crop.has_value();

  if (is_lens_crop_mounted_ != has_crop) {
    lens::ClientToSearchMessage search_message;
    auto* inject_input = search_message.mutable_inject_chrome_input();
    inject_input->set_input_type(
        lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
    inject_input->set_is_active(has_crop);
    PostSearchMessage(search_message);

    is_lens_crop_mounted_ = has_crop;
  }
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksExtensionHandler::GetOrCreateInputStateModel() {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return nullptr;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return nullptr;
  }
  auto* user_data =
      contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
          web_contents);
  if (!user_data) {
    contextual_tasks::ContextualTasksWebContentsUserData::CreateForWebContents(
        web_contents);
    user_data =
        contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
            web_contents);
  }
  auto model = user_data->GetOrCreateInputStateModel(*session_handle);
  if (input_state_model_.get() != model.get()) {
    input_state_model_ = model;
    is_lens_crop_mounted_ = false;
    if (model && render_frame_host().GetLastCommittedURL().ExtractFileName() !=
                     "lens_chip.html") {
      input_state_subscription_ =
          input_state_model_->subscribe(base::BindRepeating(
              &ContextualTasksExtensionHandler::OnInputStateChanged,
              base::Unretained(this)));
    } else {
      input_state_subscription_ = {};
    }
  }
  return input_state_model_;
}

void ContextualTasksExtensionHandler::StartScreenshare(
    bool prefer_entire_screen,
    StartScreenshareCallback callback) {
  NOTREACHED();
}

void ContextualTasksExtensionHandler::CaptureRegionScreenshot(
    CaptureRegionScreenshotCallback callback) {
  NOTREACHED();
}

void ContextualTasksExtensionHandler::ShowHotkeyDropdown(
    const gfx::Rect& anchor_bounds,
    ShowHotkeyDropdownCallback callback) {
  NOTREACHED();
}
