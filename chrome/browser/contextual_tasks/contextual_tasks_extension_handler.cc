// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/common/input_state.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
void DeleteTabToken(
    contextual_search::ContextualSearchSessionHandle* session_handle,
    const base::UnguessableToken& token) {
  if (!session_handle) {
    return;
  }
  if (!session_handle->DeleteFile(token)) {
    session_handle->RemoveUploadedContextToken(token);
  }
}
#endif

searchbox::mojom::TabInfoPtr CreateMojomTab(
    const contextual_search::TabInfo& tab) {
  auto mojom_tab = searchbox::mojom::TabInfo::New();
  mojom_tab->tab_id = tab.tab_id.value_or(0);
  mojom_tab->title = tab.title;
  mojom_tab->url = tab.url;
  return mojom_tab;
}

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(ContextualTasksExtensionHandler);

ContextualTasksExtensionHandler::ContextualTasksExtensionHandler(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<ContextualTasksExtensionHandler>(rfh) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->RegisterExtensionFrame(this);
  }
}

ContextualTasksExtensionHandler::~ContextualTasksExtensionHandler() {
  if (auto* web_contents =
          content::WebContents::FromRenderFrameHost(&render_frame_host())) {
    if (auto* user_data = contextual_tasks::ContextualTasksWebContentsUserData::
            FromWebContents(web_contents)) {
      user_data->UnregisterExtensionFrame(this);
    }
  }
}

// static
ContextualTasksExtensionHandler*
ContextualTasksExtensionHandler::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  ContextualTasksExtensionHandler* target_handler = nullptr;
  web_contents->ForEachRenderFrameHostWithAction(
      [&target_handler](content::RenderFrameHost* rfh) {
        if (auto* handler =
                ContextualTasksExtensionHandler::GetForCurrentDocument(rfh)) {
          if (handler->contextual_tasks_page_.is_bound()) {
            target_handler = handler;
            return content::RenderFrameHost::FrameIterationAction::kStop;
          }
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return target_handler;
}

contextual_tasks::ContextualTasksWebContentsUserData*
ContextualTasksExtensionHandler::GetOrCreateWebContentsUserData() const {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return nullptr;
  }
  return contextual_tasks::ContextualTasksWebContentsUserData::
      GetOrCreateForWebContents(web_contents);
}

void ContextualTasksExtensionHandler::OnLensOverlayStateChanged(
    bool is_showing) {
  if (contextual_tasks_page_) {
    contextual_tasks_page_->OnLensOverlayStateChanged(is_showing);
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
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->UpdateExtensionFrameBound(
        this, /*is_page_bound=*/true,
        base::BindRepeating(
            &ContextualTasksExtensionHandler::SendSearchMessageToBoundPage,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(
            &ContextualTasksExtensionHandler::OnHandshakeComplete,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&ContextualTasksExtensionHandler::OnLensCropUpdated,
                            weak_ptr_factory_.GetWeakPtr()));
  }
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    if (controller->IsShowingUI()) {
      contextual_tasks_page_->OnLensOverlayStateChanged(/*is_showing=*/true);
    }
  }
#endif
  InitializeInputStateModel();
  // Do not reset the session's tab context here. The session is shared by every
  // extension frame in this WebContents, so a frame binding in the middle of a
  // session (e.g. the context library, which mounts right after a tab is
  // attached) would drop the tabs unexpectedly. Instead, send the page any
  // tabs added before it subscribed, since subscribing does not load initial
  // state.
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    const auto& tab_context = session_handle->GetTabContextState();
    if (!tab_context.attached.empty() || !tab_context.restored.empty()) {
      SendTabContextToExtensionPage(tab_context);
    }
  }
}

// contextual_tasks::mojom::ExtensionPageHandler:
void ContextualTasksExtensionHandler::SetTaskId(const base::Uuid& uuid) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->SetTaskId(uuid);
  }
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

  // Try parsing SearchToClientMessage first via
  // ContextualTasksWebContentsUserData.
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    if (user_data->OnSearchMessageReceived(message)) {
      return;
    }
  }

  // Fall back to legacy AimToClientMessage.
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    OnHandshakeComplete();
    RecordTimeToHandshakeComplete();
  } else if (aim_to_client_message.has_open_link_in_side_panel_mode()) {
    if (auto* user_data = GetOrCreateWebContentsUserData()) {
      user_data->HandleOpenLinkInSidePanelMode(
          aim_to_client_message.open_link_in_side_panel_mode().url());
    }
  }
}

void ContextualTasksExtensionHandler::RecordTimeToHandshakeComplete() {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* wc =
          content::WebContents::FromRenderFrameHost(&render_frame_host())) {
    if (auto* browser = webui::GetBrowserWindowInterface(wc)) {
      if (auto* panel_controller =
              contextual_tasks::ContextualTasksPanelController::From(browser)) {
        panel_controller->RecordTimeToHandshakeComplete(wc);
      }
    }
  }
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
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->RemoveLensCrop();
  }
}

void ContextualTasksExtensionHandler::OnLensThumbnailCreated(
    const std::string& thumbnail_uri) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->OnLensThumbnailCreated(thumbnail_uri);
  }
}

// composebox::mojom::PageHandler stubs:
void ContextualTasksExtensionHandler::FocusChanged(bool focused) {}
void ContextualTasksExtensionHandler::StartPlatformVoiceRecognition() {}
void ContextualTasksExtensionHandler::HandleLensButtonClick() {
#if !BUILDFLAG(IS_ANDROID)
  if (!IsEmbeddedInSidePanel()) {
    return;
  }

  base::RecordAction(base::UserMetricsAction(
      "ContextualTasks.Composebox.UserAction.LensButtonClicked"));

  if (auto* controller = GetLensSearchController()) {
    if (auto* user_data = GetOrCreateWebContentsUserData()) {
      controller->SetThumbnailCreatedCallback(base::BindRepeating(
          &contextual_tasks::ContextualTasksWebContentsUserData::
              OnLensThumbnailCreated,
          user_data->AsWeakPtr()));
    }
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
void ContextualTasksExtensionHandler::ResetSmartTabSharing(
    ResetSmartTabSharingCallback callback) {
  std::move(callback).Run(false);
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
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  if (!contextual_tasks::CanShareTabContext(profile)) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  auto* user_data = GetOrCreateWebContentsUserData();
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!user_data || !session_handle) {
    std::move(callback).Run(base::unexpected(
        contextual_search::ContextUploadErrorType::kBrowserProcessingError));
    return;
  }

  tabs::TabInterface* const tab = tabs::TabHandle(tab_id).Get();
  content::WebContents* const tab_contents = tab ? tab->GetContents() : nullptr;
  SessionID session_id =
      tab_contents ? sessions::SessionTabHelper::IdForTab(tab_contents)
                   : SessionID::InvalidValue();
  if (session_id.is_valid()) {
    for (const auto& selected_tab : user_data->GetSelectedTabs()) {
      if (selected_tab.tab_id == session_id.id()) {
        user_data->ClearTabContextSnapshotIfMatching(
            selected_tab.context_token);
        DeleteTabToken(session_handle, selected_tab.context_token);
      }
    }
  }

  auto result = contextual_tasks::CaptureAndUploadTabContext(
      tab_id, session_handle, delay_upload,
      /*on_context_uploaded=*/
      base::BindRepeating(
          [](base::WeakPtr<contextual_tasks::ContextualTasksWebContentsUserData>
                 ud) {
            if (ud) {
              if (auto model = ud->GetOrCreateInputStateModel()) {
                model->OnContextChanged();
              }
            }
          },
          user_data->AsWeakPtr()),
      /*browser_window_interface=*/GetBrowserWindowInterface(),
      /*on_snapshot=*/
      base::BindOnce(&ContextualTasksExtensionHandler::OnTabContextSnapshot,
                     weak_ptr_factory_.GetWeakPtr()));
  if (result.has_value() && tab_contents && session_id.is_valid()) {
    user_data->RecordTabIdMapping(tab_id, session_id.id());
    session_handle->AddDelayedTabContext(
        *result, session_id.id(), tab_contents->GetLastCommittedURL(),
        base::UTF16ToUTF8(tab_contents->GetTitle()));
    // Associate the tab with the task as soon as it is attached as context so
    // the task knows which tabs are part of it.
    user_data->AssociateTabWithTask(session_id);
    user_data->UpdateContextLibraryInputState();
  }

  std::move(callback).Run(result);
#else
  std::move(callback).Run(base::unexpected(
      contextual_search::ContextUploadErrorType::kBrowserProcessingError));
#endif
}

void ContextualTasksExtensionHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->DeleteContext(file_token);
  }
}

void ContextualTasksExtensionHandler::DeleteTabContext(int32_t tab_id) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->DeleteTabContext(tab_id);
  }
}

void ContextualTasksExtensionHandler::ClearFiles(
    bool should_block_auto_suggested_tabs) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->ClearFiles();
  }
}
void ContextualTasksExtensionHandler::SubmitQuery(const std::string& query_text,
                                                  uint8_t mouse_button,
                                                  bool alt_key,
                                                  bool ctrl_key,
                                                  bool meta_key,
                                                  bool shift_key,
                                                  bool is_voice_search) {
  DVLOG(1)
      << "ContextualTasksExtensionHandler::SubmitQuery called unexpectedly";
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
  if (contextual_tasks_page_.is_bound()) {
    const size_t size = message.ByteSizeLong();
    std::vector<uint8_t> serialized_message(size);
    message.SerializeToArray(serialized_message.data(), size);
    contextual_tasks_page_->PostAimMessage(std::move(serialized_message));
  }
}

void ContextualTasksExtensionHandler::SendSearchMessageToBoundPage(
    const lens::ClientToSearchMessage& message) {
  if (contextual_tasks_page_.is_bound()) {
    contextual_tasks_page_->PostSearchMessage(mojo_base::ProtoWrapper(message));
  }
}

void ContextualTasksExtensionHandler::OnHandshakeComplete() {
  if (contextual_tasks_page_.is_bound()) {
    contextual_tasks_page_->OnHandshakeComplete();
  }
}

void ContextualTasksExtensionHandler::OnLensCropUpdated(const GURL& data_uri) {
  if (contextual_tasks_page_.is_bound()) {
    contextual_tasks_page_->OnLensCropUpdated(data_uri);
  }
}

BrowserWindowInterface*
ContextualTasksExtensionHandler::GetBrowserWindowInterface() const {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    return user_data->GetBrowserWindowInterface();
  }
  return nullptr;
}

bool ContextualTasksExtensionHandler::IsEmbeddedInSidePanel() const {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  return contextual_tasks::ContextualTasksPanelController::IsWebContentsInPanel(
      web_contents);
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualTasksExtensionHandler::OnTabContextSnapshot(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    user_data->SetTabContextSnapshot(context_token,
                                     std::move(page_content_data));
  }
  if (searchbox_page_) {
    searchbox_page_->OnContextualInputStatusChanged(
        context_token, contextual_search::ContextUploadStatus::kProcessing,
        std::nullopt);
  }
}
#endif

contextual_search::ContextualSearchSessionHandle*
ContextualTasksExtensionHandler::GetOrCreateContextualSessionHandle() {
  contextual_search::ContextualSearchSessionHandle* session = nullptr;
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    session = user_data->GetOrCreateContextualSessionHandle();
  }
  MaybeRefreshTabContextSubscription(session);
  return session;
}

std::optional<base::UnguessableToken>
ContextualTasksExtensionHandler::GetLensOverlayToken() {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    return user_data->GetLensOverlayToken();
  }
  return std::nullopt;
}

#if !BUILDFLAG(IS_ANDROID)
LensSearchController* ContextualTasksExtensionHandler::GetLensSearchController()
    const {
  if (auto* user_data = GetOrCreateWebContentsUserData()) {
    return user_data->GetLensSearchController();
  }
  return nullptr;
}
#endif

void ContextualTasksExtensionHandler::InitializeInputStateModel() {
  input_state_model_ = nullptr;
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
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksExtensionHandler::GetOrCreateInputStateModel() {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return nullptr;
  }
  auto* user_data = GetOrCreateWebContentsUserData();
  if (!user_data) {
    return nullptr;
  }
  auto model = user_data->GetOrCreateInputStateModel(*session_handle);
  if (input_state_model_.get() != model.get()) {
    input_state_model_ = model;
    if (model) {
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

void ContextualTasksExtensionHandler::MaybeRefreshTabContextSubscription(
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  if (subscribed_session_handle_.get() == session_handle) {
    return;
  }
  if (!session_handle) {
    subscribed_session_handle_ = nullptr;
    tab_context_subscription_ = {};
    return;
  }
  subscribed_session_handle_ = session_handle->AsWeakPtr();
  tab_context_subscription_ =
      session_handle->SubscribeTabContext(base::BindRepeating(
          &ContextualTasksExtensionHandler::SendTabContextToExtensionPage,
          base::Unretained(this)));
}

void ContextualTasksExtensionHandler::SendTabContextToExtensionPage(
    const contextual_search::TabContextState& state) {
  if (!contextual_tasks_page_.is_bound()) {
    return;
  }

  std::vector<base::UnguessableToken> uploaded_tokens;
  if (subscribed_session_handle_) {
    uploaded_tokens = subscribed_session_handle_->GetUploadedContextTokens();
  }

  // Combine and deduplicate tabs between restored tabs and attached
  // tabs into a single tab list to send to the frontend extension.
  std::vector<searchbox::mojom::TabInfoPtr> tabs;
  std::vector<int32_t> submitted_tab_ids;
  std::set<GURL> seen_urls;
  std::set<int32_t> seen_tab_ids;

  for (const auto& tab : state.attached) {
    // Session handle guarantees that `tab_id` is present. De-duplicate.
    if (!seen_tab_ids.insert(*tab.tab_id).second) {
      continue;
    }
    // Record the URL so restored duplicates are skipped below. Attached tabs
    // are never skipped, even if two share a URL since the user manually
    // selected them.
    if (tab.url.is_valid()) {
      seen_urls.insert(tab.url);
    }
    tabs.push_back(CreateMojomTab(tab));
    const bool is_submitted =
        !std::ranges::contains(uploaded_tokens, tab.context_token);
    if (is_submitted && tab.tab_id.has_value()) {
      submitted_tab_ids.push_back(*tab.tab_id);
    }
  }

  for (const auto& tab : state.restored) {
    // Tab ID is not guaranteed to be present. Disallow duplicate restored IDs.
    if (!tab.tab_id.has_value() || !seen_tab_ids.insert(*tab.tab_id).second) {
      continue;
    }
    // Allow for empty URLs, but not duplicate restored URLs.
    if (tab.url.is_valid() && !seen_urls.insert(tab.url).second) {
      continue;
    }
    tabs.push_back(CreateMojomTab(tab));
    submitted_tab_ids.push_back(*tab.tab_id);
  }

  contextual_tasks_page_->OnTabContextUpdated(std::move(tabs),
                                              std::move(submitted_tab_ids));
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
