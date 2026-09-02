// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include "build/build_config.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/features.h"
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
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#endif

DOCUMENT_USER_DATA_KEY_IMPL(ContextualTasksExtensionHandler);

ContextualTasksExtensionHandler::ContextualTasksExtensionHandler(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<ContextualTasksExtensionHandler>(rfh) {}

ContextualTasksExtensionHandler::~ContextualTasksExtensionHandler() = default;

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
}

// contextual_tasks::mojom::ExtensionPageHandler:
void ContextualTasksExtensionHandler::SetTaskId(const base::Uuid& uuid) {
  task_id_ = uuid;
}

void ContextualTasksExtensionHandler::OnWebviewMessage(
    const std::vector<uint8_t>& message) {
  if (!contextual_tasks_page_.is_bound()) {
    return;
  }
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    contextual_tasks_page_->OnHandshakeComplete();
  } else if (aim_to_client_message.has_hide_input()) {
    contextual_tasks_page_->HideInput();
  } else if (aim_to_client_message.has_restore_input()) {
    contextual_tasks_page_->RestoreInput();
  } else if (aim_to_client_message.has_enter_basic_mode()) {
    contextual_tasks_page_->EnterBasicMode();
  } else if (aim_to_client_message.has_exit_basic_mode()) {
    contextual_tasks_page_->ExitBasicMode();
  } else if (aim_to_client_message.has_lock_input()) {
    contextual_tasks_page_->LockInput();
  } else if (aim_to_client_message.has_unlock_input()) {
    contextual_tasks_page_->UnlockInput();
  }
}

void ContextualTasksExtensionHandler::GetHandshakeMessage(
    GetHandshakeMessageCallback callback) {
  std::move(callback).Run(
      mojo_base::ProtoWrapper(contextual_tasks::GetHandshakeMessageProto()));
}
// composebox::mojom::PageHandler stubs:
void ContextualTasksExtensionHandler::FocusChanged(bool focused) {}
void ContextualTasksExtensionHandler::StartPlatformVoiceRecognition() {}
void ContextualTasksExtensionHandler::HandleLensButtonClick() {}
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
  std::move(callback).Run({});
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
    AddFileContextCallback callback) {}
void ContextualTasksExtensionHandler::AddTabContext(
    int32_t tab_id,
    bool delay_upload,
    searchbox::mojom::TabAttachmentSource source,
    AddTabContextCallback callback) {}
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
    OnDriveUploadClickedCallback callback) {}
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

content::WebContents* ContextualTasksExtensionHandler::GetActiveTabWebContents()
    const {
  content::WebContents* host_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!host_contents) {
    return nullptr;
  }
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(host_contents);
  if (!browser_window_interface) {
    return nullptr;
  }
  auto* active_tab = browser_window_interface->GetActiveTabInterface();
  if (!active_tab) {
    return nullptr;
  }
  return active_tab->GetContents();
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

  content::WebContents* active_tab_contents = GetActiveTabWebContents();
  if (!active_tab_contents) {
    return std::nullopt;
  }
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab_contents);
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
  content::WebContents* active_tab_contents = GetActiveTabWebContents();
  if (!active_tab_contents) {
    return nullptr;
  }
  return LensSearchController::FromTabWebContents(active_tab_contents);
}
#endif

void ContextualTasksExtensionHandler::InitializeInputStateModel() {
  input_state_model_ = GetOrCreateInputStateModel();
  if (!input_state_model_) {
    return;
  }

  content::WebContents* active_tab_contents = GetActiveTabWebContents();
  if (active_tab_contents) {
    Profile* profile =
        Profile::FromBrowserContext(active_tab_contents->GetBrowserContext());
    if (profile) {
      input_state_model_->SetPrefService(profile->GetPrefs());
    }
  }

  input_state_subscription_ = input_state_model_->subscribe(
      base::BindRepeating(&ContextualTasksExtensionHandler::OnInputStateChanged,
                          base::Unretained(this)));
  input_state_model_->Initialize();
}

void ContextualTasksExtensionHandler::OnInputStateChanged(
    const omnibox::InputState& state) {
  if (searchbox_page_) {
    searchbox_page_->OnInputStateChanged(state);
  }
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksExtensionHandler::GetOrCreateInputStateModel() {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return nullptr;
  }
  content::WebContents* active_tab_contents = GetActiveTabWebContents();
  if (!active_tab_contents) {
    return nullptr;
  }
  auto* user_data =
      contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
          active_tab_contents);
  if (!user_data) {
    contextual_tasks::ContextualTasksWebContentsUserData::CreateForWebContents(
        active_tab_contents);
    user_data =
        contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
            active_tab_contents);
  }
  return user_data->GetOrCreateInputStateModel(*session_handle);
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
