// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/ai_mode_context_library_converter.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_api_frame_id_map.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif

namespace contextual_tasks {

namespace {

constexpr size_t kMaxSearchMessageBytes = 1024 * 1024;
constexpr size_t kMaxPendingSearchMessages = 100;

struct IdentityState {
  bool is_signed_in = false;
  bool browser_identity_matches_aim_identity = false;
};

IdentityState GetIdentityState(content::WebContents* web_contents) {
  if (!web_contents) {
    return {};
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* ui_service = profile
                         ? contextual_tasks::ContextualTasksUiServiceFactory::
                               GetForBrowserContext(profile)
                         : nullptr;
  GURL url = web_contents->GetLastCommittedURL();
  IdentityState state;
  if (ui_service) {
    state.is_signed_in = ui_service->IsSignedInToBrowserWithValidCredentials();
    state.browser_identity_matches_aim_identity =
        state.is_signed_in && ui_service->IsUrlForPrimaryAccount(url);
  } else if (profile && omnibox::kComposeboxDriveIdentityFallback.Get()) {
    if (auto* identity_manager =
            IdentityManagerFactory::GetForProfile(profile)) {
      if (contextual_tasks::IsSignedInToBrowserWithValidCredentials(
              identity_manager)) {
        state.is_signed_in = true;
        state.browser_identity_matches_aim_identity =
            contextual_tasks::IsUrlForPrimaryAccount(identity_manager, url);
      }
    }
  }
  return state;
}

#if !BUILDFLAG(IS_ANDROID)
void RemoveAttachedTabUnderline(
    std::optional<int32_t> session_tab_id,
    const ContextualTasksWebContentsUserData& user_data,
    BrowserWindowInterface* browser_window_interface) {
  if (!session_tab_id.has_value() || !browser_window_interface) {
    return;
  }
  if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
    for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
      if (!tab || !tab->GetContents()) {
        continue;
      }
      if (sessions::SessionTabHelper::IdForTab(tab->GetContents()).id() ==
          *session_tab_id) {
        contextual_tasks::RemoveTabUnderline(tab->GetHandle().raw_value(),
                                             browser_window_interface);
        return;
      }
    }
  }
  if (std::optional<int32_t> tab_handle_id =
          user_data.GetTabHandleForSessionTabId(*session_tab_id)) {
    contextual_tasks::RemoveTabUnderline(*tab_handle_id,
                                         browser_window_interface);
  }
}

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

}  // namespace

ContextualTasksWebContentsUserData::ExtensionFrameInfo::ExtensionFrameInfo() =
    default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo::ExtensionFrameInfo(
    const ExtensionFrameInfo&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo&
ContextualTasksWebContentsUserData::ExtensionFrameInfo::operator=(
    const ExtensionFrameInfo&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo::ExtensionFrameInfo(
    ExtensionFrameInfo&&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo&
ContextualTasksWebContentsUserData::ExtensionFrameInfo::operator=(
    ExtensionFrameInfo&&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo::~ExtensionFrameInfo() =
    default;

struct ContextualTasksWebContentsUserData::PageSearchState
    : public content::PageUserData<
          ContextualTasksWebContentsUserData::PageSearchState> {
  explicit PageSearchState(content::Page& page)
      : content::PageUserData<PageSearchState>(page) {}
  ~PageSearchState() override = default;

  std::string connected_document_id;
  bool is_handshake_complete = false;
  std::vector<std::vector<uint8_t>> pending_search_messages;
  bool is_lens_crop_mounted = false;
  std::string last_lens_crop_data_uri;
  bool context_library_is_active = false;
  ContextState last_sent_context_state = ContextState::kNone;

  PAGE_USER_DATA_KEY_DECL();
};

PAGE_USER_DATA_KEY_IMPL(ContextualTasksWebContentsUserData::PageSearchState);

ContextualTasksWebContentsUserData::ContextualTasksWebContentsUserData(
    content::WebContents* contents)
    : content::WebContentsUserData<ContextualTasksWebContentsUserData>(
          *contents) {}

ContextualTasksWebContentsUserData::~ContextualTasksWebContentsUserData() {
  if (observed_controller_) {
    observed_controller_->RemoveObserver(this);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksWebContentsUserData);

void ContextualTasksWebContentsUserData::set_input_state_model(
    std::unique_ptr<contextual_search::InputStateModel> input_state_model) {
  if (!input_state_model) {
    return;
  }
  if (auto* handle = input_state_model->session_handle()) {
    last_active_model_ = input_state_model->AsWeakPtr();
    input_state_models_[handle->session_id()] = std::move(input_state_model);
    SubscribeToInputStateModel(last_active_model_);
  }
}

// static
void ContextualTasksWebContentsUserData::UpdateInputStateModelIdentity(
    content::WebContents* web_contents,
    contextual_search::InputStateModel* input_state_model) {
  if (!web_contents || !input_state_model) {
    return;
  }
  IdentityState identity_state = GetIdentityState(web_contents);
  input_state_model->SetIdentityState(
      identity_state.is_signed_in,
      identity_state.browser_identity_matches_aim_identity);
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksWebContentsUserData::GetOrCreateInputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle) {
  // Garbage collect models whose session handles have been destroyed
  base::EraseIf(input_state_models_, [](const auto& pair) {
    return !pair.second->session_handle();
  });

  content::WebContents* web_contents = &GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
  const omnibox::SearchboxConfig* config =
      service ? service->GetSearchboxConfig() : nullptr;

  IdentityState identity_state = GetIdentityState(web_contents);

  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  contextual_search::ContextualSearchSessionHandle* web_contents_session =
      helper ? (task_id_.has_value() ? helper->GetSessionForTask(*task_id_)
                                     : helper->session_handle())
             : nullptr;
  const bool is_web_contents_session =
      (&session_handle == web_contents_session);

  auto it = input_state_models_.find(session_handle.session_id());
  if (it != input_state_models_.end()) {
    it->second->SetIdentityState(
        identity_state.is_signed_in,
        identity_state.browser_identity_matches_aim_identity);
    if (config) {
      it->second->UpdateConfig(*config);
    }
    if (is_web_contents_session || !subscribed_model_) {
      last_active_model_ = it->second->AsWeakPtr();
    }
    if (is_web_contents_session) {
      SubscribeToInputStateModel(it->second->AsWeakPtr());
    }
    return it->second->AsWeakPtr();
  }

  GURL url = web_contents->GetLastCommittedURL();
  bool is_off_the_record = profile->IsOffTheRecord();

  auto model = std::make_unique<contextual_search::InputStateModel>(
      session_handle, config ? *config : omnibox::SearchboxConfig(), url,
      is_off_the_record, identity_state.is_signed_in,
      identity_state.browser_identity_matches_aim_identity);
  if (profile) {
    model->SetPrefService(profile->GetPrefs());
  }

  auto model_weak = model->AsWeakPtr();
  if (is_web_contents_session || !subscribed_model_) {
    last_active_model_ = model_weak;
  }
  input_state_models_[session_handle.session_id()] = std::move(model);
  if (is_web_contents_session) {
    SubscribeToInputStateModel(model_weak);
  }
  return model_weak;
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksWebContentsUserData::GetOrCreateInputStateModel() {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return nullptr;
  }
  auto model = GetOrCreateInputStateModel(*session_handle);
  SubscribeToInputStateModel(model);
  return model;
}

void ContextualTasksWebContentsUserData::SubscribeToInputStateModel(
    base::WeakPtr<contextual_search::InputStateModel> model) {
  if (subscribed_model_.get() == model.get()) {
    return;
  }
  subscribed_model_ = model;
  auto& page_state = GetPrimaryPageSearchState();
  page_state.is_lens_crop_mounted = false;
  page_state.last_lens_crop_data_uri.clear();
  if (subscribed_model_) {
    // Safe to use base::Unretained(this) because `input_state_subscription_` is
    // owned by `this` and unsubscribes on destruction.
    input_state_subscription_ =
        subscribed_model_->Subscribe(base::BindRepeating(
            &ContextualTasksWebContentsUserData::OnInputStateChanged,
            base::Unretained(this)));
  } else {
    input_state_subscription_ = {};
  }
}

void ContextualTasksWebContentsUserData::SetTaskId(const base::Uuid& uuid) {
  task_id_ = uuid;
  // Moves to `uuid`'s session. An untasked session and its tabs are adopted by
  // `uuid` (see `GetSessionForTask()`), so do not reset its tabs. The new
  // session will adopt these unassigned tabs.
  GetOrCreateInputStateModel();
}

void ContextualTasksWebContentsUserData::RegisterExtensionFrame(
    const void* handler_id) {
  for (auto& frame : extension_frames_) {
    if (frame.handler_id == handler_id) {
      return;
    }
  }
  ExtensionFrameInfo info;
  info.handler_id = handler_id;
  info.is_page_bound = false;
  extension_frames_.push_back(std::move(info));
}

void ContextualTasksWebContentsUserData::UpdateExtensionFrameBound(
    const void* handler_id,
    bool is_page_bound,
    base::RepeatingCallback<void(const lens::ClientToSearchMessage&)>
        post_search_message_cb,
    base::RepeatingClosure on_handshake_complete_cb,
    base::RepeatingCallback<void(const GURL&)> on_lens_crop_updated_cb) {
  bool had_bound_frame = HasBoundExtensionFrame();
  for (auto& frame : extension_frames_) {
    if (frame.handler_id == handler_id) {
      frame.is_page_bound = is_page_bound;
      frame.post_search_message_cb = std::move(post_search_message_cb);
      frame.on_handshake_complete_cb = std::move(on_handshake_complete_cb);
      frame.on_lens_crop_updated_cb = std::move(on_lens_crop_updated_cb);
      if (is_page_bound && !had_bound_frame &&
          !IsServiceWorkerPortConnected()) {
        auto& page_state = GetPrimaryPageSearchState();
        page_state.is_lens_crop_mounted = false;
        page_state.last_lens_crop_data_uri.clear();
        page_state.last_sent_context_state = ContextState::kNone;
      }
      return;
    }
  }
}

void ContextualTasksWebContentsUserData::UnregisterExtensionFrame(
    const void* handler_id) {
  std::erase_if(extension_frames_, [&](const ExtensionFrameInfo& frame) {
    return frame.handler_id == handler_id;
  });
}

bool ContextualTasksWebContentsUserData::HasBoundExtensionFrame() const {
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound) {
      return true;
    }
  }

  return false;
}

void ContextualTasksWebContentsUserData::OnDocumentConnected(
    content::RenderFrameHost* main_rfh) {
  if (!main_rfh) {
    return;
  }
  auto& page_state = *PageSearchState::GetOrCreateForPage(main_rfh->GetPage());
  std::string doc_id;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  doc_id =
      extensions::ExtensionApiFrameIdMap::GetDocumentId(main_rfh).ToString();
#endif
  if (!page_state.connected_document_id.empty()) {
    if (page_state.connected_document_id != doc_id) {
      page_state.pending_search_messages.clear();
      page_state.last_lens_crop_data_uri.clear();
    }
    page_state.is_lens_crop_mounted = false;
    page_state.context_library_is_active = false;
    page_state.last_sent_context_state = ContextState::kNone;
  }
  page_state.connected_document_id = doc_id;
  page_state.is_handshake_complete = false;
}

bool ContextualTasksWebContentsUserData::IsServiceWorkerPortConnected() const {
  const auto* page_state = GetPrimaryPageSearchState();
  if (!page_state || page_state->connected_document_id.empty()) {
    return false;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content::RenderFrameHost* primary_rfh =
      const_cast<content::WebContents&>(GetWebContents()).GetPrimaryMainFrame();
  if (!primary_rfh) {
    return false;
  }
  return extensions::ExtensionApiFrameIdMap::GetDocumentId(primary_rfh)
             .ToString() == page_state->connected_document_id;
#else
  return true;
#endif
}

bool ContextualTasksWebContentsUserData::IsHandshakeCompleteForTesting() const {
  const auto* page_state = GetPrimaryPageSearchState();
  return page_state && page_state->is_handshake_complete;
}

const std::string&
ContextualTasksWebContentsUserData::GetConnectedDocumentIdForTesting() const {
  const auto* page_state = GetPrimaryPageSearchState();
  return page_state ? page_state->connected_document_id : base::EmptyString();
}

bool ContextualTasksWebContentsUserData::OnSearchMessageReceived(
    base::span<const uint8_t> message) {
  if (message.size() > kMaxSearchMessageBytes) {
    return false;
  }

  lens::SearchToClientMessage search_to_client_message;
  if (!search_to_client_message.ParseFromArray(message.data(),
                                               message.size())) {
    return false;
  }

  switch (search_to_client_message.event_message_case()) {
    case lens::SearchToClientMessage::kHandshakeResponse: {
      int32_t raw_auth_index =
          search_to_client_message.handshake_response().auth_user_index();
      size_t auth_user_index = static_cast<size_t>(std::max(0, raw_auth_index));
      if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
        session_handle->set_auth_user_index(auth_user_index);
      }
      OnHandshakeComplete();
      return true;
    }
    case lens::SearchToClientMessage::kOnSubmitQueryRequest:
      HandleOnSubmitQueryRequest();
      return true;
    case lens::SearchToClientMessage::kOpenLinkInSidePanelMode:
      HandleOpenLinkInSidePanelMode(
          search_to_client_message.open_link_in_side_panel_mode().url());
      return true;
    case lens::SearchToClientMessage::kUpdateThreadContextLibrary:
      HandleThreadContextLibraryUpdateFromAim(
          search_to_client_message.update_thread_context_library());
      return true;
    case lens::SearchToClientMessage::EVENT_MESSAGE_NOT_SET:
      return false;
  }
  return false;
}

void ContextualTasksWebContentsUserData::OnHandshakeComplete() {
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound && frame.on_handshake_complete_cb) {
      frame.on_handshake_complete_cb.Run();
    }
  }
  RecordTimeToHandshakeComplete();
  auto& page_state = GetPrimaryPageSearchState();
  page_state.is_handshake_complete = true;

  const bool had_queued_messages = !page_state.pending_search_messages.empty();
  std::vector<std::vector<uint8_t>> queued =
      std::exchange(page_state.pending_search_messages, {});
  for (const auto& msg_bytes : queued) {
    DispatchSerializedSearchMessage(msg_bytes);
  }

  if (!had_queued_messages && !IsServiceWorkerPortConnected()) {
    page_state.context_library_is_active = false;
    page_state.last_sent_context_state = ContextState::kNone;
  }

  if (!GetSelectedTabs().empty() && !page_state.context_library_is_active) {
    SendMountContextLibrary();
  }
  if (auto model = GetOrCreateInputStateModel()) {
    if (model->lens_crop().has_value() && !page_state.is_lens_crop_mounted) {
      page_state.is_lens_crop_mounted = true;
      SendInjectChromeInput(InjectedInputType::kLensChip, /*is_active=*/true);
    }
  }
  UpdateContextState();
}

void ContextualTasksWebContentsUserData::PostSearchMessage(
    const lens::ClientToSearchMessage& message) {
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> bytes(size);
  message.SerializeToArray(bytes.data(), size);

  auto& page_state = GetPrimaryPageSearchState();
  if (IsServiceWorkerPortConnected()) {
    if (!page_state.is_handshake_complete) {
      if (page_state.pending_search_messages.size() <
          kMaxPendingSearchMessages) {
        page_state.pending_search_messages.push_back(std::move(bytes));
      }
      return;
    }
    DispatchSerializedSearchMessage(bytes);
    return;
  }

  // Fallback when Service Worker Port is not connected: route via the primary
  // bound extension iframe if present.
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound && frame.post_search_message_cb) {
      frame.post_search_message_cb.Run(message);
      if (search_message_dispatcher_for_testing_) {
        search_message_dispatcher_for_testing_.Run(message);
      }
      return;
    }
  }

  // Zero bound extension frames and no Service Worker Port connected yet:
  // queue until handshake completes (or dispatch to testing callback if
  // handshake already completed).
  if (!page_state.is_handshake_complete) {
    if (page_state.pending_search_messages.size() < kMaxPendingSearchMessages) {
      page_state.pending_search_messages.push_back(std::move(bytes));
    }
    return;
  }
  DispatchSerializedSearchMessage(bytes);
}

void ContextualTasksWebContentsUserData::SendInjectChromeInput(
    InjectedInputType type,
    bool is_active) {
  lens::ClientToSearchMessage inject_msg;
  auto* inject_input = inject_msg.mutable_inject_chrome_input();
  switch (type) {
    case InjectedInputType::kContextLibrary:
      inject_input->set_input_type(
          lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
      break;
    case InjectedInputType::kLensChip:
      inject_input->set_input_type(
          lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
      break;
  }
  inject_input->set_is_active(is_active);
  PostSearchMessage(inject_msg);
}

void ContextualTasksWebContentsUserData::SendMountContextLibrary() {
  GetPrimaryPageSearchState().context_library_is_active = true;
  SendInjectChromeInput(InjectedInputType::kContextLibrary,
                        /*is_active=*/true);
}

void ContextualTasksWebContentsUserData::UpdateContextLibraryInputState() {
  bool has_tabs = !GetSelectedTabs().empty();
  if (!has_tabs) {
    pending_context_uploads_.clear();
    failed_context_uploads_.clear();
  }
  auto& page_state = GetPrimaryPageSearchState();
  if (has_tabs != page_state.context_library_is_active) {
    page_state.context_library_is_active = has_tabs;
    SendInjectChromeInput(InjectedInputType::kContextLibrary,
                          /*is_active=*/has_tabs);
  }
  UpdateContextState();
}

void ContextualTasksWebContentsUserData::OnTabContextUploadStarted(
    const base::UnguessableToken& context_token) {
  pending_context_uploads_.insert(context_token);
  failed_context_uploads_.erase(context_token);
}

void ContextualTasksWebContentsUserData::OnTabContextRemoved(
    const base::UnguessableToken& context_token) {
  pending_context_uploads_.erase(context_token);
  failed_context_uploads_.erase(context_token);
}

void ContextualTasksWebContentsUserData::OnContextUploadStatusChanged(
    const base::UnguessableToken& context_token,
    lens::MimeType mime_type,
    contextual_search::ContextUploadStatus context_upload_status,
    const std::optional<contextual_search::ContextUploadErrorType>&
        error_type) {
  if (contextual_search::IsTerminalContextStatus(context_upload_status)) {
    pending_context_uploads_.erase(context_token);
    if (context_upload_status ==
        contextual_search::ContextUploadStatus::kUploadSuccessful) {
      failed_context_uploads_.erase(context_token);
    } else {
      failed_context_uploads_.insert(context_token);
    }
  } else {
    pending_context_uploads_.insert(context_token);
    failed_context_uploads_.erase(context_token);
  }
  UpdateContextState();
}

void ContextualTasksWebContentsUserData::HandleOnSubmitQueryRequest() {
  content::WebContents* web_contents = &GetWebContents();
  // Require a recent user interaction on the page before attaching context.
  if (!web_contents->HasRecentInteraction()) {
    return;
  }

  // Prevent replay by ensuring each submission consumes a new interaction.
  base::TimeTicks last_interaction =
      web_contents->GetLastInteractionTimeTicks();
  if (last_interaction <= last_handled_submit_interaction_time_) {
    return;
  }
  last_handled_submit_interaction_time_ = last_interaction;

  lens::ClientToSearchMessage response_message;
  auto* on_submit_response =
      response_message.mutable_on_submit_query_response();

  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    PostSearchMessage(response_message);
    return;
  }

  UploadSnapshotTabContextIfPresent();

  std::optional<base::UnguessableToken> overlay_token = GetLensOverlayToken();

  if (auto lens_added_context = GetLensAddedContext()) {
    *on_submit_response->add_added_contexts() = std::move(*lens_added_context);
  }

  AppendTabContextsToOnSubmitQueryResponse(&response_message, session_handle,
                                           overlay_token);

  PostSearchMessage(response_message);

  // Submission moved the uploaded tabs into the session's persisted tabs, so
  // the context library chip state may have changed.
  UpdateContextLibraryInputState();

  DoSubmitQueryCleanup();
}

void ContextualTasksWebContentsUserData::HandleOpenLinkInSidePanelMode(
    std::string_view url) {
  GURL target_url(url);
  // Only accept valid URLs that are HTTP or HTTPS.
  if (!target_url.is_valid() || !target_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  content::WebContents* web_contents = &GetWebContents();
  auto* ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!ui_service) {
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  BrowserWindowInterface* browser = GetBrowserWindowInterface();

  base::Uuid task_id = task_id_.value_or(base::Uuid());
  if (!task_id.is_valid()) {
    if (auto* helper =
            ContextualSearchWebContentsHelper::FromWebContents(web_contents);
        helper && helper->task_id().has_value()) {
      task_id = *helper->task_id();
    }
  }

  ui_service->OnThreadLinkClicked(
      target_url, task_id, tab ? tab->GetWeakPtr() : nullptr,
      browser ? browser->GetWeakPtr() : nullptr,
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

void ContextualTasksWebContentsUserData::
    HandleThreadContextLibraryUpdateFromAim(
        const lens::SearchToClientMessage::UpdateThreadContextLibrary&
            message) {
  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksContextLibrary)) {
    return;
  }
  if (!task_id_.has_value()) {
    return;
  }

  auto* service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
  contextual_search::ContextualSearchSessionHandle* session_handle =
      GetOrCreateContextualSessionHandle();
  if (!service || !session_handle) {
    return;
  }

  std::vector<contextual_search::FileInfo> submitted_context =
      session_handle->GetSubmittedContextFileInfos();

  std::vector<contextual_tasks::UrlResource> committed_context =
      contextual_tasks::ConvertAiModeContextToUrlResources(message,
                                                           submitted_context);
  if (committed_context.empty()) {
    return;
  }

  // Save the thread's tabs in the session handle's central
  // restored tabs tracker:
  std::vector<contextual_search::TabInfo> restored_tabs;
  for (const auto& resource : committed_context) {
    if (!resource.has_chrome_tab_data) {
      continue;
    }
    contextual_search::TabInfo tab;
    if (resource.tab_id.has_value()) {
      tab.tab_id = resource.tab_id->id();
    }
    tab.url = resource.url;
    tab.title = resource.title.value_or("");
    tab.submitted = true;

    restored_tabs.push_back(std::move(tab));
  }
  session_handle->SetRestoredTabs(std::move(restored_tabs));

  // The submitted contexts are now part of the task's server-provided
  // context (and the restored tabs above), so the session handle no longer
  // needs to track their tokens.
  session_handle->ClearSubmittedContextTokens();

  // Save the context list on the task in `ContextualTasksService` (outlives the
  // session). This notifies `ActiveTaskContextProvider`, which recomputes tab
  // strip underlines from the task's context (these underlines are in addition
  // to any underlines placed by other handlers).
  service->SetUrlResourcesFromServer(*task_id_, std::move(committed_context));
}

void ContextualTasksWebContentsUserData::OnLensThumbnailCreated(
    const std::string& thumbnail_uri) {
  auto model = GetOrCreateInputStateModel();
  if (!model) {
    return;
  }
  model->SetLensCrop(thumbnail_uri);
}

void ContextualTasksWebContentsUserData::RemoveLensCrop() {
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

void ContextualTasksWebContentsUserData::OnInputStateChanged(
    const omnibox::InputState& state) {
  if (!subscribed_model_) {
    return;
  }

  const auto& crop = subscribed_model_->lens_crop();
  bool has_crop = crop.has_value();

  auto& page_state = GetPrimaryPageSearchState();
  if (has_crop && crop->data_uri != page_state.last_lens_crop_data_uri) {
    page_state.last_lens_crop_data_uri = crop->data_uri;
    for (const auto& frame : extension_frames_) {
      if (frame.is_page_bound && frame.on_lens_crop_updated_cb) {
        frame.on_lens_crop_updated_cb.Run(GURL(crop->data_uri));
      }
    }
  } else if (!has_crop) {
    page_state.last_lens_crop_data_uri.clear();
  }

  if (page_state.is_lens_crop_mounted != has_crop) {
    page_state.is_lens_crop_mounted = has_crop;
    SendInjectChromeInput(InjectedInputType::kLensChip,
                          /*is_active=*/has_crop);
    UpdateContextState();
  }
}

void ContextualTasksWebContentsUserData::ObserveContextController(
    contextual_search::ContextualSearchContextController* controller) {
  if (observed_controller_.get() == controller) {
    return;
  }
  if (observed_controller_) {
    observed_controller_->RemoveObserver(this);
    observed_controller_ = nullptr;
  }
  if (controller) {
    if (auto weak_controller = controller->AsWeakPtr()) {
      controller->AddObserver(this);
      observed_controller_ = std::move(weak_controller);
    }
  }
}

ContextualTasksWebContentsUserData::ContextState
ContextualTasksWebContentsUserData::ComputeContextState() {
  bool any_uploading = false;
  bool any_ready = GetPrimaryPageSearchState().is_lens_crop_mounted;
  for (const auto& tab : GetSelectedTabs()) {
    if (pending_context_uploads_.contains(tab.context_token)) {
      any_uploading = true;
    } else if (!failed_context_uploads_.contains(tab.context_token)) {
      any_ready = true;
    }
  }
  if (any_uploading) {
    return ContextState::kUploading;
  }
  if (any_ready) {
    return ContextState::kReady;
  }
  return ContextState::kNone;
}

void ContextualTasksWebContentsUserData::UpdateContextState() {
  ContextState current_state = ComputeContextState();
  auto& page_state = GetPrimaryPageSearchState();
  if (current_state == page_state.last_sent_context_state) {
    return;
  }
  page_state.last_sent_context_state = current_state;

  lens::ClientToSearchMessage message;
  auto* state_changed = message.mutable_on_context_state_changed();
  switch (current_state) {
    case ContextState::kNone:
      state_changed->set_context_state(
          lens::ClientToSearchMessage::OnContextStateChanged::
              CONTEXT_STATE_NONE);
      break;
    case ContextState::kUploading:
      state_changed->set_context_state(
          lens::ClientToSearchMessage::OnContextStateChanged::
              CONTEXT_STATE_UPLOADING);
      break;
    case ContextState::kReady:
      state_changed->set_context_state(
          lens::ClientToSearchMessage::OnContextStateChanged::
              CONTEXT_STATE_READY);
      break;
  }
  PostSearchMessage(message);
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksWebContentsUserData::GetOrCreateContextualSessionHandle() {
  content::WebContents* web_contents = &GetWebContents();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  contextual_search::ContextualSearchSessionHandle* existing_session =
      task_id_.has_value() ? helper->GetSessionForTask(task_id_.value())
                           : helper->session_handle();
  if (existing_session) {
    ObserveContextController(existing_session->GetController());
    return existing_session;
  }

  if (!task_id_) {
    auto* browser_context = web_contents->GetBrowserContext();
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
      auto* created_session = helper->session_handle();
      if (created_session) {
        ObserveContextController(created_session->GetController());
      }
      return created_session;
    }
  }

  return existing_session;
}

std::optional<base::UnguessableToken>
ContextualTasksWebContentsUserData::GetLensOverlayToken() {
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
LensSearchController*
ContextualTasksWebContentsUserData::GetLensSearchController() const {
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

void ContextualTasksWebContentsUserData::SetTabContextSnapshot(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (tab_context_snapshot_.has_value() &&
      tab_context_snapshot_->first != context_token) {
    OnTabContextRemoved(tab_context_snapshot_->first);
    DeleteTabToken(GetOrCreateContextualSessionHandle(),
                   tab_context_snapshot_->first);
  }
  tab_context_snapshot_.emplace(context_token, std::move(page_content_data));
  pending_context_uploads_.erase(context_token);
  failed_context_uploads_.erase(context_token);
  UpdateContextState();
}

void ContextualTasksWebContentsUserData::ClearTabContextSnapshotIfMatching(
    const base::UnguessableToken& token) {
  if (tab_context_snapshot_.has_value() &&
      tab_context_snapshot_->first == token) {
    tab_context_snapshot_.reset();
  }
}
#endif

void ContextualTasksWebContentsUserData::UploadSnapshotTabContextIfPresent() {
#if !BUILDFLAG(IS_ANDROID)
  if (!tab_context_snapshot_.has_value()) {
    return;
  }
  auto [context_token, page_content_data] =
      std::move(tab_context_snapshot_.value());
  tab_context_snapshot_.reset();

  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_handle->StartTabContextUploadFlow(
        context_token, std::move(page_content_data),
        contextual_tasks::CreateImageEncodingOptions());
  }
#endif
}

void ContextualTasksWebContentsUserData::DoSubmitQueryCleanup() {
  pending_context_uploads_.clear();
  failed_context_uploads_.clear();
  if (auto model = GetOrCreateInputStateModel()) {
    model->RemoveLensCrop();
  }

  CloseLensAsync(
      lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
  UpdateContextState();
}

void ContextualTasksWebContentsUserData::CloseLensAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    controller->CloseLensAsync(dismissal_source);
  }
#endif
}

std::vector<contextual_search::TabInfo>
ContextualTasksWebContentsUserData::GetSelectedTabs() {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return {};
  }
  const auto& attached_tabs = session_handle->GetTabContextState().attached;
  const auto uploaded_tokens = session_handle->GetUploadedContextTokens();
  std::vector<contextual_search::TabInfo> selected_tabs;
  std::ranges::copy_if(attached_tabs, std::back_inserter(selected_tabs),
                       [&](const contextual_search::TabInfo& tab) {
                         return std::ranges::contains(uploaded_tokens,
                                                      tab.context_token);
                       });
  return selected_tabs;
}

void ContextualTasksWebContentsUserData::DeleteContext(
    const base::UnguessableToken& file_token) {
#if !BUILDFLAG(IS_ANDROID)
  ClearTabContextSnapshotIfMatching(file_token);
  for (const auto& selected_tab : GetSelectedTabs()) {
    if (selected_tab.context_token == file_token) {
      RemoveAttachedTabUnderline(selected_tab.tab_id, *this,
                                 GetBrowserWindowInterface());
      break;
    }
  }
#endif
  OnTabContextRemoved(file_token);
  DeleteTabToken(GetOrCreateContextualSessionHandle(), file_token);
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::DeleteTabContext(int32_t tab_id) {
  std::optional<int32_t> session_tab_id;
  if (tabs::TabInterface* const tab = tabs::TabHandle(tab_id).Get();
      tab && tab->GetContents()) {
    SessionID session_id =
        sessions::SessionTabHelper::IdForTab(tab->GetContents());
    if (session_id.is_valid()) {
      session_tab_id = session_id.id();
    }
  }
  if (!session_tab_id.has_value()) {
    session_tab_id = GetSessionTabIdForTabHandle(tab_id);
  }
  if (!session_tab_id.has_value()) {
    return;
  }
  auto selected_tabs = GetSelectedTabs();
  auto it = std::ranges::find_if(
      selected_tabs, [&](const contextual_search::TabInfo& tab_info) {
        return tab_info.tab_id == *session_tab_id;
      });
  if (it == selected_tabs.end()) {
    return;
  }
  base::UnguessableToken token = it->context_token;
#if !BUILDFLAG(IS_ANDROID)
  ClearTabContextSnapshotIfMatching(token);
  contextual_tasks::RemoveTabUnderline(tab_id, GetBrowserWindowInterface());
#endif
  OnTabContextRemoved(token);
  DeleteTabToken(GetOrCreateContextualSessionHandle(), token);
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::ClearFiles() {
#if !BUILDFLAG(IS_ANDROID)
  tab_context_snapshot_.reset();
  auto* browser_window_interface = GetBrowserWindowInterface();
  for (const auto& selected_tab : GetSelectedTabs()) {
    RemoveAttachedTabUnderline(selected_tab.tab_id, *this,
                               browser_window_interface);
  }
#endif
  pending_context_uploads_.clear();
  failed_context_uploads_.clear();
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_handle->ClearFiles();
  }
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::AssociateTabWithTask(
    SessionID tab_session_id) {
  if (!task_id_.has_value() || !tab_session_id.is_valid()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (auto* service =
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              profile)) {
    service->AssociateTabWithTask(*task_id_, tab_session_id);
  }
}

void ContextualTasksWebContentsUserData::RefreshActiveTaskContext() {
  auto* browser_window_interface = GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return;
  }
  if (auto* provider = contextual_tasks::ActiveTaskContextProvider::From(
          browser_window_interface)) {
    provider->RefreshContext();
  }
}

BrowserWindowInterface*
ContextualTasksWebContentsUserData::GetBrowserWindowInterface() const {
  content::WebContents* host_contents =
      &const_cast<content::WebContents&>(GetWebContents());
  // Retrieve the browser window via TabInterface when hosted in a tab, or via
  // WebUI embedding context when hosted in the side panel.
  auto* tab = tabs::TabInterface::MaybeGetFromContents(host_contents);
#if !BUILDFLAG(IS_ANDROID)
  return tab ? tab->GetBrowserWindowInterface()
             : webui::GetBrowserWindowInterface(host_contents);
#else
  return tab ? tab->GetBrowserWindowInterface() : nullptr;
#endif
}

void ContextualTasksWebContentsUserData::RecordTabIdMapping(
    int32_t tab_handle_id,
    int32_t session_tab_id) {
  tab_handle_to_session_id_[tab_handle_id] = session_tab_id;
}

std::optional<int32_t>
ContextualTasksWebContentsUserData::GetSessionTabIdForTabHandle(
    int32_t tab_handle_id) const {
  auto it = tab_handle_to_session_id_.find(tab_handle_id);
  if (it != tab_handle_to_session_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<int32_t>
ContextualTasksWebContentsUserData::GetTabHandleForSessionTabId(
    int32_t session_tab_id) const {
  for (const auto& [tab_handle_id, mapped_session_id] :
       tab_handle_to_session_id_) {
    if (mapped_session_id == session_tab_id) {
      return tab_handle_id;
    }
  }
  return std::nullopt;
}

void ContextualTasksWebContentsUserData::RecordTimeToHandshakeComplete() {
#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* wc = &GetWebContents();
  if (auto* browser = webui::GetBrowserWindowInterface(wc)) {
    if (auto* panel_controller =
            contextual_tasks::ContextualTasksPanelController::From(browser)) {
      panel_controller->RecordTimeToHandshakeComplete(wc);
    }
  }
#endif
}

void ContextualTasksWebContentsUserData::
    AppendTabContextsToOnSubmitQueryResponse(
        lens::ClientToSearchMessage* response_message,
        contextual_search::ContextualSearchSessionHandle* session_handle,
        const std::optional<base::UnguessableToken>& overlay_token) {
  auto* on_submit_response =
      response_message->mutable_on_submit_query_response();

  // Report persisted tabs the user removed since the last turn (deselected,
  // closed, or navigated away) before reading the uploaded tokens, since this
  // also drops tokens for closed tabs. `TakeRemovedContexts()` also carries
  // Smart Tab Sharing removals, but this surface never produces them:
  // `SetSmartTabSharingActive()` is a no-op here because the AIM page is a
  // website and must not be able to trigger tab sharing without a user
  // gesture. If Smart Tab Sharing is enabled for this surface later, that
  // gating belongs in `SetSmartTabSharingActive()` and the recent interaction
  // check in `HandleOnSubmitQueryRequest()`, not here.
  for (const auto& removed_id : session_handle->TakeRemovedContexts()) {
    *on_submit_response->add_removed_contexts()->mutable_request_id() =
        removed_id;
  }

  const auto selected_tabs = GetSelectedTabs();
  // Add uploaded tab contexts directly from the session handle.
  for (const auto& file_info : session_handle->GetUploadedContextFileInfos()) {
    if (!file_info.request_id.has_value()) {
      continue;
    }
    // If this file corresponds to the lens overlay token, it will be handled
    // separately when Lens crop is finalized.
    if (overlay_token.has_value() && file_info.file_token == *overlay_token) {
      continue;
    }
    if (std::ranges::none_of(
            selected_tabs, [&](const contextual_search::TabInfo& selected_tab) {
              return selected_tab.context_token == file_info.file_token;
            })) {
      continue;
    }
    auto* added = on_submit_response->add_added_contexts();
    added->set_search_session_id(session_handle->search_session_id());
    *added->mutable_request_id() = *file_info.request_id;
    if (file_info.input_data && file_info.input_data->upload_type.has_value()) {
      added->set_contextual_input_upload_type(
          *file_info.input_data->upload_type);
    } else {
      added->set_contextual_input_upload_type(
          lens::LensOverlayContextualInputUploadType::
              CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
    }
  }

  // The page owns the query text, so no query length metrics are recorded
  // here. This moves the uploaded tabs into the session's persisted tabs so
  // they are tracked across turns and are not re-added on the next
  // submission.
  session_handle->MarkQuerySubmitted(/*file_tokens=*/{},
                                     /*query_text_length=*/std::nullopt);
}

std::optional<lens::AddedContext>
ContextualTasksWebContentsUserData::GetLensAddedContext() {
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

void ContextualTasksWebContentsUserData::DispatchSerializedSearchMessage(
    const std::vector<uint8_t>& message_bytes) {
  if (search_message_dispatcher_for_testing_) {
    lens::ClientToSearchMessage parsed;
    if (parsed.ParseFromArray(message_bytes.data(), message_bytes.size())) {
      search_message_dispatcher_for_testing_.Run(parsed);
    }
  }

  if (IsServiceWorkerPortConnected()) {
    return;
  }

  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound && frame.post_search_message_cb) {
      lens::ClientToSearchMessage parsed;
      if (parsed.ParseFromArray(message_bytes.data(), message_bytes.size())) {
        frame.post_search_message_cb.Run(parsed);
      }
      return;
    }
  }
}

ContextualTasksWebContentsUserData::PageSearchState&
ContextualTasksWebContentsUserData::GetPrimaryPageSearchState() {
  return *PageSearchState::GetOrCreateForPage(
      GetWebContents().GetPrimaryPage());
}

const ContextualTasksWebContentsUserData::PageSearchState*
ContextualTasksWebContentsUserData::GetPrimaryPageSearchState() const {
  return PageSearchState::GetForPage(
      const_cast<content::WebContents&>(GetWebContents()).GetPrimaryPage());
}

}  // namespace contextual_tasks
